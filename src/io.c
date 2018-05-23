#include <stdio.h>

#include "anf.h"
#include "adt.h"

typedef struct hdr_s hdr_t;
typedef struct blk_s blk_t;

struct hdr_s {
    uint8_t magic[3];
    uint32_t version;
};

struct blk_s {
    uint32_t tag;
    uint32_t skip;
};

enum blk_tag_e {
    BLK_NODES,
    BLK_TYPES,
    BLK_DBG
};

VEC(dbg_vec, const dbg_t*)
HMAP_DEFAULT(dbg2idx,  const dbg_t*,  uint32_t)
HMAP_DEFAULT(node2idx, const node_t*, uint32_t)
HMAP_DEFAULT(type2idx, const type_t*, uint32_t)
HMAP_DEFAULT(idx2dbg,  uint32_t, const dbg_t*)
HMAP_DEFAULT(idx2node, uint32_t, const node_t*)
HMAP_DEFAULT(idx2type, uint32_t, const type_t*)

static size_t locate_block(FILE* fp, uint32_t tag) {
    while (fread(blk, sizeof(blk_t), 1, fp) == 1) {
        if (blk.tag == tag)
            return blk.skip + ftell(fp);
        fseek(fp, blk.skip, SEEK_CUR);
    }
    return -1;
}

static inline long write_dummy_block(FILE* fp) {
    blk_t blk = { .tag = 0, .skip = 0 };
    fwrite(&blk, sizeof(blk_t), 1, fp);
    return ftell(fp);
}

static inline void finalize_block(FILE* fp, long off, uint32_t tag) {
    long cur = ftell(fp);
    blk_t blk = { .tag = tag, .skip = cur - off };
    fseek(fp, off - sizeof(blk_t), SEEK_SET);
    fwrite(&blk, sizeof(blk_t), 1, fp);
    fseek(fp, cur, SEEK_SET);
}

static inline void write_node(FILE* fp, const node_t* node, const node2idx_t* node2idx, const type2idx_t * type2idx, const dbg2idx_t* dbg2idx) {
    fwrite(&node->tag,  sizeof(uint32_t), 1, fp);
    fwrite(&node->nops, sizeof(uint32_t), 1, fp);
    fwrite(&node->box,  sizeof(box_t), 1, fp);
    for (size_t i = 0; i < node->nops; ++i)
        fwrite(node2idx_lookup(node2idx, node->ops[i]), sizeof(uint32_t), 1, fp);
    fwrite(type2idx_lookup(type2idx, node->type), sizeof(uint32_t), 1, fp);
    fwrite(node2idx_lookup(node2idx, node->rep), sizeof(uint32_t), 1, fp);
    uint32_t dbg_idx = node->dbg ? *dbg2idx_lookup(dbg2idx, node->dbg) : UINT32_C(0xFFFFFFFF);
    fwrite(&dbg_idx, sizeof(uint32_t), 1, fp);
}

static inline void write_type(FILE* fp, const type_t* type, const type2idx_t * type2idx) {
    fwrite(&type->tag,  sizeof(uint32_t), 1, fp);
    fwrite(&type->nops, sizeof(uint32_t), 1, fp);
    for (size_t i = 0; i < type->nops; ++i)
        fwrite(type2idx_lookup(type2idx, type->ops[i]), sizeof(uint32_t), 1, fp);
    uint32_t fast = type->fast ? 1 : 0;
    fwrite(&fast, sizeof(uint32_t), 1, fp);
}

static inline const dbg* read_dbg(FILE* fp, mod_t* mod) {
    dbg_t* dbg = mpool_alloc(&mod->mpool, sizeof(dbg_t));

    uint32_t name_len;
    fread(&name_len, sizeof(uint32_t), 1, fp);
    dbg->name = mpool_alloc(&mod->mpool, sizeof(char) * (name_len + 1));
    fwrite(dbg->name, sizeof(char), name_len, fp);
    dbg->name[name_len] = 0;

    uint32_t file_len;
    fread(&file_len, sizeof(uint32_t), 1, fp);
    dbg->file = mpool_alloc(&mod->mpool, sizeof(char) * (file_len + 1));
    fwrite(dbg->file, sizeof(char), file_len, fp);
    dbg->file[file_len] = 0;

    fread(&dbg->brow, sizeof(uint32_t), 1, fp);
    fread(&dbg->bcol, sizeof(uint32_t), 1, fp);
    fread(&dbg->erow, sizeof(uint32_t), 1, fp);
    fread(&dbg->ecol, sizeof(uint32_t), 1, fp);

    return dbg;
}

static inline void write_dbg(FILE* fp, const dbg_t* dbg) {
    uint32_t name_len = strlen(dbg->name);
    uint32_t file_len = strlen(dbg->file);
    fwrite(&name_len, sizeof(uint32_t), 1, fp);
    fwrite(dbg->name, sizeof(char), name_len, fp);
    fwrite(&file_len, sizeof(uint32_t), 1, fp);
    fwrite(dbg->file, sizeof(char), file_len, fp);
    fwrite(&dbg->brow, sizeof(uint32_t), 1, fp);
    fwrite(&dbg->bcol, sizeof(uint32_t), 1, fp);
    fwrite(&dbg->erow, sizeof(uint32_t), 1, fp);
    fwrite(&dbg->ecol, sizeof(uint32_t), 1, fp);
}

bool mod_save(const mod_t* mod, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp)
        return false;

    dbg2idx_t dbg2idx   = dbg2idx_create(64);
    node2idx_t node2idx = node2idx_create(64);
    type2idx_t type2idx = type2idx_create(64);
    dbg_vec_t dbg_vec = dbg_vec_create(64);

    FORALL_HSET(mod->nodes, const node_t*, node, {
        node2idx_insert(&node2idx, node, node2idx.table->nelems);
        if (node->dbg) {
            dbg_vec_push(&dbg_vec, node->dbg);
            dbg2idx_insert(&dbg2idx, node->dbg, dbg2idx.table->nelems);
        }
    })
    FORALL_HSET(mod->types, const type_t*, type, {
        type2idx_insert(&type2idx, type, type2idx.table->nelems);
    })
    
    hdr_t hdr = (hdr_t) {
        .magic = {'A', 'N', 'F'},
        .version = 1,
        .nnodes = node2idx.table->nelems,
        .ntypes = type2idx.table->nelems,
        .ndbg   = dbg2idx.table->nelems
    };

    fwrite(&hdr, sizeof(hdr_t), 1, fp);

    long off;
    uint32_t count;

    off = write_dummy_block(fp);
    count = mod->nodes.table.nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    FORALL_HSET(mod->nodes, const node_t*, node, {
        write_node(fp, node, &node2idx, &type2idx, &dbg2idx);
    })
    finalize_block(fp, off, BLK_NODES);

    off = write_dummy_block(fp);
    count = mod->types.table.nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    FORALL_HSET(mod->types, const type_t*, type, {
        write_type(fp, type, &type2idx);
    })
    finalize_block(fp, off, BLK_TYPES);

    off = write_dummy_block(fp);
    count = dbg_vec.nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    FORALL_VEC(dbg_vec, const dbg_t*, dbg, {
        write_dbg(fp, dbg);
    })
    finalize_block(fp, off, BLK_DBG);

    node2idx_destroy(&node2idx);
    type2idx_destroy(&type2idx);
    dbg2idx_destroy(&dbg2idx);
    dbg_vec_destroy(&dbg_vec);

    fclose(fp);
    return true;
}

bool mod_load(const mod_t* mod, const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return false;

    hdr_t hdr;
    fread(&hdr, sizeof(hdr_t), 1, fp);
    if (hdr.magic[0] != 'A' ||
        hdr.magic[1] != 'N' ||
        hdr.magic[2] != 'F' ||
        hdr.version != 1) {
        fclose(fp);
        return false;
    }

    idx2dbg_t idx2dbg   = idx2dbg_create(64);
    idx2type_t idx2type = idx2type_create(64);
    idx2node_t idx2node = idx2node_create(64);

    uint32_t count;
    bool ok;

    (void)ok;

    ok = locate_block(fp, BLK_DBG);
    assert(ok);

    fread(&count, sizeof(uint32_t), 1, fp);
    for (uint32_t i = 0; i < count; ++i)
        idx2dbg_insert(&idx2dbg, i, read_dbg(fp, mod));

    idx2_node_destroy(&idx2node);
    idx2_type_destroy(&idx2type);
    idx2_dbg_destroy(&idx2dbg2);

    fclose(fp);
    return true;
}
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
    BLK_FNS,
    BLK_NODES,
    BLK_TYPES,
    BLK_DBG
};

enum flags_e {
    FLAG_EXPORTED  = 0x1,
    FLAG_IMPORTED  = 0x2,
    FLAG_INTRINSIC = 0x4,
    FLAG_FAST      = 0x1
};

VEC(dbg_vec, const dbg_t*)
HMAP_DEFAULT(dbg2idx,  const dbg_t*,  uint32_t)
HMAP_DEFAULT(node2idx, const node_t*, uint32_t)
HMAP_DEFAULT(type2idx, const type_t*, uint32_t)
HMAP_DEFAULT(idx2dbg,  uint32_t, const dbg_t*)
HMAP_DEFAULT(idx2node, uint32_t, const node_t*)
HMAP_DEFAULT(idx2type, uint32_t, const type_t*)

static size_t locate_block(FILE* fp, uint32_t tag) {
    blk_t blk;
    fseek(fp, sizeof(hdr_t), SEEK_SET);
    while (fread(&blk, sizeof(blk_t), 1, fp) == 1) {
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

static inline void write_fn(FILE* fp, const fn_t* fn, const node2idx_t* node2idx, const type2idx_t* type2idx, const dbg2idx_t* dbg2idx) {
    fwrite(node2idx_lookup(node2idx, fn->node.ops[0]), sizeof(uint32_t), 1, fp);
    fwrite(node2idx_lookup(node2idx, fn->node.ops[1]), sizeof(uint32_t), 1, fp);
    fwrite(type2idx_lookup(type2idx, fn->node.type), sizeof(uint32_t), 1, fp);
    uint32_t flags =
        (fn->exported  ? FLAG_EXPORTED  : 0) |
        (fn->imported  ? FLAG_IMPORTED  : 0) |
        (fn->intrinsic ? FLAG_INTRINSIC : 0);
    fwrite(&flags, sizeof(uint32_t), 1, fp);
    uint32_t dbg_idx = fn->node.dbg ? *dbg2idx_lookup(dbg2idx, fn->node.dbg) : (uint32_t)-1;
    fwrite(&dbg_idx, sizeof(uint32_t), 1, fp);
}

static inline bool write_node(FILE* fp, const node_t* node, node2idx_t* node2idx, const type2idx_t* type2idx, const dbg2idx_t* dbg2idx) {
    assert(!node->rep);
    if (node2idx_lookup(node2idx, node))
        return true;
    uint32_t ops[node->nops];
    for (size_t i = 0; i < node->nops; ++i) {
        const uint32_t* id = node2idx_lookup(node2idx, node->ops[i]);
        if (!id)
            return false;
        ops[i] = *id;
    }
    node2idx_insert(node2idx, node, node2idx->table->nelems);

    fwrite(&node->tag,  sizeof(uint32_t), 1, fp);
    fwrite(&node->nops, sizeof(uint32_t), 1, fp);
    fwrite(&node->box,  sizeof(box_t),    1, fp);
    fwrite(ops, sizeof(uint32_t), node->nops, fp);
    fwrite(type2idx_lookup(type2idx, node->type), sizeof(uint32_t), 1, fp);
    uint32_t dbg_idx = node->dbg ? *dbg2idx_lookup(dbg2idx, node->dbg) : (uint32_t)-1;
    fwrite(&dbg_idx, sizeof(uint32_t), 1, fp);
    return true;
}

static inline bool write_type(FILE* fp, const type_t* type, type2idx_t* type2idx) {
    if (type2idx_lookup(type2idx, type))
        return true;
    uint32_t ops[type->nops];
    for (size_t i = 0; i < type->nops; ++i) {
        const uint32_t* id = type2idx_lookup(type2idx, type->ops[i]);
        if (!id)
            return false;
        ops[i] = *id;
    }
    type2idx_insert(type2idx, type, type2idx->table->nelems);

    fwrite(&type->tag,  sizeof(uint32_t), 1, fp);
    fwrite(&type->nops, sizeof(uint32_t), 1, fp);
    fwrite(ops, sizeof(uint32_t), type->nops, fp);
    uint32_t flags = type->fast ? FLAG_FAST : 0;
    fwrite(&flags, sizeof(uint32_t), 1, fp);
    return true;
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

static inline const node_t* read_node(FILE* fp, mod_t* mod, const idx2node_t* idx2node, const idx2type_t* idx2type, const idx2dbg_t* idx2dbg) {
    node_t node;
    memset(&node, 0, sizeof(node_t));
    fread(&node.tag, sizeof(uint32_t), 1, fp);
    fread(&node.nops, sizeof(uint32_t), 1, fp);
    fread(&node.box, sizeof(box_t), 1, fp);
    const node_t* ops[node.nops];
    node.ops = ops;
    for (uint32_t i = 0; i < node.nops; ++i) {
        uint32_t id;
        fread(&id, sizeof(uint32_t), 1, fp);
        ops[i] = *idx2node_lookup(idx2node, id);
    }
    uint32_t type_idx;
    fread(&type_idx, sizeof(uint32_t), 1, fp);
    uint32_t dbg_idx;
    fread(&dbg_idx, sizeof(uint32_t), 1, fp);
    node.dbg = idx2dbg && dbg_idx != (uint32_t)-1 ? *idx2dbg_lookup(idx2dbg, dbg_idx) : NULL;
    node.type = *idx2type_lookup(idx2type, type_idx);
    return node_rebuild(mod, &node, ops, node.type);
}

static inline void read_fn_ops(FILE* fp, mod_t* mod, uint32_t i, idx2node_t* idx2node) {
    uint32_t body_idx;
    fread(&body_idx, sizeof(uint32_t), 1, fp);
    uint32_t run_if_idx;
    fread(&run_if_idx, sizeof(uint32_t), 1, fp);
    fn_t* fn = fn_cast(*idx2node_lookup(idx2node,i));
    fn_bind(mod, fn, *idx2node_lookup(idx2node, body_idx));
    fn_run_if(mod, fn, *idx2node_lookup(idx2node, run_if_idx));
    fseek(fp, sizeof(uint32_t) * 3, SEEK_CUR);
}

static inline const fn_t* read_fn(FILE* fp, mod_t* mod, const idx2type_t* idx2type, const idx2dbg_t* idx2dbg) {
    fseek(fp, 2 * sizeof(uint32_t), SEEK_CUR);
    uint32_t type_idx;
    fread(&type_idx, sizeof(uint32_t), 1, fp);
    uint32_t flags;
    fread(&flags, sizeof(uint32_t), 1, fp);
    uint32_t dbg_idx;
    fread(&dbg_idx, sizeof(uint32_t), 1, fp);
    const dbg_t* dbg = idx2dbg && dbg_idx != (uint32_t)-1 ? *idx2dbg_lookup(idx2dbg, dbg_idx) : NULL;
    const type_t* type = *idx2type_lookup(idx2type, type_idx);
    fn_t* fn = node_fn(mod, type, dbg);
    fn->exported  = flags & FLAG_EXPORTED;
    fn->imported  = flags & FLAG_IMPORTED;
    fn->intrinsic = flags & FLAG_INTRINSIC;
    return fn;
}

static inline const type_t* read_type(FILE* fp, mod_t* mod, idx2type_t* idx2type) {
    type_t type;
    memset(&type, 0, sizeof(type_t));
    fread(&type.tag,  sizeof(uint32_t), 1, fp);
    fread(&type.nops, sizeof(uint32_t), 1, fp);
    const type_t* ops[type.nops];
    type.ops = ops;
    for (uint32_t i = 0; i < type.nops; ++i) {
        uint32_t id;
        fread(&id, sizeof(uint32_t), 1, fp);
        ops[i] = *idx2type_lookup(idx2type, id);
    }
    uint32_t flags;
    fread(&flags, sizeof(uint32_t), 1, fp);
    type.fast = flags & FLAG_FAST;
    return type_rebuild(mod, &type, ops);
}

static inline const dbg_t* read_dbg(FILE* fp, mpool_t** dbg_pool) {
    dbg_t* dbg = mpool_alloc(dbg_pool, sizeof(dbg_t));

    uint32_t name_len;
    fread(&name_len, sizeof(uint32_t), 1, fp);
    char* name = mpool_alloc(dbg_pool, sizeof(char) * (name_len + 1));
    fwrite(name, sizeof(char), name_len, fp);
    name[name_len] = 0;

    uint32_t file_len;
    fread(&file_len, sizeof(uint32_t), 1, fp);
    char* file = mpool_alloc(dbg_pool, sizeof(char) * (file_len + 1));
    fwrite(file, sizeof(char), file_len, fp);
    file[file_len] = 0;

    dbg->name = name;
    dbg->file = file;

    fread(&dbg->brow, sizeof(uint32_t), 1, fp);
    fread(&dbg->bcol, sizeof(uint32_t), 1, fp);
    fread(&dbg->erow, sizeof(uint32_t), 1, fp);
    fread(&dbg->ecol, sizeof(uint32_t), 1, fp);

    return dbg;
}

bool mod_save(const mod_t* mod, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp)
        return false;

    dbg2idx_t dbg2idx   = dbg2idx_create(64);
    node2idx_t node2idx = node2idx_create(64);
    type2idx_t type2idx = type2idx_create(64);
    dbg_vec_t dbg_vec = dbg_vec_create(64);

    hdr_t hdr = (hdr_t) {
        .magic = {'A', 'N', 'F'},
        .version = 1,
    };

    fwrite(&hdr, sizeof(hdr_t), 1, fp);

    long off;
    uint32_t count;
    bool todo;

    FORALL_HSET(mod->nodes, const node_t*, node, {
        if (node->dbg && dbg2idx_insert(&dbg2idx, node->dbg, dbg2idx.table->nelems))
            dbg_vec_push(&dbg_vec, node->dbg);
    })
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        if (fn->node.dbg && dbg2idx_insert(&dbg2idx, fn->node.dbg, dbg2idx.table->nelems))
            dbg_vec_push(&dbg_vec, fn->node.dbg);
    })

    // First, write debug info
    off = write_dummy_block(fp);
    count = dbg_vec.nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    FORALL_VEC(dbg_vec, const dbg_t*, dbg, {
        write_dbg(fp, dbg);
    })
    finalize_block(fp, off, BLK_DBG);

    // Then types
    off = write_dummy_block(fp);
    count = mod->types.table->nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    todo = true;
    while (todo) {
        todo = false;
        FORALL_HSET(mod->types, const type_t*, type, {
            todo |= !write_type(fp, type, &type2idx);
        })
    }
    finalize_block(fp, off, BLK_TYPES);

    // Then nodes
    off = write_dummy_block(fp);
    count = mod->nodes.table->nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    // Insert all functions in the map
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        node2idx_insert(&node2idx, &fn->node, node2idx.table->nelems);
    })
    todo = true;
    while (todo) {
        todo = false;
        FORALL_HSET(mod->nodes, const node_t*, node, {
            todo |= !write_node(fp, node, &node2idx, &type2idx, &dbg2idx);
        })
    }
    finalize_block(fp, off, BLK_NODES);

    // Then functions
    off = write_dummy_block(fp);
    count = mod->fns.nelems;
    fwrite(&count, sizeof(uint32_t), 1, fp);
    FORALL_VEC(mod->fns, const fn_t*, fn, {
        write_fn(fp, fn, &node2idx, &type2idx, &dbg2idx);
    })
    finalize_block(fp, off, BLK_FNS);

    node2idx_destroy(&node2idx);
    type2idx_destroy(&type2idx);
    dbg2idx_destroy(&dbg2idx);
    dbg_vec_destroy(&dbg_vec);

    fclose(fp);
    return true;
}

mod_t* mod_load(const char* filename, mpool_t** dbg_pool) {
    FILE* fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

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
    idx2type_t idx2type = idx2type_create(1);
    idx2node_t idx2node = idx2node_create(64);
    mod_t* mod = mod_create();

    uint32_t count;
    bool ok;

    // First, read all debug information (if needed)
    if (dbg_pool) {
        ok = locate_block(fp, BLK_DBG);
        (void)ok;assert(ok);
        fread(&count, sizeof(uint32_t), 1, fp);
        for (uint32_t i = 0; i < count; ++i)
            idx2dbg_insert(&idx2dbg, i, read_dbg(fp, dbg_pool));
    }

    // Then, read all types
    ok = locate_block(fp, BLK_TYPES);
    (void)ok;assert(ok);
    fread(&count, sizeof(uint32_t), 1, fp);
    for (uint32_t i = 0; i < count; ++i)
        idx2type_insert(&idx2type, i, read_type(fp, mod, &idx2type));

    // Then, read functions (but not their body/runif)
    ok = locate_block(fp, BLK_FNS);
    (void)ok;assert(ok);
    fread(&count, sizeof(uint32_t), 1, fp);
    for (uint32_t i = 0; i < count; ++i)
        idx2node_insert(&idx2node, i, &read_fn(fp, mod, &idx2type, dbg_pool ? &idx2dbg : NULL)->node);

    // Then nodes
    ok = locate_block(fp, BLK_NODES);
    (void)ok;assert(ok);
    fread(&count, sizeof(uint32_t), 1, fp);
    for (uint32_t i = 0; i < count; ++i)
        idx2node_insert(&idx2node, mod->fns.nelems + i, read_node(fp, mod, &idx2node, &idx2type, dbg_pool ? &idx2dbg : NULL));

    // Then function bodies
    ok = locate_block(fp, BLK_FNS);
    (void)ok;assert(ok);
    fread(&count, sizeof(uint32_t), 1, fp);
    for (uint32_t i = 0; i < count; ++i)
        read_fn_ops(fp, mod, i, &idx2node);

    idx2node_destroy(&idx2node);
    idx2type_destroy(&idx2type);
    idx2dbg_destroy(&idx2dbg);

    fclose(fp);
    return mod;
}

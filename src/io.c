#include "io.h"
#include "node.h"
#include "type.h"
#include "adt.h"

typedef struct hdr_s hdr_t;
typedef struct blk_s blk_t;

struct hdr_s {
    uint8_t magic[4];
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

VEC(dbg_vec, const dbg_t*)
HMAP_DEFAULT(dbg2idx,  const dbg_t*,  uint32_t)
HMAP_DEFAULT(node2idx, const node_t*, uint32_t)
HMAP_DEFAULT(type2idx, const type_t*, uint32_t)
HMAP_DEFAULT(idx2dbg,  uint32_t, const dbg_t*)
HMAP_DEFAULT(idx2node, uint32_t, const node_t*)
HMAP_DEFAULT(idx2type, uint32_t, const type_t*)

static bool locate_block(io_t* io, uint32_t tag) {
    blk_t blk;
    io->seek(io, sizeof(hdr_t), SEEK_SET);
    while (io->read(io, &blk, sizeof(blk_t)) == sizeof(blk_t)) {
        if (blk.tag == tag)
            return true;
        io->seek(io, blk.skip, SEEK_CUR);
    }
    return false;
}

static inline long write_dummy_block(io_t* io) {
    blk_t blk = { .tag = 0, .skip = 0 };
    io->write(io, &blk, sizeof(blk_t));
    return io->tell(io);
}

static inline void finalize_block(io_t* io, long off, uint32_t tag) {
    long cur = io->tell(io);
    blk_t blk = { .tag = tag, .skip = cur - off };
    io->seek(io, off - sizeof(blk_t), SEEK_SET);
    io->write(io, &blk, sizeof(blk_t));
    io->seek(io, cur, SEEK_SET);
}

static inline void write_fn(io_t* io, const node_t* fn, const node2idx_t* node2idx, const type2idx_t* type2idx, const dbg2idx_t* dbg2idx) {
    io->write(io, node2idx_lookup(node2idx, fn->ops[0]), sizeof(uint32_t));
    io->write(io, node2idx_lookup(node2idx, fn->ops[1]), sizeof(uint32_t));
    io->write(io, type2idx_lookup(type2idx, fn->type), sizeof(uint32_t));
    io->write(io, &fn->data.fn_flags, sizeof(fn_flags_t));
    uint32_t dbg_idx = fn->dbg ? *dbg2idx_lookup(dbg2idx, fn->dbg) : (uint32_t)-1;
    io->write(io, &dbg_idx, sizeof(uint32_t));
}

static inline bool write_node(io_t* io, const node_t* node, node2idx_t* node2idx, const type2idx_t* type2idx, const dbg2idx_t* dbg2idx) {
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

    io->write(io, &node->tag,  sizeof(uint32_t));
    io->write(io, &node->nops, sizeof(uint32_t));
    io->write(io, &node->data, sizeof(node->data));
    io->write(io, ops, sizeof(uint32_t) * node->nops);
    io->write(io, type2idx_lookup(type2idx, node->type), sizeof(uint32_t));
    uint32_t dbg_idx = node->dbg ? *dbg2idx_lookup(dbg2idx, node->dbg) : (uint32_t)-1;
    io->write(io, &dbg_idx, sizeof(uint32_t));
    return true;
}

static inline bool write_type(io_t* io, const type_t* type, type2idx_t* type2idx) {
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

    io->write(io, &type->tag,  sizeof(uint32_t));
    io->write(io, &type->nops, sizeof(uint32_t));
    io->write(io, ops, sizeof(uint32_t) * type->nops);
    io->write(io, &type->data, sizeof(type->data));
    return true;
}

static inline void write_dbg(io_t* io, const dbg_t* dbg) {
    uint32_t name_len = strlen(dbg->name);
    uint32_t file_len = strlen(dbg->file);
    io->write(io, &name_len, sizeof(uint32_t));
    io->write(io, dbg->name, name_len);
    io->write(io, &file_len, sizeof(uint32_t));
    io->write(io, dbg->file, file_len);
    io->write(io, &dbg->loc.brow, sizeof(uint32_t));
    io->write(io, &dbg->loc.bcol, sizeof(uint32_t));
    io->write(io, &dbg->loc.erow, sizeof(uint32_t));
    io->write(io, &dbg->loc.ecol, sizeof(uint32_t));
}

static inline const node_t* read_node(io_t* io, mod_t* mod, const idx2node_t* idx2node, const idx2type_t* idx2type, const idx2dbg_t* idx2dbg) {
    node_t node;
    memset(&node, 0, sizeof(node_t));
    io->read(io, &node.tag, sizeof(uint32_t));
    io->read(io, &node.nops, sizeof(uint32_t));
    io->read(io, &node.data, sizeof(node.data));
    const node_t* ops[node.nops];
    node.ops = ops;
    for (uint32_t i = 0; i < node.nops; ++i) {
        uint32_t id;
        io->read(io, &id, sizeof(uint32_t));
        ops[i] = *idx2node_lookup(idx2node, id);
    }
    uint32_t type_idx;
    io->read(io, &type_idx, sizeof(uint32_t));
    uint32_t dbg_idx;
    io->read(io, &dbg_idx, sizeof(uint32_t));
    node.dbg = idx2dbg && dbg_idx != (uint32_t)-1 ? *idx2dbg_lookup(idx2dbg, dbg_idx) : NULL;
    node.type = *idx2type_lookup(idx2type, type_idx);
    return node_rebuild(mod, &node, ops, node.type);
}

static inline void read_fn_ops(io_t* io, mod_t* mod, uint32_t i, idx2node_t* idx2node) {
    uint32_t body_idx;
    io->read(io, &body_idx, sizeof(uint32_t));
    uint32_t run_if_idx;
    io->read(io, &run_if_idx, sizeof(uint32_t));
    const node_t* fn = *idx2node_lookup(idx2node, i);
    assert(fn->tag == NODE_FN);
    node_bind(mod, fn, 0, *idx2node_lookup(idx2node, body_idx));
    node_bind(mod, fn, 1, *idx2node_lookup(idx2node, run_if_idx));
    io->seek(io, sizeof(uint32_t) * 2 + sizeof(fn_flags_t), SEEK_CUR);
}

static inline const node_t* read_fn(io_t* io, mod_t* mod, const idx2type_t* idx2type, const idx2dbg_t* idx2dbg) {
    io->seek(io, 2 * sizeof(uint32_t), SEEK_CUR);
    uint32_t type_idx;
    fn_flags_t flags;
    io->read(io, &type_idx, sizeof(uint32_t));
    io->read(io, &flags,    sizeof(fn_flags_t));
    uint32_t dbg_idx;
    io->read(io, &dbg_idx,  sizeof(uint32_t));
    const dbg_t* dbg = idx2dbg && dbg_idx != (uint32_t)-1 ? *idx2dbg_lookup(idx2dbg, dbg_idx) : NULL;
    const type_t* type = *idx2type_lookup(idx2type, type_idx);
    return node_fn(mod, type, flags, dbg);
}

static inline const type_t* read_type(io_t* io, mod_t* mod, idx2type_t* idx2type) {
    type_t type;
    memset(&type, 0, sizeof(type_t));
    io->read(io, &type.tag,  sizeof(uint32_t));
    io->read(io, &type.nops, sizeof(uint32_t));
    const type_t* ops[type.nops];
    type.ops = ops;
    for (uint32_t i = 0; i < type.nops; ++i) {
        uint32_t id;
        io->read(io, &id, sizeof(uint32_t));
        ops[i] = *idx2type_lookup(idx2type, id);
    }
    io->read(io, &type.data, sizeof(type.data));
    return type_rebuild(mod, &type, ops);
}

static inline const dbg_t* read_dbg(io_t* io, mpool_t** dbg_pool) {
    dbg_t* dbg = mpool_alloc(dbg_pool, sizeof(dbg_t));

    uint32_t name_len;
    io->read(io, &name_len, sizeof(uint32_t));
    char* name = mpool_alloc(dbg_pool, name_len + 1);
    io->read(io, name, name_len);
    name[name_len] = 0;

    uint32_t file_len;
    io->read(io, &file_len, sizeof(uint32_t));
    char* file = mpool_alloc(dbg_pool, file_len + 1);
    io->read(io, file, file_len);
    file[file_len] = 0;

    dbg->name = name;
    dbg->file = file;

    io->read(io, &dbg->loc.brow, sizeof(uint32_t));
    io->read(io, &dbg->loc.bcol, sizeof(uint32_t));
    io->read(io, &dbg->loc.erow, sizeof(uint32_t));
    io->read(io, &dbg->loc.ecol, sizeof(uint32_t));

    return dbg;
}

bool mod_save(const mod_t* mod, io_t* io) {
    hdr_t hdr = {
        .magic = {'A', 'N', 'F', '0'},
        .version = 1,
    };

    if (io->write(io, &hdr, sizeof(hdr_t)) != sizeof(hdr_t))
        return false;

    dbg2idx_t dbg2idx   = dbg2idx_create();
    node2idx_t node2idx = node2idx_create();
    type2idx_t type2idx = type2idx_create();
    dbg_vec_t dbg_vec = dbg_vec_create();

    long off;
    uint32_t count;
    bool todo;
    bool ret = true;

    FORALL_NODES(mod, node, {
        if (node->dbg && dbg2idx_insert(&dbg2idx, node->dbg, dbg2idx.table->nelems)) {
            node_dump(node);
            dbg_vec_push(&dbg_vec, node->dbg);
        }
    })
    FORALL_FNS(mod, fn, {
        if (fn->dbg && dbg2idx_insert(&dbg2idx, fn->dbg, dbg2idx.table->nelems)) {
            node_dump(fn);
            dbg_vec_push(&dbg_vec, fn->dbg);
        }
    })

    // First, write debug info
    off = write_dummy_block(io);
    count = dbg_vec.nelems;
    if (io->write(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    FORALL_VEC(dbg_vec, const dbg_t*, dbg, {
        write_dbg(io, dbg);
    })
    finalize_block(io, off, BLK_DBG);

    // Then types
    off = write_dummy_block(io);
    count = mod->types.table->nelems;
    if (io->write(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    todo = true;
    while (todo) {
        todo = false;
        FORALL_TYPES(mod, type, {
            todo |= !write_type(io, type, &type2idx);
        })
    }
    finalize_block(io, off, BLK_TYPES);

    // Then nodes
    off = write_dummy_block(io);
    count = mod->nodes.table->nelems;
    if (io->write(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    // Insert all functions in the map
    FORALL_FNS(mod, fn, {
        node2idx_insert(&node2idx, fn, node2idx.table->nelems);
    })
    todo = true;
    while (todo) {
        todo = false;
        FORALL_NODES(mod, node, {
            todo |= !write_node(io, node, &node2idx, &type2idx, &dbg2idx);
        })
    }
    finalize_block(io, off, BLK_NODES);

    // Then functions
    off = write_dummy_block(io);
    count = mod->fns.nelems;
    if (io->write(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    FORALL_FNS(mod, fn, {
        write_fn(io, fn, &node2idx, &type2idx, &dbg2idx);
    })
    finalize_block(io, off, BLK_FNS);

    goto ok;

error:
    ret = false;
ok:
    node2idx_destroy(&node2idx);
    type2idx_destroy(&type2idx);
    dbg2idx_destroy(&dbg2idx);
    dbg_vec_destroy(&dbg_vec);
    return ret;
}

mod_t* mod_load(io_t* io, mpool_t** dbg_pool) {
    hdr_t hdr;
    if (io->read(io, &hdr, sizeof(hdr_t)) != sizeof(hdr_t) ||
        hdr.magic[0] != 'A' ||
        hdr.magic[1] != 'N' ||
        hdr.magic[2] != 'F' ||
        hdr.magic[3] != '0' ||
        hdr.version != 1) {
        return NULL;
    }

    idx2dbg_t idx2dbg   = idx2dbg_create();
    idx2type_t idx2type = idx2type_create();
    idx2node_t idx2node = idx2node_create();
    mod_t* mod = mod_create();

    uint32_t count;

    // First, read all debug information (if needed)
    if (dbg_pool) {
        if (!locate_block(io, BLK_DBG) || io->read(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
            goto error;
        for (uint32_t i = 0; i < count; ++i)
            idx2dbg_insert(&idx2dbg, i, read_dbg(io, dbg_pool));
    }

    // Then, read all types
    if (!locate_block(io, BLK_TYPES) || io->read(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    for (uint32_t i = 0; i < count; ++i)
        idx2type_insert(&idx2type, i, read_type(io, mod, &idx2type));

    // Then, read functions (but not their operands)
    if (!locate_block(io, BLK_FNS) || io->read(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    for (uint32_t i = 0; i < count; ++i)
        idx2node_insert(&idx2node, i, read_fn(io, mod, &idx2type, dbg_pool ? &idx2dbg : NULL));

    // Then nodes
    if (!locate_block(io, BLK_NODES) || io->read(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    for (uint32_t i = 0; i < count; ++i)
        idx2node_insert(&idx2node, mod->fns.nelems + i, read_node(io, mod, &idx2node, &idx2type, dbg_pool ? &idx2dbg : NULL));

    // Then function bodies
    if (!locate_block(io, BLK_FNS) || io->read(io, &count, sizeof(uint32_t)) != sizeof(uint32_t))
        goto error;
    for (uint32_t i = 0; i < count; ++i)
        read_fn_ops(io, mod, i, &idx2node);

    goto ok;

error:
    mod_destroy(mod);
    mod = NULL;

ok:
    idx2node_destroy(&idx2node);
    idx2type_destroy(&idx2type);
    idx2dbg_destroy(&idx2dbg);
    return mod;
}

static size_t file_io_read(io_t* io, void* buf, size_t n) {
    file_io_t* file_io = (file_io_t*)io;
    return fread(buf, 1, n, file_io->fp);
}

static size_t file_io_write(io_t* io, const void* buf, size_t n) {
    file_io_t* file_io = (file_io_t*)io;
    return fwrite(buf, 1, n, file_io->fp);
}

static void file_io_seek(io_t* io, long off, int org) {
    file_io_t* file_io = (file_io_t*)io;
    fseek(file_io->fp, off, org);
}

static long file_io_tell(io_t* io) {
    file_io_t* file_io = (file_io_t*)io;
    return ftell(file_io->fp);
}

file_io_t io_from_file(FILE* fp) {
    return (file_io_t) {
        .io = {
            .read  = file_io_read,
            .write = file_io_write,
            .seek  = file_io_seek,
            .tell  = file_io_tell
        },
        .fp = fp
    };
}

static size_t mem_io_read(io_t* io, void* buf, size_t n) {
    mem_io_t* mem_io = (mem_io_t*)io;
    assert((size_t)mem_io->off <= mem_io->size);
    size_t to_read = mem_io->off + n > mem_io->size ? mem_io->size - mem_io->off : n;
    memcpy(buf, (uint8_t*)mem_io->buf + mem_io->off, to_read);
    mem_io->off += to_read;
    return to_read;
}

static size_t mem_io_write(io_t* io, const void* buf, size_t n) {
    mem_io_t* mem_io = (mem_io_t*)io;
    assert((size_t)mem_io->off <= mem_io->size);
    size_t to_write = mem_io->off + n > mem_io->size ? mem_io->size - mem_io->off : n;
    memcpy((uint8_t*)mem_io->buf + mem_io->off, buf, to_write);
    mem_io->off += to_write;
    return to_write;
}

static void mem_io_seek(io_t* io, long off, int org) {
    mem_io_t* mem_io = (mem_io_t*)io;
    long end = mem_io->size;
    long pos = mem_io->off;
    switch (org) {
        case SEEK_SET: pos = off;       break;
        case SEEK_CUR: pos += off;      break;
        case SEEK_END: pos = end + off; break;
        default:
            assert(false);
            break;
    }
    mem_io->off = pos;
}

static long mem_io_tell(io_t* io) {
    mem_io_t* mem_io = (mem_io_t*)io;
    return mem_io->off;
}

mem_io_t io_from_buffer(void* buf, size_t size) {
    return (mem_io_t) {
        .io = {
            .read  = mem_io_read,
            .write = mem_io_write,
            .seek  = mem_io_seek,
            .tell  = mem_io_tell
        },
        .buf  = buf,
        .size = size,
        .off  = 0
    };
}

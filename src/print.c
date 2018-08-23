#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include "anf.h"

void type_print(const type_t* type, bool colorize) {
    const char* prefix = colorize ? "\33[;34;1m" : "";
    const char* suffix = colorize ? "\33[0m"     : "";
    switch (type->tag) {
        case TYPE_I1:  printf("%si1%s",  prefix, suffix); break;
        case TYPE_I8:  printf("%si8%s",  prefix, suffix); break;
        case TYPE_I16: printf("%si16%s", prefix, suffix); break;
        case TYPE_I32: printf("%si32%s", prefix, suffix); break;
        case TYPE_I64: printf("%si64%s", prefix, suffix); break;
        case TYPE_U8:  printf("%su8%s",  prefix, suffix); break;
        case TYPE_U16: printf("%su16%s", prefix, suffix); break;
        case TYPE_U32: printf("%su32%s", prefix, suffix); break;
        case TYPE_U64: printf("%su64%s", prefix, suffix); break;
        case TYPE_F32: printf("%sf32%s", prefix, suffix); break;
        case TYPE_F64: printf("%sf64%s", prefix, suffix); break;
        case TYPE_MEM: printf("%smem%s", prefix, suffix); break;
        case TYPE_PTR:
            type_print(type->ops[0], colorize);
            printf("*");
            break;
        case TYPE_TUPLE:
            printf("(");
            for (size_t i = 0; i < type->nops; ++i) {
                type_print(type->ops[i], colorize);
                if (i != type->nops - 1)
                    printf(", ");
            }
            printf(")");
            break;
        case TYPE_ARRAY:
            printf("[");
            type_print(type->ops[0], colorize);
            printf("]");
            break;
        case TYPE_FN:
            if (type->ops[0]->tag == TYPE_FN) printf("(");
            type_print(type->ops[0], colorize);
            if (type->ops[0]->tag == TYPE_FN) printf(")");
            printf(" -> ");
            type_print(type->ops[1], colorize);
            break;
        default:
            assert(false);
            break;
    }
}

static inline void node_print_name(const node_t* node, bool colorize) {
    const char* prefix = colorize ? "\33[;33m"   : "";
    const char* suffix = colorize ? "\33[0m"     : "";
    if (node->dbg && strlen(node->dbg->name) > 0)
        printf("<%s : %s%"PRIxPTR"%s>", node->dbg->name, prefix, (uintptr_t)node, suffix);
    else
        printf("<%s%"PRIxPTR"%s>", prefix, (uintptr_t)node, suffix);
}

void node_print(const node_t* node, bool colorize) {
    const char* tprefix = colorize ? "\33[;34;1m" : "";
    const char* nprefix = colorize ? "\33[;36;1m" : "";
    const char* suffix  = colorize ? "\33[0m"     : "";
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_I1:  printf("%si1%s %s", tprefix, suffix, node->box.i1 ? "true" : "false"); break;
            case TYPE_I8:  printf("%si8%s  %"PRIi8,  tprefix, suffix, node->box.i8);  break;
            case TYPE_I16: printf("%si16%s %"PRIi16, tprefix, suffix, node->box.i16); break;
            case TYPE_I32: printf("%si32%s %"PRIi32, tprefix, suffix, node->box.i32); break;
            case TYPE_I64: printf("%si64%s %"PRIi64, tprefix, suffix, node->box.i64); break;
            case TYPE_U8:  printf("%su8%s  %"PRIu8,  tprefix, suffix, node->box.u8);  break;
            case TYPE_U16: printf("%su16%s %"PRIu16, tprefix, suffix, node->box.u16); break;
            case TYPE_U32: printf("%su32%s %"PRIu32, tprefix, suffix, node->box.u32); break;
            case TYPE_U64: printf("%su64%s %"PRIu64, tprefix, suffix, node->box.u64); break;
            case TYPE_F32: printf("%sf32%s %f", tprefix, suffix, node->box.f32); break;
            case TYPE_F64: printf("%sf64%s %g", tprefix, suffix, node->box.f64); break;
            default:
                assert(false);
                break;
        }
    } else {
        if (node->nops > 0) {
            node_print_name(node, colorize);
            printf(" = ");
        }
        type_print(node->type, colorize);
        const char* op = NULL;
        switch (node->tag) {
#define NODE(name, str) case name: op = str; break;
            NODE_LIST(NODE)
#undef NODE
            default:
                assert(false);
                break;
        }
        printf(" %s%s%s", nprefix, op, suffix);
        if (node->nops > 0) {
            printf(" ");
            for (size_t i = 0; i < node->nops; ++i) {
                if (node->ops[i]->nops == 0) {
                    // Print literals and undefs inline
                    node_print(node->ops[i], colorize);
                } else {
                    node_print_name(node->ops[i], colorize);
                }
                if (i != node->nops - 1)
                    printf(", ");
            }
        }
    }
}

void type_dump(const type_t* type) {
    type_print(type, true);
    printf("\n");
}

void node_dump(const node_t* node) {
    node_print(node, true);
    printf("\n");
}

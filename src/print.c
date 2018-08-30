#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include "anf.h"
#include "ast.h"

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

void type_dump(const type_t* type) {
    type_print(type, true);
    printf("\n");
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

void node_dump(const node_t* node) {
    node_print(node, true);
    printf("\n");
}

static inline void print_indent(size_t indent) {
    for (size_t i = 0; i < indent; ++i)
        putc(' ', stdout);
}

static inline void ast_print_list(const ast_list_t* list, size_t indent, bool colorize, const char* sep, bool new_line) {
    while (list) {
        ast_print(list->ast, indent, colorize);
        if (list->next) {
            printf(sep);
            if (new_line) print_indent(indent);
        }
        list = list->next;
    }
}

static inline void ast_print_binop_op(const ast_t* op, int prec, size_t indent, bool colorize) {
    bool needs_parens = op->tag == AST_BINOP && binop_precedence(op->data.binop.tag) > prec;
    if (needs_parens) printf("(");
    ast_print(op, indent, colorize);
    if (needs_parens) printf(")");
}

void ast_print(const ast_t* ast, size_t indent, bool colorize) {
    const char* eprefix = colorize ? "\33[;31;1m" : "";
    const char* kprefix = colorize ? "\33[;34;1m" : "";
    const char* lprefix = colorize ? "\33[;36;1m" : "";
    const char* suffix  = colorize ? "\33[0m"     : "";
    const size_t indent_inc = 4;
    switch (ast->tag) {
        case AST_ID:  printf("%s", ast->data.id.str);                       break;
        case AST_LIT: printf("%s%s%s", lprefix, ast->data.lit.str, suffix); break;
        case AST_MOD:
            printf("%smod%s %s {\n", kprefix, suffix, ast->data.mod.id->data.id.str);
            indent += indent_inc;
            print_indent(indent);
            ast_print_list(ast->data.mod.decls, indent, colorize, "\n", true);
            indent -= indent_inc;
            printf("\n");
            print_indent(indent);
            printf("}");
            break;
        case AST_DEF:
            printf("%sdef%s %s", kprefix, suffix, ast->data.mod.id->data.id.str);
            if (ast->data.def.param) {
                ast_print(ast->data.def.param, indent, colorize);
                printf(" ");
            } else
                printf(" = ");
            ast_print(ast->data.def.value, indent, colorize);
            break;
        case AST_VAR:
        case AST_VAL:
            printf(ast->tag == AST_VAR ? "%svar%s " : "%sval%s ", kprefix, suffix);
            ast_print(ast->data.varl.ptrn, indent, colorize);
            printf(" = ");
            ast_print(ast->data.varl.value, indent, colorize);
            break;
        case AST_BLOCK:
            printf("{\n");
            indent += indent_inc;
            print_indent(indent);
            ast_print_list(ast->data.block.stmts, indent, colorize, "\n", true);
            indent -= indent_inc;
            printf("\n");
            print_indent(indent);
            printf("}");
            break;
        case AST_TUPLE:
            printf("(");
            ast_print_list(ast->data.tuple.args, indent, colorize, ", ", false);
            printf(")");
            break;
        case AST_BINOP:
            {
                int prec = binop_precedence(ast->data.binop.tag);
                ast_print_binop_op(ast->data.binop.left, prec, indent, colorize);
                printf(" %s ", binop_symbol(ast->data.binop.tag));
                ast_print_binop_op(ast->data.binop.right, prec, indent, colorize);
            }
            break;
        case AST_UNOP:
            {
                bool prefix = unop_is_prefix(ast->data.unop.tag);
                const char* symbol = unop_symbol(ast->data.unop.tag);
                if (prefix) printf("%s", symbol);
                ast_print(ast->data.unop.op, indent, colorize);
                if (!prefix) printf("%s", symbol);
            }
            break;
        case AST_ERR:
            printf("%s<syntax error>%s", eprefix, suffix);
            break;
        default:
            assert(false);
            break;
    }
}

void ast_dump(const ast_t* ast) {
    ast_print(ast, 0, true);
    printf("\n");
}

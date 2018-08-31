#include <inttypes.h>
#include <stdio.h>
#include <assert.h>

#include "anf.h"
#include "ast.h"
#include "util.h"

void type_print(const type_t* type, bool colorize) {
    switch (type->tag) {
        case TYPE_I1:  printf(COLORIZE(colorize, COLOR_KEY("i1" ))); break;
        case TYPE_I8:  printf(COLORIZE(colorize, COLOR_KEY("i8" ))); break;
        case TYPE_I16: printf(COLORIZE(colorize, COLOR_KEY("i16"))); break;
        case TYPE_I32: printf(COLORIZE(colorize, COLOR_KEY("i32"))); break;
        case TYPE_I64: printf(COLORIZE(colorize, COLOR_KEY("i64"))); break;
        case TYPE_U8:  printf(COLORIZE(colorize, COLOR_KEY("u8" ))); break;
        case TYPE_U16: printf(COLORIZE(colorize, COLOR_KEY("u16"))); break;
        case TYPE_U32: printf(COLORIZE(colorize, COLOR_KEY("u32"))); break;
        case TYPE_U64: printf(COLORIZE(colorize, COLOR_KEY("u64"))); break;
        case TYPE_F32: printf(COLORIZE(colorize, COLOR_KEY("f32"))); break;
        case TYPE_F64: printf(COLORIZE(colorize, COLOR_KEY("f64"))); break;
        case TYPE_MEM: printf(COLORIZE(colorize, COLOR_KEY("mem"))); break;
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
    if (node->dbg && strlen(node->dbg->name) > 0)
        printf(COLORIZE(colorize, "<%s : ", COLOR_ID("%"PRIxPTR), ">"), node->dbg->name, (uintptr_t)node);
    else
        printf(COLORIZE(colorize, "<", COLOR_ID("%"PRIxPTR), ">"), (uintptr_t)node);
}

void node_print(const node_t* node, bool colorize) {
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_I1:  printf(COLORIZE(colorize, COLOR_KEY("i1" ), " ", COLOR_LIT("%s")),      node->box.i1 ? "true" : "false"); break;
            case TYPE_I8:  printf(COLORIZE(colorize, COLOR_KEY("i8" ), " ", COLOR_LIT("%"PRIi8)),  node->box.i8);  break;
            case TYPE_I16: printf(COLORIZE(colorize, COLOR_KEY("i16"), " ", COLOR_LIT("%"PRIi16)), node->box.i16); break;
            case TYPE_I32: printf(COLORIZE(colorize, COLOR_KEY("i32"), " ", COLOR_LIT("%"PRIi32)), node->box.i32); break;
            case TYPE_I64: printf(COLORIZE(colorize, COLOR_KEY("i64"), " ", COLOR_LIT("%"PRIi64)), node->box.i64); break;
            case TYPE_U8:  printf(COLORIZE(colorize, COLOR_KEY("u8" ), " ", COLOR_LIT("%"PRIu8)),  node->box.u8);  break;
            case TYPE_U16: printf(COLORIZE(colorize, COLOR_KEY("u16"), " ", COLOR_LIT("%"PRIu16)), node->box.u16); break;
            case TYPE_U32: printf(COLORIZE(colorize, COLOR_KEY("u32"), " ", COLOR_LIT("%"PRIu32)), node->box.u32); break;
            case TYPE_U64: printf(COLORIZE(colorize, COLOR_KEY("u64"), " ", COLOR_LIT("%"PRIu64)), node->box.u64); break;
            case TYPE_F32: printf(COLORIZE(colorize, COLOR_KEY("f32"), " ", COLOR_LIT("%f")),      node->box.f32); break;
            case TYPE_F64: printf(COLORIZE(colorize, COLOR_KEY("f64"), " ", COLOR_LIT("%g")),      node->box.f64); break;
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
        printf(COLORIZE(colorize, " ", COLOR_KEY("%s")), op);
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
    const size_t indent_inc = 4;
    switch (ast->tag) {
        case AST_ID:  printf("%s", ast->data.id.str); break;
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_FLT:
                case LIT_INT:
                    printf(COLORIZE(colorize, COLOR_LIT("%s")), ast->data.lit.str);
                    break;
                case LIT_STR:  printf(COLORIZE(colorize, COLOR_LIT("\"%s\"")), ast->data.lit.str); break;
                case LIT_CHR:  printf(COLORIZE(colorize, COLOR_LIT("\'%s\'")), ast->data.lit.str); break;
                case LIT_BOOL: printf(COLORIZE(colorize, COLOR_LIT("%s")), ast->data.lit.value.bval ? "true" : "false"); break;
            }
            break;
        case AST_MOD:
            printf(COLORIZE(colorize, COLOR_KEY("mod"), " %s {\n"), ast->data.mod.id->data.id.str);
            indent += indent_inc;
            print_indent(indent);
            ast_print_list(ast->data.mod.decls, indent, colorize, "\n", true);
            indent -= indent_inc;
            printf("\n");
            print_indent(indent);
            printf("}");
            break;
        case AST_DEF:
            printf(COLORIZE(colorize, COLOR_KEY("def"), " %s"), ast->data.mod.id->data.id.str);
            if (ast->data.def.param) {
                ast_print(ast->data.def.param, indent, colorize);
                printf(" ");
            } else
                printf(" = ");
            ast_print(ast->data.def.value, indent, colorize);
            break;
        case AST_VAR:
        case AST_VAL:
            printf(COLORIZE(colorize, COLOR_KEY("%s"), " "), ast->tag == AST_VAR ? "var" : "val");
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
            printf(COLORIZE(colorize, COLOR_ERR("<syntax error>")));
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

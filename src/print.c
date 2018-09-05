#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "print.h"

static void file_printer_printf(printer_t* printer, const char* fmt, ...) {
    file_printer_t* file_printer = (file_printer_t*)printer;
    va_list args;
    va_start(args, fmt);
    vfprintf(file_printer->fp, fmt, args);
    va_end(args);
}

file_printer_t printer_from_file(FILE* fp, bool colorize, size_t indent) {
    return (file_printer_t) {
        .printer = (printer_t) {
            .printf   = file_printer_printf,
            .colorize = colorize,
            .indent   = indent
        },
        .fp = fp
    };
}

void type_print(const type_t* type, printer_t* printer) {
    bool colorize = printer->colorize;
    switch (type->tag) {
        case TYPE_I1:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("i1" ))); break;
        case TYPE_I8:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("i8" ))); break;
        case TYPE_I16: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i16"))); break;
        case TYPE_I32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i32"))); break;
        case TYPE_I64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i64"))); break;
        case TYPE_U8:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("u8" ))); break;
        case TYPE_U16: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u16"))); break;
        case TYPE_U32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u32"))); break;
        case TYPE_U64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u64"))); break;
        case TYPE_F32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("f32"))); break;
        case TYPE_F64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("f64"))); break;
        case TYPE_MEM: pprintf(printer, COLORIZE(colorize, COLOR_KEY("mem"))); break;
        case TYPE_PTR:
            type_print(type->ops[0], printer);
            pprintf(printer, "*");
            break;
        case TYPE_TUPLE:
            pprintf(printer, "(");
            for (size_t i = 0; i < type->nops; ++i) {
                type_print(type->ops[i], printer);
                if (i != type->nops - 1)
                    pprintf(printer, ", ");
            }
            pprintf(printer, ")");
            break;
        case TYPE_ARRAY:
            pprintf(printer, "[");
            type_print(type->ops[0], printer);
            pprintf(printer, "]");
            break;
        case TYPE_FN:
            if (type->ops[0]->tag == TYPE_FN) pprintf(printer, "(");
            type_print(type->ops[0], printer);
            if (type->ops[0]->tag == TYPE_FN) pprintf(printer, ")");
            pprintf(printer, " -> ");
            type_print(type->ops[1], printer);
            break;
        default:
            assert(false);
            break;
    }
}

void type_dump(const type_t* type) {
    file_printer_t file_printer = printer_from_file(stdout, true, 0);
    type_print(type, &file_printer.printer);
    pprintf(&file_printer.printer, "\n");
}

static inline void node_print_name(const node_t* node, printer_t* printer) {
    if (node->dbg && strlen(node->dbg->name) > 0)
        pprintf(printer, COLORIZE(printer->colorize, "<%s : ", COLOR_ID("%"PRIxPTR), ">"), node->dbg->name, (uintptr_t)node);
    else
        pprintf(printer, COLORIZE(printer->colorize, "<", COLOR_ID("%"PRIxPTR), ">"), (uintptr_t)node);
}

void node_print(const node_t* node, printer_t* printer) {
    bool colorize = printer->colorize;
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_I1:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("i1" ), " ", COLOR_LIT("%s")),      node->box.i1 ? "true" : "false"); break;
            case TYPE_I8:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("i8" ), " ", COLOR_LIT("%"PRIi8)),  node->box.i8);  break;
            case TYPE_I16: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i16"), " ", COLOR_LIT("%"PRIi16)), node->box.i16); break;
            case TYPE_I32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i32"), " ", COLOR_LIT("%"PRIi32)), node->box.i32); break;
            case TYPE_I64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("i64"), " ", COLOR_LIT("%"PRIi64)), node->box.i64); break;
            case TYPE_U8:  pprintf(printer, COLORIZE(colorize, COLOR_KEY("u8" ), " ", COLOR_LIT("%"PRIu8)),  node->box.u8);  break;
            case TYPE_U16: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u16"), " ", COLOR_LIT("%"PRIu16)), node->box.u16); break;
            case TYPE_U32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u32"), " ", COLOR_LIT("%"PRIu32)), node->box.u32); break;
            case TYPE_U64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("u64"), " ", COLOR_LIT("%"PRIu64)), node->box.u64); break;
            case TYPE_F32: pprintf(printer, COLORIZE(colorize, COLOR_KEY("f32"), " ", COLOR_LIT("%f")),      node->box.f32); break;
            case TYPE_F64: pprintf(printer, COLORIZE(colorize, COLOR_KEY("f64"), " ", COLOR_LIT("%g")),      node->box.f64); break;
            default:
                assert(false);
                break;
        }
    } else {
        if (node->nops > 0) {
            node_print_name(node, printer);
            pprintf(printer, " = ");
        }
        type_print(node->type, printer);
        const char* op = NULL;
        switch (node->tag) {
#define NODE(name, str) case name: op = str; break;
            NODE_LIST(NODE)
#undef NODE
            default:
                assert(false);
                break;
        }
        pprintf(printer, COLORIZE(colorize, " ", COLOR_KEY("%s")), op);
        if (node->nops > 0) {
            pprintf(printer, " ");
            for (size_t i = 0; i < node->nops; ++i) {
                if (node->ops[i]->nops == 0) {
                    // Print literals and undefs inline
                    node_print(node->ops[i], printer);
                } else {
                    node_print_name(node->ops[i], printer);
                }
                if (i != node->nops - 1)
                    pprintf(printer, ", ");
            }
        }
    }
}

void node_dump(const node_t* node) {
    file_printer_t file_printer = printer_from_file(stdout, true, 0);
    node_print(node, &file_printer.printer);
    pprintf(&file_printer.printer, "\n");
}

static inline void print_indent(printer_t* printer) {
    for (size_t i = 0; i < printer->indent; ++i)
        pprintf(printer, "    ");
}

static inline void ast_print_list(const ast_list_t* list, printer_t* printer, const char* sep, bool new_line) {
    while (list) {
        ast_print(list->ast, printer);
        if (list->next) {
            pprintf(printer, sep);
            if (new_line) print_indent(printer);
        }
        list = list->next;
    }
}

static inline void ast_print_binop_op(const ast_t* op, printer_t* printer, int prec) {
    bool needs_parens = op->tag == AST_BINOP && binop_precedence(op->data.binop.tag) > prec;
    if (needs_parens) pprintf(printer, "(");
    ast_print(op, printer);
    if (needs_parens) pprintf(printer, ")");
}

void ast_print(const ast_t* ast, printer_t* printer) {
    bool colorize = printer->colorize;
    switch (ast->tag) {
        case AST_ID:  pprintf(printer, "%s", ast->data.id.str); break;
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_FLT:
                case LIT_INT:
                    pprintf(printer, COLORIZE(colorize, COLOR_LIT("%s")), ast->data.lit.str);
                    break;
                case LIT_STR:  pprintf(printer, COLORIZE(colorize, COLOR_LIT("\"%s\"")), ast->data.lit.str); break;
                case LIT_CHR:  pprintf(printer, COLORIZE(colorize, COLOR_LIT("\'%s\'")), ast->data.lit.str); break;
                case LIT_BOOL: pprintf(printer, COLORIZE(colorize, COLOR_LIT("%s")), ast->data.lit.value.bval ? "true" : "false"); break;
            }
            break;
        case AST_MOD:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("mod"), " %s {\n"), ast->data.mod.id->data.id.str);
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.mod.decls, printer, "\n", true);
            printer->indent--;
            pprintf(printer, "\n");
            print_indent(printer);
            pprintf(printer, "}");
            break;
        case AST_DEF:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("def"), " %s"), ast->data.mod.id->data.id.str);
            if (ast->data.def.param) {
                ast_print(ast->data.def.param, printer);
                pprintf(printer, " ");
            } else
                pprintf(printer, " = ");
            ast_print(ast->data.def.value, printer);
            break;
        case AST_VAR:
        case AST_VAL:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("%s"), " "), ast->tag == AST_VAR ? "var" : "val");
            ast_print(ast->data.varl.ptrn, printer);
            pprintf(printer, " = ");
            ast_print(ast->data.varl.value, printer);
            break;
        case AST_BLOCK:
            pprintf(printer, "{\n");
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.block.stmts, printer, "\n", true);
            printer->indent--;
            pprintf(printer, "\n");
            print_indent(printer);
            pprintf(printer, "}");
            break;
        case AST_TUPLE:
            pprintf(printer, "(");
            ast_print_list(ast->data.tuple.args, printer, ", ", false);
            pprintf(printer, ")");
            break;
        case AST_BINOP:
            {
                int prec = binop_precedence(ast->data.binop.tag);
                ast_print_binop_op(ast->data.binop.left, printer, prec);
                pprintf(printer, " %s ", binop_symbol(ast->data.binop.tag));
                ast_print_binop_op(ast->data.binop.right, printer, prec);
            }
            break;
        case AST_UNOP:
            {
                bool prefix = unop_is_prefix(ast->data.unop.tag);
                const char* symbol = unop_symbol(ast->data.unop.tag);
                if (prefix) pprintf(printer, "%s", symbol);
                ast_print(ast->data.unop.op, printer);
                if (!prefix) pprintf(printer, "%s", symbol);
            }
            break;
        case AST_LAMBDA:
            ast_print(ast->data.lambda.param, printer);
            pprintf(printer, " => ");
            ast_print(ast->data.lambda.body, printer);
            break;
        case AST_CALL:
            ast_print(ast->data.call.callee, printer);
            ast_print(ast->data.call.arg, printer);
            break;
        case AST_IF:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("if"), " "));
            ast_print(ast->data.if_.cond, printer);
            pprintf(printer, " ");
            ast_print(ast->data.if_.if_true, printer);
            if (ast->data.if_.if_false) {
                pprintf(printer, COLORIZE(colorize, " ", COLOR_KEY("else"), " "));
                ast_print(ast->data.if_.if_false, printer);
            }
            break;
        case AST_WHILE:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("while"), " "));
            ast_print(ast->data.while_.cond, printer);
            pprintf(printer, " ");
            ast_print(ast->data.while_.body, printer);
            break;
        case AST_FOR:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("for"), " ("));
            ast_print(ast->data.for_.vars, printer);
            pprintf(printer, " <- ");
            ast_print(ast->data.for_.expr, printer);
            pprintf(printer, ") ");
            ast_print(ast->data.for_.body, printer);
            break;
        case AST_MATCH:
            ast_print(ast->data.match.expr, printer);
            pprintf(printer, COLORIZE(colorize, " ", COLOR_KEY("match"), " {\n"));
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.mod.decls, printer, "\n", true);
            printer->indent--;
            pprintf(printer, "\n");
            print_indent(printer);
            pprintf(printer, "}");
            break;
        case AST_CASE:
            pprintf(printer, COLORIZE(colorize, COLOR_KEY("case"), " "));
            ast_print(ast->data.case_.ptrn, printer);
            pprintf(printer, " => ");
            ast_print(ast->data.case_.value, printer);
            break;
        case AST_BREAK:    pprintf(printer, COLORIZE(colorize, COLOR_KEY("break")));    break;
        case AST_CONTINUE: pprintf(printer, COLORIZE(colorize, COLOR_KEY("continue"))); break;
        case AST_RETURN:   pprintf(printer, COLORIZE(colorize, COLOR_KEY("return")));   break;
        case AST_ERR:
            pprintf(printer, COLORIZE(colorize, COLOR_ERR("<syntax error>")));
            break;
        default:
            assert(false);
            break;
    }
}

void ast_dump(const ast_t* ast) {
    file_printer_t file_printer = printer_from_file(stdout, true, 0);
    ast_print(ast, &file_printer.printer);
    pprintf(&file_printer.printer, "\n");
}

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "print.h"

static void file_printer_format(printer_t* printer, const char* fmt, const fmt_arg_t* args) {
    format_to_file(((file_printer_t*)printer)->fp, fmt, args, printer->colorize ? FMT_COLORIZE : 0);
}

file_printer_t printer_from_file(FILE* fp, bool colorize, const char* tab, size_t indent) {
    return (file_printer_t) {
        .printer = (printer_t) {
            .format   = file_printer_format,
            .colorize = colorize,
            .tab      = tab,
            .indent   = indent
        },
        .fp = fp
    };
}

void type_print(const type_t* type, printer_t* printer) {
    switch (type->tag) {
        case TYPE_I1:  print(printer, "{$key}i1{$}");  break;
        case TYPE_I8:  print(printer, "{$key}i8{$}");  break;
        case TYPE_I16: print(printer, "{$key}i16{$}"); break;
        case TYPE_I32: print(printer, "{$key}i32{$}"); break;
        case TYPE_I64: print(printer, "{$key}i64{$}"); break;
        case TYPE_U8:  print(printer, "{$key}u8{$}");  break;
        case TYPE_U16: print(printer, "{$key}u16{$}"); break;
        case TYPE_U32: print(printer, "{$key}u32{$}"); break;
        case TYPE_U64: print(printer, "{$key}u64{$}"); break;
        case TYPE_F32: print(printer, "{$key}f32{$}"); break;
        case TYPE_F64: print(printer, "{$key}f64{$}"); break;
        case TYPE_MEM: print(printer, "{$key}mem{$}"); break;
        case TYPE_PTR:
            type_print(type->ops[0], printer);
            print(printer, "*");
            break;
        case TYPE_TUPLE:
            print(printer, "(");
            for (size_t i = 0; i < type->nops; ++i) {
                type_print(type->ops[i], printer);
                if (i != type->nops - 1)
                    print(printer, ", ");
            }
            print(printer, ")");
            break;
        case TYPE_ARRAY:
            print(printer, "[");
            type_print(type->ops[0], printer);
            print(printer, "]");
            break;
        case TYPE_FN:
            if (type->ops[0]->tag == TYPE_FN) print(printer, "(");
            type_print(type->ops[0], printer);
            if (type->ops[0]->tag == TYPE_FN) print(printer, ")");
            print(printer, " -> ");
            type_print(type->ops[1], printer);
            break;
        default:
            assert(false);
            break;
    }
}

void type_dump(const type_t* type) {
    file_printer_t file_printer = printer_from_file(stdout, true, "    ", 0);
    type_print(type, &file_printer.printer);
    print(&file_printer.printer, "\n");
}

static inline void node_print_name(const node_t* node, printer_t* printer) {
    print(printer, "<{0:s}{1:s}{$id}{2:p}{$}>",
        { .str = node->dbg ? node->dbg->name : "" },
        { .str = node->dbg ? " : " : "" },
        { .ptr = node });
}

void node_print(const node_t* node, printer_t* printer) {
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_I1:  print(printer, "{$key}i1{$} {$lit}{0:i1}{$}",   { .b   = node->box.i1  }); break;
            case TYPE_I8:  print(printer, "{$key}i8{$} {$lit}{0:i8}{$}",   { .i8  = node->box.i8  }); break;
            case TYPE_I16: print(printer, "{$key}i16{$} {$lit}{0:i16}{$}", { .i16 = node->box.i16 }); break;
            case TYPE_I32: print(printer, "{$key}i32{$} {$lit}{0:i32}{$}", { .i32 = node->box.i32 }); break;
            case TYPE_I64: print(printer, "{$key}i64{$} {$lit}{0:i64}{$}", { .i64 = node->box.i64 }); break;
            case TYPE_U8:  print(printer, "{$key}u8{$} {$lit}{0:u8}{$}",   { .u8  = node->box.u8  }); break;
            case TYPE_U16: print(printer, "{$key}u16{$} {$lit}{0:u16}{$}", { .i16 = node->box.u16 }); break;
            case TYPE_U32: print(printer, "{$key}u32{$} {$lit}{0:u32}{$}", { .i32 = node->box.u32 }); break;
            case TYPE_U64: print(printer, "{$key}u64{$} {$lit}{0:u64}{$}", { .i64 = node->box.u64 }); break;
            case TYPE_F32: print(printer, "{$key}f32{$} {$lit}{0:f32}{$}", { .f32 = node->box.f32 }); break;
            case TYPE_F64: print(printer, "{$key}f64{$} {$lit}{0:f64}{$}", { .f64 = node->box.f64 }); break;
            default:
                assert(false);
                break;
        }
    } else {
        if (node->nops > 0) {
            node_print_name(node, printer);
            print(printer, " = ");
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
        print(printer, " {$key}{0:s}{$}", {.str = op });
        if (node->nops > 0) {
            print(printer, " ");
            for (size_t i = 0; i < node->nops; ++i) {
                if (node->ops[i]->nops == 0) {
                    // Print literals and undefs inline
                    node_print(node->ops[i], printer);
                } else {
                    node_print_name(node->ops[i], printer);
                }
                if (i != node->nops - 1)
                    print(printer, ", ");
            }
        }
    }
}

void node_dump(const node_t* node) {
    file_printer_t file_printer = printer_from_file(stdout, true, "    ", 0);
    node_print(node, &file_printer.printer);
    print(&file_printer.printer, "\n");
}

static inline void print_indent(printer_t* printer) {
    for (size_t i = 0; i < printer->indent; ++i)
        print(printer, "{0:s}", { .str = printer->tab });
}

static inline void ast_print_list(const ast_list_t* list, printer_t* printer, const char* sep, bool new_line) {
    while (list) {
        ast_print(list->ast, printer);
        if (list->next) {
            print(printer, sep);
            if (new_line) print_indent(printer);
        }
        list = list->next;
    }
}

static inline void ast_print_binop_op(const ast_t* op, printer_t* printer, int prec) {
    bool needs_parens = op->tag == AST_BINOP && binop_precedence(op->data.binop.tag) > prec;
    if (needs_parens) print(printer, "(");
    ast_print(op, printer);
    if (needs_parens) print(printer, ")");
}

void ast_print(const ast_t* ast, printer_t* printer) {
    switch (ast->tag) {
        case AST_ID: print(printer, "{$id}{0:s}{$}", { .str = ast->data.id.str }); break;
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_FLT:
                case LIT_INT:
                    print(printer, "{$lit}{0:s}{$}", { .str = ast->data.lit.str });
                    break;
                case LIT_STR:  print(printer, "{$lit}\"{0:s}\"{$}", { .str = ast->data.lit.str }); break;
                case LIT_CHR:  print(printer, "{$lit}\'{0:s}\'{$}", { .str = ast->data.lit.str }); break;
                case LIT_BOOL: print(printer, "{$lit}{0:b}{$}", { .b = ast->data.lit.value.bval });  break;
                default:
                    assert(false);
                    break;
            }
            break;
        case AST_MOD:
            print(printer, "{$key}mod{$} {0:s} {{\n", { .str = ast->data.mod.id->data.id.str });
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.mod.decls, printer, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_STRUCT:
            print(printer, "{$key}struct{$}{0:s}{$key}{1:s}{$} {2:s}",
                { .str = ast->data.struct_.byref ? " " : ""},
                { .str = ast->data.struct_.byref ? "byref" : ""},
                { .str = ast->data.struct_.id->data.id.str });
            ast_print(ast->data.struct_.members, printer);
            break;
        case AST_DEF:
            print(printer, "{$key}def{$} {0:s}", { .str = ast->data.mod.id->data.id.str });
            if (ast->data.def.param) {
                ast_print(ast->data.def.param, printer);
                print(printer, " ");
            } else
                print(printer, " = ");
            ast_print(ast->data.def.value, printer);
            break;
        case AST_VAR:
        case AST_VAL:
            print(printer, "{$key}{0:s}{$} ", { .str = ast->tag == AST_VAR ? "var" : "val" });
            ast_print(ast->data.varl.ptrn, printer);
            print(printer, " = ");
            ast_print(ast->data.varl.value, printer);
            break;
        case AST_ANNOT:
            ast_print(ast->data.annot.ast, printer);
            print(printer, " : ");
            ast_print(ast->data.annot.type, printer);
            break;
        case AST_PRIM:
            print(printer, "{$key}{0:s}{$}", { .str = prim2str(ast->data.prim.tag) });
            break;
        case AST_BLOCK:
            print(printer, "{{\n");
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.block.stmts, printer, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_TUPLE:
            print(printer, "(");
            ast_print_list(ast->data.tuple.args, printer, ", ", false);
            print(printer, ")");
            break;
        case AST_ARRAY:
            print(printer, "[");
            ast_print_list(ast->data.array.elems, printer, ast->data.array.regular ? "; " : ", ", false);
            print(printer, "]");
            break;
        case AST_FIELD:
            ast_print(ast->data.field.arg, printer);
            print(printer, ".{0:s}", { .str = ast->data.field.id->data.id.str });
            break;
        case AST_BINOP:
            {
                int prec = binop_precedence(ast->data.binop.tag);
                ast_print_binop_op(ast->data.binop.left, printer, prec);
                print(printer, " {0:s} ", { .str = binop_symbol(ast->data.binop.tag) });
                ast_print_binop_op(ast->data.binop.right, printer, prec);
            }
            break;
        case AST_UNOP:
            {
                bool prefix = unop_is_prefix(ast->data.unop.tag);
                const char* symbol = unop_symbol(ast->data.unop.tag);
                if (prefix) print(printer, "{0:s}", { .str = symbol });
                ast_print(ast->data.unop.arg, printer);
                if (!prefix) print(printer, "{0:s}", { .str = symbol });
            }
            break;
        case AST_LAMBDA:
            ast_print(ast->data.lambda.param, printer);
            print(printer, " => ");
            ast_print(ast->data.lambda.body, printer);
            break;
        case AST_CALL:
            ast_print(ast->data.call.callee, printer);
            ast_print(ast->data.call.arg, printer);
            break;
        case AST_IF:
            print(printer, "{$key}if{$} ");
            ast_print(ast->data.if_.cond, printer);
            print(printer, " ");
            ast_print(ast->data.if_.if_true, printer);
            if (ast->data.if_.if_false) {
                print(printer, " {$key}else{$} ");
                ast_print(ast->data.if_.if_false, printer);
            }
            break;
        case AST_WHILE:
            print(printer, "{$key}while{$} ");
            ast_print(ast->data.while_.cond, printer);
            print(printer, " ");
            ast_print(ast->data.while_.body, printer);
            break;
        case AST_FOR:
            print(printer, "{$key}for{$} (");
            ast_print(ast->data.for_.vars, printer);
            print(printer, " <- ");
            ast_print(ast->data.for_.expr, printer);
            print(printer, ") ");
            ast_print(ast->data.for_.body, printer);
            break;
        case AST_MATCH:
            ast_print(ast->data.match.arg, printer);
            print(printer, "{$key}match{$} {{");
            printer->indent++;
            print_indent(printer);
            ast_print_list(ast->data.match.cases, printer, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_CASE:
            print(printer, "{$key}case{$} ");
            ast_print(ast->data.case_.ptrn, printer);
            print(printer, " => ");
            printer->indent++;
            ast_print(ast->data.case_.value, printer);
            printer->indent--;
            break;
        case AST_CONT:
            switch (ast->data.cont.tag) {
                case CONT_BREAK:    print(printer, "{$key}break{$}");    break;
                case CONT_CONTINUE: print(printer, "{$key}continue{$}"); break;
                case CONT_RETURN:   print(printer, "{$key}return{$}");   break;
                default:
                    assert(false);
                    break;
            }
            break;
        case AST_PROGRAM:
            ast_print_list(ast->data.program.mods, printer, "\n", true);
            break;
        case AST_ERR:
            print(printer, "{$err}<syntax error>{$}");
            break;
        default:
            assert(false);
            break;
    }
}

void ast_dump(const ast_t* ast) {
    file_printer_t file_printer = printer_from_file(stdout, true, "    ", 0);
    ast_print(ast, &file_printer.printer);
    print(&file_printer.printer, "\n");
}

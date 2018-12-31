#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "print.h"
#include "type.h"
#include "node.h"
#include "ast.h"

static void print_type(printer_t* printer, const type_t* type) {
    switch (type->tag) {
        case TYPE_BOOL: print(printer, "{$key}bool{$}");  break;
        case TYPE_I8:   print(printer, "{$key}i8{$}");  break;
        case TYPE_I16:  print(printer, "{$key}i16{$}"); break;
        case TYPE_I32:  print(printer, "{$key}i32{$}"); break;
        case TYPE_I64:  print(printer, "{$key}i64{$}"); break;
        case TYPE_U8:   print(printer, "{$key}u8{$}");  break;
        case TYPE_U16:  print(printer, "{$key}u16{$}"); break;
        case TYPE_U32:  print(printer, "{$key}u32{$}"); break;
        case TYPE_U64:  print(printer, "{$key}u64{$}"); break;
        case TYPE_F32:  print(printer, "{$key}f32{$}"); break;
        case TYPE_F64:  print(printer, "{$key}f64{$}"); break;
        case TYPE_MEM:  print(printer, "{$key}mem{$}"); break;
        case TYPE_PTR:
            print_type(printer, type->ops[0]);
            print(printer, "*");
            break;
        case TYPE_TUPLE:
            print(printer, "(");
            for (size_t i = 0; i < type->nops; ++i) {
                print_type(printer, type->ops[i]);
                if (i != type->nops - 1)
                    print(printer, ", ");
            }
            print(printer, ")");
            break;
        case TYPE_ARRAY:
            print(printer, "[");
            print_type(printer, type->ops[0]);
            print(printer, "]");
            break;
        case TYPE_FN:
            if (type->ops[0]->tag == TYPE_FN) print(printer, "(");
            print_type(printer, type->ops[0]);
            if (type->ops[0]->tag == TYPE_FN) print(printer, ")");
            print(printer, " => ");
            print_type(printer, type->ops[1]);
            break;
        case TYPE_STRUCT:
            print(printer, type->data.struct_def->byref ? "{$key}struct{$} {$key}byref{$} {0:s}" : "{$key}struct{$} {0:s}", { .s = type->data.struct_def->name });
            if (type->nops > 0) {
                print(printer, "[");
                for (size_t i = 0; i < type->nops; ++i) {
                    print_type(printer, type->ops[i]);
                    if (i != type->nops - 1)
                        print(printer, ", ");
                }
                print(printer, "]");
            }
            break;
        default:
            assert(false);
            break;
    }
}

static inline void print_node_name(printer_t* printer, const node_t* node) {
    print(printer, "<{0:s}{1:s}{$id}{2:p}{$}>",
        { .s = node->dbg ? node->dbg->name : "" },
        { .s = node->dbg ? " : " : "" },
        { .p = node });
}

static void print_node(printer_t* printer, const node_t* node) {
    if (node->tag == NODE_LITERAL) {
        switch (node->type->tag) {
            case TYPE_BOOL: print(printer, "{$key}bool{$} {$lit}{0:b}{$}",  { .b   = node->data.box.b   }); break;
            case TYPE_I8:   print(printer, "{$key}i8{$} {$lit}{0:i8}{$}",   { .i8  = node->data.box.i8  }); break;
            case TYPE_I16:  print(printer, "{$key}i16{$} {$lit}{0:i16}{$}", { .i16 = node->data.box.i16 }); break;
            case TYPE_I32:  print(printer, "{$key}i32{$} {$lit}{0:i32}{$}", { .i32 = node->data.box.i32 }); break;
            case TYPE_I64:  print(printer, "{$key}i64{$} {$lit}{0:i64}{$}", { .i64 = node->data.box.i64 }); break;
            case TYPE_U8:   print(printer, "{$key}u8{$} {$lit}{0:u8}{$}",   { .u8  = node->data.box.u8  }); break;
            case TYPE_U16:  print(printer, "{$key}u16{$} {$lit}{0:u16}{$}", { .i16 = node->data.box.u16 }); break;
            case TYPE_U32:  print(printer, "{$key}u32{$} {$lit}{0:u32}{$}", { .i32 = node->data.box.u32 }); break;
            case TYPE_U64:  print(printer, "{$key}u64{$} {$lit}{0:u64}{$}", { .i64 = node->data.box.u64 }); break;
            case TYPE_F32:  print(printer, "{$key}f32{$} {$lit}{0:f32}{$}", { .f32 = node->data.box.f32 }); break;
            case TYPE_F64:  print(printer, "{$key}f64{$} {$lit}{0:f64}{$}", { .f64 = node->data.box.f64 }); break;
            default:
                assert(false);
                break;
        }
    } else {
        if (node->nops > 0) {
            print_node_name(printer, node);
            print(printer, " = ");
        }
        print_type(printer, node->type);
        const char* op = NULL;
        switch (node->tag) {
#define NODE(name, str) case name: op = str; break;
            NODE_LIST(NODE)
#undef NODE
            default:
                assert(false);
                break;
        }
        print(printer, " {$key}{0:s}{$}", { .s = op });
        if (node->nops > 0) {
            print(printer, " ");
            for (size_t i = 0; i < node->nops; ++i) {
                if (node->ops[i]->nops == 0) {
                    // Print literals and undefs inline
                    print_node(printer, node->ops[i]);
                } else {
                    print_node_name(printer, node->ops[i]);
                }
                if (i != node->nops - 1)
                    print(printer, ", ");
            }
        }
    }
}

static void print_ast(printer_t*, const ast_t*);

static inline void print_indent(printer_t* printer) {
    for (size_t i = 0; i < printer->indent; ++i)
        print(printer, "{0:s}", { .s = printer->tab });
}

static inline void print_ast_list(printer_t* printer, const ast_list_t* list, const char* sep, bool new_line) {
    while (list) {
        print_ast(printer, list->ast);
        if (list->next) {
            print(printer, sep);
            if (new_line) print_indent(printer);
        }
        list = list->next;
    }
}

static inline void print_ast_binop_op(printer_t* printer, const ast_t* op, int prec) {
    bool needs_parens = op->tag == AST_BINOP && binop_precedence(op->data.binop.tag) > prec;
    if (needs_parens) print(printer, "(");
    print_ast(printer, op);
    if (needs_parens) print(printer, ")");
}

static void print_ast(printer_t* printer, const ast_t* ast) {
    switch (ast->tag) {
        case AST_ID: print(printer, "{0:s}", { .s = ast->data.id.str }); break;
        case AST_LIT:
            switch (ast->data.lit.tag) {
                case LIT_FLT:
                case LIT_INT:
                    print(printer, "{$lit}{0:s}{$}", { .s = ast->data.lit.str });
                    break;
                case LIT_STR:  print(printer, "{$lit}\"{0:s}\"{$}", { .s = ast->data.lit.str }); break;
                case LIT_CHR:  print(printer, "{$lit}\'{0:s}\'{$}", { .s = ast->data.lit.str }); break;
                case LIT_BOOL: print(printer, "{$lit}{0:b}{$}", { .b = ast->data.lit.value.bval });  break;
                default:
                    assert(false);
                    break;
            }
            break;
        case AST_MOD:
            print(printer, "{$key}mod{$} {0:s} {{\n", { .s = ast->data.mod.id->data.id.str });
            printer->indent++;
            print_indent(printer);
            print_ast_list(printer, ast->data.mod.decls, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_STRUCT:
            print(printer, "{$key}struct{$}{0:s}{$key}{1:s}{$} {2:s}",
                { .s = ast->data.struct_.byref ? " " : ""},
                { .s = ast->data.struct_.byref ? "byref" : ""},
                { .s = ast->data.struct_.id->data.id.str });
            print_ast(printer, ast->data.struct_.members);
            break;
        case AST_DEF:
            print(printer, "{$key}def{$} {0:s}", { .s = ast->data.mod.id->data.id.str });
            if (ast->data.def.param)
                print_ast(printer, ast->data.def.param);
            if (ast->data.def.ret) {
                print(printer, " : ");
                print_ast(printer, ast->data.def.ret);
            }
            print(printer, " = ");
            print_ast(printer, ast->data.def.value);
            break;
        case AST_VAR:
        case AST_VAL:
            print(printer, "{$key}{0:s}{$} ", { .s = ast->tag == AST_VAR ? "var" : "val" });
            print_ast(printer, ast->data.varl.ptrn);
            print(printer, " = ");
            print_ast(printer, ast->data.varl.value);
            break;
        case AST_ANNOT:
            print_ast(printer, ast->data.annot.ast);
            print(printer, " : ");
            print_ast(printer, ast->data.annot.type);
            break;
        case AST_PRIM:
            print(printer, "{$key}{0:s}{$}", { .s = prim2str(ast->data.prim.tag) });
            break;
        case AST_BLOCK:
            print(printer, "{{\n");
            printer->indent++;
            print_indent(printer);
            print_ast_list(printer, ast->data.block.stmts, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_TUPLE:
            print(printer, "(");
            print_ast_list(printer, ast->data.tuple.args, ", ", false);
            print(printer, ")");
            break;
        case AST_ARRAY:
            print(printer, "[");
            print_ast_list(printer, ast->data.array.elems, ast->data.array.regular ? "; " : ", ", false);
            print(printer, "]");
            break;
        case AST_FIELD:
            print_ast(printer, ast->data.field.arg);
            print(printer, ".{0:s}", { .s = ast->data.field.id->data.id.str });
            break;
        case AST_BINOP:
            {
                int prec = binop_precedence(ast->data.binop.tag);
                print_ast_binop_op(printer, ast->data.binop.left, prec);
                print(printer, " {0:s} ", { .s = binop_symbol(ast->data.binop.tag) });
                print_ast_binop_op(printer, ast->data.binop.right, prec);
            }
            break;
        case AST_UNOP:
            {
                bool prefix = unop_is_prefix(ast->data.unop.tag);
                const char* symbol = unop_symbol(ast->data.unop.tag);
                if (prefix) print(printer, "{0:s}", { .s = symbol });
                print_ast(printer, ast->data.unop.arg);
                if (!prefix) print(printer, "{0:s}", { .s = symbol });
            }
            break;
        case AST_LAMBDA:
            print_ast(printer, ast->data.lambda.param);
            print(printer, " => ");
            print_ast(printer, ast->data.lambda.body);
            break;
        case AST_CALL:
            print_ast(printer, ast->data.call.callee);
            print_ast(printer, ast->data.call.arg);
            break;
        case AST_IF:
            print(printer, "{$key}if{$} ");
            print_ast(printer, ast->data.if_.cond);
            print(printer, " ");
            print_ast(printer, ast->data.if_.if_true);
            if (ast->data.if_.if_false) {
                print(printer, " {$key}else{$} ");
                print_ast(printer, ast->data.if_.if_false);
            }
            break;
        case AST_WHILE:
            print(printer, "{$key}while{$} ");
            print_ast(printer, ast->data.while_.cond);
            print(printer, " ");
            print_ast(printer, ast->data.while_.body);
            break;
        case AST_FOR:
            print(printer, "{$key}for{$} (");
            print_ast(printer, ast->data.for_.vars);
            print(printer, " <- ");
            print_ast(printer, ast->data.for_.expr);
            print(printer, ") ");
            print_ast(printer, ast->data.for_.body);
            break;
        case AST_MATCH:
            print_ast(printer, ast->data.match.arg);
            print(printer, " {$key}match{$} {{\n");
            printer->indent++;
            print_indent(printer);
            print_ast_list(printer, ast->data.match.cases, "\n", true);
            printer->indent--;
            print(printer, "\n");
            print_indent(printer);
            print(printer, "}");
            break;
        case AST_CASE:
            print(printer, "{$key}case{$} ");
            print_ast(printer, ast->data.case_.ptrn);
            print(printer, " => ");
            printer->indent++;
            print_ast(printer, ast->data.case_.value);
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
            print_ast_list(printer, ast->data.program.mods, "\n", true);
            break;
        case AST_ERR:
            print(printer, "{$err}<syntax error>{$}");
            break;
        default:
            assert(false);
            break;
    }
}

static size_t format(char* buf, size_t buf_len, const char* fmt, const fmt_arg_t* args, bool colorize) {
    // Reserve space for null terminator
    if (buf_len == 0)
        return 0;
    buf_len--;

    mem_printer_t mem_printer = printer_from_buffer(buf, buf_len);
    mem_printer.printer.colorize = colorize;
    const char* ptr = fmt;
    size_t len = 0;

    while (*ptr && len < buf_len) {
        while (*ptr && *ptr != '{' && len < buf_len) buf[len++] = *(ptr++);
        if (len == buf_len)
            break;
        if (*ptr == '{') {
            ptr++;
            if (*ptr == '{') {
                buf[len++] = '{';
                ptr++;
                continue;
            } else if (*ptr == '$') {
                ptr++;
                const char* color = NULL;
                size_t color_len = 0;
                switch (*ptr) {
                    case 'k':
                        assert(!strncmp(ptr, "key", 3)); ptr += 3;
                        color = "\33[34;1m";
                        color_len = 7;
                        break;
                    case 'e':
                        assert(!strncmp(ptr, "err", 3)); ptr += 3;
                        color = "\33[31;1m";
                        color_len = 7;
                        break;
                    case 'l':
                        if (ptr[1] == 'i') {
                            assert(!strncmp(ptr, "lit", 3)); ptr += 3;
                            color = "\33[36;1m";
                            color_len = 7;
                        } else {
                            assert(!strncmp(ptr, "loc", 3)); ptr += 3;
                            color = "\33[37;1m";
                            color_len = 7;
                        }
                        break;
                    case 'i':
                        assert(!strncmp(ptr, "id", 2)); ptr += 2;
                        color = "\33[33m";
                        color_len = 5;
                        break;
                    case 'w':
                        assert(!strncmp(ptr, "wrn", 3)); ptr += 3;
                        color = "\33[33;1m";
                        color_len = 7;
                        break;
                    case 'n':
                        assert(!strncmp(ptr, "not", 3)); ptr += 3;
                        color = "\33[36;1m";
                        color_len = 7;
                        break;
                    case '}':
                        color = "\33[0m";
                        color_len = 4;
                        break;
                    default:
                        assert(false);
                        break;
                }
                // Do not write color codes partially to avoid problems
                if (colorize && len + color_len <= buf_len) {
                    memcpy(buf + len, color, color_len);
                    len += color_len;
                }
            } else if (isdigit(*ptr)) {
                size_t id = strtoul(ptr, (char**)&ptr, 10);
                int n = 0;
                assert(*ptr == ':');
                ptr++;
                switch (*ptr) {
                    case 'u':
                        ptr++;
                        switch (*ptr) {
                            case '8':
                                n = snprintf(buf + len, buf_len - len, "%"PRIu8, args[id].u8);
                                break;
                            case '1':
                                ptr++; assert(*ptr == '6');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu16, args[id].u16);
                                break;
                            case '3':
                                ptr++; assert(*ptr == '2');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu32, args[id].u32);
                                break;
                            case '6':
                                ptr++; assert(*ptr == '4');
                                n = snprintf(buf + len, buf_len - len, "%"PRIu64, args[id].u64);
                                break;
                        }
                         ptr++;
                        break;
                    case 'i':
                        ptr++;
                        switch (*ptr) {
                            case '8':
                                n = snprintf(buf + len, buf_len - len, "%"PRIi8, args[id].i8); ptr++;
                                break;
                            case '1':
                                ptr++; assert(*ptr == '6');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi16, args[id].i16);
                                break;
                            case '3':
                                ptr++; assert(*ptr == '2');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi32, args[id].i32);
                                break;
                            case '6':
                                ptr++; assert(*ptr == '4');
                                n = snprintf(buf + len, buf_len - len, "%"PRIi64, args[id].i64);
                                break;
                        }
                        ptr++;
                        break;
                    case 'p':
                        n = snprintf(buf + len, buf_len - len, "%"PRIxPTR, (intptr_t)args[id].p); ptr++;
                        break;
                    case 'b':
                        n = snprintf(buf + len, buf_len - len, "%s", args[id].b ? "true" : "false"); ptr++;
                        break;
                    case 's':
                        {
                            const char* str = args[id].s;
                            while (*str && len < buf_len)
                                buf[len++] = *(str++);
                            ptr++;
                        }
                        break;
                    case 'c':
                        buf[len++] = args[id].c;
                        ptr++;
                        break;
                    case 'a':
                        ptr++;
                        mem_printer.off = len;
                        print_ast(&mem_printer.printer, args[id].a);
                        len = mem_printer.off;
                        break;
                    case 't':
                        ptr++;
                        mem_printer.off = len;
                        print_type(&mem_printer.printer, args[id].t);
                        len = mem_printer.off;
                        break;
                    case 'n':
                        ptr++;
                        mem_printer.off = len;
                        print_node(&mem_printer.printer, args[id].n);
                        len = mem_printer.off;
                        break;
                    default:
                        assert(false);
                        break;
                }
                assert(n >= 0);
                len += n;
            } else {
                assert(false);
            }
            assert(*ptr == '}');
            ptr++;
        }
    }

    buf[len] = 0;
    return len;
}

static void file_printer_format(printer_t* printer, const char* fmt, const fmt_arg_t* args) {
    file_printer_t* file_printer = (file_printer_t*)printer;

    // Allocate a temporary buffer on the stack
    static const size_t stack_buf_size = 1024;
    char tmp[stack_buf_size];
    char* buf  = tmp;
    size_t cap = stack_buf_size;
    size_t len = 0;

    do {
        if (len != 0) {
            // Perform dynamic allocation if stack space is not enough
            if (buf == tmp)
                buf = NULL;
            cap = (len + 1) * 2;
            buf = xrealloc(buf, cap);
        }
        len = format(buf, cap, fmt, args, printer->colorize);
    } while (len + 1 >= cap);

    fwrite(buf, 1, len, file_printer->fp);
    if (buf != tmp)
        free(buf);
}

file_printer_t printer_from_file(FILE* fp) {
    return (file_printer_t) {
        .printer = (printer_t) {
            .format   = file_printer_format,
            .colorize = false,
            .tab      = "    ",
            .indent   = 0
        },
        .fp  = fp
    };
}

static void mem_printer_format(printer_t* printer, const char* fmt, const fmt_arg_t* args) {
    mem_printer_t* mem_printer = (mem_printer_t*)printer;
    mem_printer->off += format(mem_printer->buf + mem_printer->off, mem_printer->cap - mem_printer->off, fmt, args, printer->colorize);
}

mem_printer_t printer_from_buffer(char* buf, size_t cap) {
    return (mem_printer_t) {
        .printer = (printer_t) {
            .format   = mem_printer_format,
            .colorize = false,
            .tab      = "    ",
            .indent   = 0
        },
        .buf = buf,
        .cap = cap,
        .off = 0
    };
}

void node_dump(const node_t* node) {
    file_printer_t file_printer = printer_from_file(stdout);
    print_node(&file_printer.printer, node);
    printf("\n");
}

void type_dump(const type_t* type) {
    file_printer_t file_printer = printer_from_file(stdout);
    print_type(&file_printer.printer, type);
    printf("\n");
}

void ast_dump(const ast_t* ast) {
    file_printer_t file_printer = printer_from_file(stdout);
    print_ast(&file_printer.printer, ast);
    printf("\n");
}

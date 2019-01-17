#include "ast.h"

bool ast_is_ptrn(const ast_t* ast) {
    switch (ast->tag) {
        case AST_ANNOT: return ast_is_ptrn(ast->data.annot.ast);
        case AST_ID:    return true;
        case AST_LIT:   return true;
        case AST_TUPLE:
            FORALL_AST(ast->data.tuple.args, arg, {
                if (!ast_is_ptrn(arg))
                    return false;
            })
            return true;
        default:
            return false;
    }
}

bool ast_is_refutable(const ast_t* ast) {
    switch (ast->tag) {
        case AST_ANNOT: return ast_is_refutable(ast->data.annot.ast);
        case AST_ID:    return false;
        case AST_LIT:   return true;
        case AST_TUPLE:
            FORALL_AST(ast->data.tuple.args, arg, {
                if (ast_is_refutable(arg))
                    return true;
            })
            return false;
        default:
            assert(false);
            return false;
    }
}

size_t ast_list_length(const ast_list_t* list) {
    size_t len = 0;
    while (list) {
        len++;
        list = list->next;
    }
    return len;
}

const char* prim2str(uint32_t tag) {
    switch (tag) {
#define PRIM(name, str) case name: return str;
        PRIM_LIST(PRIM_, PRIM)
#undef PRIM
        default:
            assert(false);
            return "";
    }
}

int binop_precedence(uint32_t tag) {
    switch (tag) {
        case BINOP_ADD:          return 2;
        case BINOP_SUB:          return 2;
        case BINOP_MUL:          return 1;
        case BINOP_DIV:          return 1;
        case BINOP_REM:          return 1;
        case BINOP_AND:          return 4;
        case BINOP_OR:           return 6;
        case BINOP_XOR:          return 5;
        case BINOP_LSHFT:        return 3;
        case BINOP_RSHFT:        return 3;
        case BINOP_ASSIGN:       return 10;
        case BINOP_ASSIGN_ADD:   return 10;
        case BINOP_ASSIGN_SUB:   return 10;
        case BINOP_ASSIGN_MUL:   return 10;
        case BINOP_ASSIGN_DIV:   return 10;
        case BINOP_ASSIGN_REM:   return 10;
        case BINOP_ASSIGN_AND:   return 10;
        case BINOP_ASSIGN_OR:    return 10;
        case BINOP_ASSIGN_XOR:   return 10;
        case BINOP_ASSIGN_LSHFT: return 10;
        case BINOP_ASSIGN_RSHFT: return 10;
        case BINOP_LOGIC_AND:    return 8;
        case BINOP_LOGIC_OR:     return 9;
        case BINOP_CMPEQ:        return 7;
        case BINOP_CMPNE:        return 7;
        case BINOP_CMPGT:        return 7;
        case BINOP_CMPLT:        return 7;
        case BINOP_CMPGE:        return 7;
        case BINOP_CMPLE:        return 7;
        default:
            assert(false);
            return 0;
    }
}

uint32_t binop_tag_from_token(uint32_t tag) {
    switch (tag) {
        case TOK_ADD:     return BINOP_ADD;
        case TOK_SUB:     return BINOP_SUB;
        case TOK_MUL:     return BINOP_MUL;
        case TOK_DIV:     return BINOP_DIV;
        case TOK_REM:     return BINOP_REM;
        case TOK_AND:     return BINOP_AND;
        case TOK_OR:      return BINOP_OR;
        case TOK_XOR:     return BINOP_XOR;
        case TOK_LSHFT:   return BINOP_LSHFT;
        case TOK_RSHFT:   return BINOP_RSHFT;
        case TOK_EQ:      return BINOP_ASSIGN;
        case TOK_ADDEQ:   return BINOP_ASSIGN_ADD;
        case TOK_SUBEQ:   return BINOP_ASSIGN_SUB;
        case TOK_MULEQ:   return BINOP_ASSIGN_MUL;
        case TOK_DIVEQ:   return BINOP_ASSIGN_DIV;
        case TOK_REMEQ:   return BINOP_ASSIGN_REM;
        case TOK_ANDEQ:   return BINOP_ASSIGN_AND;
        case TOK_OREQ:    return BINOP_ASSIGN_OR;
        case TOK_XOREQ:   return BINOP_ASSIGN_XOR;
        case TOK_LSHFTEQ: return BINOP_ASSIGN_LSHFT;
        case TOK_RSHFTEQ: return BINOP_ASSIGN_RSHFT;
        case TOK_DBLAND:  return BINOP_LOGIC_AND;
        case TOK_DBLOR:   return BINOP_LOGIC_OR;
        case TOK_CMPEQ:   return BINOP_CMPEQ;
        case TOK_NOTEQ:   return BINOP_CMPNE;
        case TOK_LANGLE:  return BINOP_CMPLT;
        case TOK_RANGLE:  return BINOP_CMPGT;
        case TOK_CMPGE:   return BINOP_CMPGE;
        case TOK_CMPLE:   return BINOP_CMPLE;
        default:          return INVALID_TAG;
    }
}

const char* binop_symbol(uint32_t tag) {
    switch (tag) {
        case BINOP_ADD:          return "+";
        case BINOP_SUB:          return "-";
        case BINOP_MUL:          return "*";
        case BINOP_DIV:          return "/";
        case BINOP_REM:          return "%";
        case BINOP_AND:          return "&";
        case BINOP_OR:           return "|";
        case BINOP_XOR:          return "^";
        case BINOP_LSHFT:        return "<<";
        case BINOP_RSHFT:        return ">>";
        case BINOP_ASSIGN:       return "=";
        case BINOP_ASSIGN_ADD:   return "+=";
        case BINOP_ASSIGN_SUB:   return "-=";
        case BINOP_ASSIGN_MUL:   return "*=";
        case BINOP_ASSIGN_DIV:   return "/=";
        case BINOP_ASSIGN_REM:   return "%=";
        case BINOP_ASSIGN_AND:   return "&=";
        case BINOP_ASSIGN_OR:    return "|=";
        case BINOP_ASSIGN_XOR:   return "^=";
        case BINOP_ASSIGN_LSHFT: return "<<=";
        case BINOP_ASSIGN_RSHFT: return ">>=";
        case BINOP_LOGIC_AND:    return "&&";
        case BINOP_LOGIC_OR:     return "||";
        case BINOP_CMPEQ:        return "==";
        case BINOP_CMPNE:        return "!=";
        case BINOP_CMPGT:        return ">";
        case BINOP_CMPLT:        return "<";
        case BINOP_CMPGE:        return ">=";
        case BINOP_CMPLE:        return "<=";
        default:
            assert(false);
            return "";
    }
}

bool unop_is_prefix(uint32_t tag) {
    switch (tag) {
        case UNOP_NOT:      return true;
        case UNOP_NEG:      return true;
        case UNOP_PLUS:     return true;
        case UNOP_PRE_INC:  return true;
        case UNOP_PRE_DEC:  return true;
        case UNOP_POST_INC: return false;
        case UNOP_POST_DEC: return false;
        default:
            assert(false);
            return false;
    }
}

const char* unop_symbol(uint32_t tag) {
    switch (tag) {
        case UNOP_NOT:  return "!";
        case UNOP_NEG:  return "-";
        case UNOP_PLUS: return "+";
        case UNOP_PRE_INC:
        case UNOP_POST_INC:
            return "++";
        case UNOP_PRE_DEC:
        case UNOP_POST_DEC:
            return "--";
        default:
            assert(false);
            return "";
    }
}

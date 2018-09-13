#include "ast.h"

bool ast_is_refutable(const ast_t* ast) {
    ast_list_t* list;
    switch (ast->tag) {
        case AST_ID:  return false;
        case AST_LIT: return true;
        case AST_TUPLE:
            list = ast->data.tuple.args;
            while (list) {
                if (ast_is_refutable(list->ast))
                    return true;
                list = list->next;
            }
            return false;
        default:
            assert(false);
            return false;
    }
}

int binop_precedence(uint32_t tag) {
    static const int prec[] = {
        2,  // BINOP_ADD
        2,  // BINOP_SUB
        1,  // BINOP_MUL
        1,  // BINOP_DIV
        1,  // BINOP_REM
        4,  // BINOP_AND
        6,  // BINOP_OR
        5,  // BINOP_XOR
        3,  // BINOP_LSHFT
        3,  // BINOP_RSHFT
        10, // BINOP_ASSIGN
        10, // BINOP_ASSIGN_ADD
        10, // BINOP_ASSIGN_SUB
        10, // BINOP_ASSIGN_MUL
        10, // BINOP_ASSIGN_DIV
        10, // BINOP_ASSIGN_REM
        10, // BINOP_ASSIGN_AND
        10, // BINOP_ASSIGN_OR
        10, // BINOP_ASSIGN_XOR
        10, // BINOP_ASSIGN_LSHFT
        10, // BINOP_ASSIGN_RSHFT
        8,  // BINOP_LOGIC_AND
        9,  // BINOP_LOGIC_OR
        7,  // BINOP_CMPEQ
        7,  // BINOP_CMPNE
        7,  // BINOP_CMPGT
        7,  // BINOP_CMPLT
        7,  // BINOP_CMPGE
        7   // BINOP_CMPLE
    };
    assert(tag < sizeof(prec) / sizeof(prec[0]));
    assert(prec[tag] <= MAX_BINOP_PRECEDENCE);
    return prec[tag];
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
    static const char* symbols[] = {
        "+",   // BINOP_ADD
        "-",   // BINOP_SUB
        "*",   // BINOP_MUL
        "/",   // BINOP_DIV
        "%",   // BINOP_REM
        "&",   // BINOP_AND
        "|",   // BINOP_OR
        "^",   // BINOP_XOR
        "<<",  // BINOP_LSHFT
        ">>",  // BINOP_RSHFT
        "=",   // BINOP_ASSIGN
        "+=",  // BINOP_ASSIGN_ADD
        "-=",  // BINOP_ASSIGN_SUB
        "*=",  // BINOP_ASSIGN_MUL
        "/=",  // BINOP_ASSIGN_DIV
        "%=",  // BINOP_ASSIGN_REM
        "&=",  // BINOP_ASSIGN_AND
        "|=",  // BINOP_ASSIGN_OR
        "^=",  // BINOP_ASSIGN_XOR
        "<<=", // BINOP_ASSIGN_LSHFT
        ">>=", // BINOP_ASSIGN_RSHFT
        "&&",  // BINOP_LOGIC_AND
        "||",  // BINOP_LOGIC_OR
        "==",  // BINOP_CMPEQ
        "!=",  // BINOP_CMPNE
        ">",   // BINOP_CMPGT
        "<",   // BINOP_CMPLT
        ">=",  // BINOP_CMPGE
        "<="   // BINOP_CMPLE
    };
    assert(tag < sizeof(symbols) / sizeof(symbols[0]));
    return symbols[tag];
}

bool unop_is_prefix(uint32_t tag) {
    static const bool prefix[] = {
        true,  // UNOP_NOT
        true,  // UNOP_NEG
        true,  // UNOP_PLUS
        true,  // UNOP_PRE_INC
        true,  // UNOP_PRE_DEC
        false, // UNOP_POST_INC
        false  // UNOP_POST_DEC
    };
    assert(tag < sizeof(prefix) / sizeof(prefix[0]));
    return prefix[tag];
}

const char* unop_symbol(uint32_t tag) {
    static const char* symbols[] = {
        "!",  // UNOP_NOT
        "-",  // UNOP_NEG
        "+",  // UNOP_PLUS
        "++", // UNOP_PRE_INC
        "--", // UNOP_PRE_DEC
        "++", // UNOP_POST_INC
        "--"  // UNOP_POST_DEC
    };
    assert(tag < sizeof(symbols) / sizeof(symbols[0]));
    return symbols[tag];
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

typedef void (*action_t) (void*);
typedef struct trie_s trie_t;

struct trie_s {
    uint8_t val;
    void* data;
    action_t action;
    int16_t beg, end;
    trie_t* children[256];
};

static trie_t* new(char c) {
    trie_t* trie = calloc(1, sizeof(trie_t));
    trie->val = c;
    trie->beg = 256;
    trie->end = 0;
    return trie;
}

static trie_t* add(trie_t* trie, const char* str) {
    while (*str) {
        uint8_t c = *str;
        if (!trie->children[c]) {
            trie_t* child = new(c);
            trie->beg = trie->beg > c ? c : trie->beg;
            trie->end = trie->end < c + 1 ? c + 1 : trie->end;
            trie->children[c] = child;
        }
        trie = trie->children[c];
        str++;
    }
    assert(!trie->action && !trie->data);
    return trie;
}

static void destroy(trie_t* trie) {
    for (int i = trie->beg; i < trie->end; ++i) {
        if (trie->children[i])
            destroy(trie->children[i]);
    }
    free(trie);
}

static void display(const trie_t* trie, int depth) {
    if (depth <= 0)
        printf("// *\n");
    else {
        printf("// |");
        for (int i = 0; i < depth; ++i) putc('-', stdout);
        printf(" %c\n", trie->val);
    }
    for (int i = trie->beg; i < trie->end; ++i) {
        if (trie->children[i])
            display(trie->children[i], depth + 1);
    }
}

static void print_keyword(void* data) {
    printf("(tok_t) { .tag = %s, .loc = loc }", (const char*)data);
}

static void print_boolean(void* data) {
    printf("(tok_t) { .tag = TOK_BLT, .loc = loc, .lit = { .bval = %s } }", (const char*)data);
}

static void add_keyword(trie_t* trie, const char* key, const char* tok) {
    trie = add(trie, key);
    trie->action = print_keyword;
    trie->data   = (void*)tok;
}

static void add_boolean(trie_t* trie, const char* key, const char* value) {
    trie = add(trie, key);
    trie->action = print_boolean;
    trie->data   = (void*)value;
}

static void generate(const trie_t* trie, int depth, int indent) {
    int nelems = trie->end - trie->beg;
    if (nelems > 1 || (trie->action && nelems > 0)) {
        printf("%*sswitch (str[%d]) {\n", indent * 4, "", depth);
        for (int i = trie->beg; i < trie->end; ++i) {
            if (trie->children[i]) {
                printf("%*scase \'%c\':\n", (indent + 1) * 4, "", trie->children[i]->val);
                generate(trie->children[i], depth + 1, indent + 2);
                printf("%*sbreak;\n", (indent + 2) * 4, "");
            }
        }
        if (trie->action) {
            printf("%*scase \'\\0\': return ", (indent + 1) * 4, "");
            trie->action(trie->data);
            printf(";\n");
        }
        printf("%*s}\n", indent * 4, "");
    } else if (nelems > 0) {
        for (int i = trie->beg; i < trie->end; ++i) {
            if (trie->children[i]) {
                printf("%*sif (str[%d] == \'%c\') {\n", indent * 4, "", depth, trie->children[i]->val);
                generate(trie->children[i], depth + 1, indent + 1);
                printf("%*s}\n", indent * 4, "");
                break;
            }
        }
    } else {
        printf("%*sif (str[%d] == \'\\0\') return ", indent * 4, "", depth);
        trie->action(trie->data);
        printf(";\n");
    }
}

int main(int argc, char** argv) {
    (void)argc, (void)argv; // Silence "unused variable" warnings
    trie_t* trie = new(0);

    add_keyword(trie, "i8" ,      "TOK_I8");
    add_keyword(trie, "i16",      "TOK_I16");
    add_keyword(trie, "i32",      "TOK_I32");
    add_keyword(trie, "i64",      "TOK_I64");
    add_keyword(trie, "u8" ,      "TOK_U8");
    add_keyword(trie, "u16",      "TOK_U16");
    add_keyword(trie, "u32",      "TOK_U32");
    add_keyword(trie, "u64",      "TOK_U64");
    add_keyword(trie, "f32",      "TOK_F32");
    add_keyword(trie, "f64",      "TOK_F64");
    add_keyword(trie, "def" ,     "TOK_DEF");
    add_keyword(trie, "var" ,     "TOK_VAR");
    add_keyword(trie, "val",      "TOK_VAL");
    add_keyword(trie, "if",       "TOK_IF");
    add_keyword(trie, "else",     "TOK_ELSE");
    add_keyword(trie, "while" ,   "TOK_WHILE");
    add_keyword(trie, "for",      "TOK_FOR");
    add_keyword(trie, "match",    "TOK_MATCH");
    add_keyword(trie, "case",     "TOK_CASE");
    add_keyword(trie, "break",    "TOK_BREAK");
    add_keyword(trie, "continue", "TOK_CONTINUE");
    add_keyword(trie, "return",   "TOK_RETURN");
    add_keyword(trie, "mod",      "TOK_MOD");
    add_keyword(trie, "bool",     "TOK_BOOL");
    add_keyword(trie, "struct",   "TOK_STRUCT");
    add_keyword(trie, "byref",    "TOK_BYREF");

    add_boolean(trie, "true",  "true");
    add_boolean(trie, "false", "false");

    display(trie, 0);
    printf("static inline tok_t tok_from_str(const char* str, loc_t loc) {\n");
    generate(trie, 0, 1);
    printf("    return (tok_t) { .tag = TOK_ID, .str = str, .loc = loc };\n}\n");
    destroy(trie);
    return 0;
}

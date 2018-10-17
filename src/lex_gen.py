class TrieNode:
    def __init__(self, c):
        self.elems = {}
        self.key = c
        self.val = None

def add(root, str, val):
    node = root
    p = 0
    while p < len(str):
        c = str[p]
        p = p + 1
        if c not in node.elems:
            node.elems[c] = TrieNode(c)
        node = node.elems[c]
    node.val = val

def display(root, depth):
    if depth != 0:
        print("// {} {}".format("|" + "-" * depth, root.key))
    else:
        print("// *")
    for key, value in root.elems.items():
        display(value, depth + 1)

def generate(root, depth, indent):
    tab = "    "
    if len(root.elems) > 1:
        print("{}switch (str[{}]) {{".format(tab * indent, depth))
        for key, val in root.elems.items():
            print("{}case \'{}\':".format(tab * (indent + 1), key))
            generate(val, depth + 1, indent + 2)
            print("{}break;".format(tab * (indent + 2)))
        if not root.val is None:
            print("{}case \'\\0\': return {};".format(tab * (indent + 1), root.val))
        print("{}}}".format(tab * indent))
    elif len(root.elems) == 1:
        if not root.val is None:
            print("{}if (str[{}] == \'\\0\') return {};".format(tab * indent, depth, root.val))
        key, val = next(iter(root.elems.items()))
        print("{}if (str[{}] == \'{}\') {{".format(tab * indent, depth, key))
        generate(val, depth + 1, indent + 1)
        print("{}}}".format(tab * indent))
    else:
        print("{}if (str[{}] == \'\\0\') return {};".format(tab * indent, depth, root.val))

def generate_linear(root, str, indent):
    tab = "    "
    if not root.val is None:
        print("{}if (!strcmp(str, \"{}\")) return {};".format(tab * indent, str, root.val))
    for key, val in root.elems.items():
        generate_linear(val, str + key, indent)

def keyword(x):
    return "(tok_t) { .tag = " + x + ", .loc = loc }" 

root = TrieNode('')

add(root, "i1" ,      keyword("TOK_I1"))
add(root, "i8" ,      keyword("TOK_I8"))
add(root, "i16",      keyword("TOK_I16"))
add(root, "i32",      keyword("TOK_I32"))
add(root, "i64",      keyword("TOK_I64"))
add(root, "u8" ,      keyword("TOK_U8"))
add(root, "u16",      keyword("TOK_U16"))
add(root, "u32",      keyword("TOK_U32"))
add(root, "u64",      keyword("TOK_U64"))
add(root, "f32",      keyword("TOK_F32"))
add(root, "f64",      keyword("TOK_F64"))
add(root, "def" ,     keyword("TOK_DEF"))
add(root, "var" ,     keyword("TOK_VAR"))
add(root, "val",      keyword("TOK_VAL"))
add(root, "if",       keyword("TOK_IF"))
add(root, "else",     keyword("TOK_ELSE"))
add(root, "while" ,   keyword("TOK_WHILE"))
add(root, "for",      keyword("TOK_FOR"))
add(root, "match",    keyword("TOK_MATCH"))
add(root, "case",     keyword("TOK_CASE"))
add(root, "break",    keyword("TOK_BREAK"))
add(root, "continue", keyword("TOK_CONTINUE"))
add(root, "return",   keyword("TOK_RETURN"))
add(root, "mod",      keyword("TOK_MOD"))

add(root, "true",     "(tok_t) { .tag = TOK_BOOL, .loc = loc, .lit = { .bval = true  } }")
add(root, "false",    "(tok_t) { .tag = TOK_BOOL, .loc = loc, .lit = { .bval = false } }")

display(root, 0)
print("static inline tok_t token_from_str(const char* str, loc_t loc) {")
generate(root, 0, 1)
#generate_linear(root, "", 1)
print("    return (tok_t) { .tag = TOK_ID, .str = str, .loc = loc };")
print("}")

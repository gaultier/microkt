#pragma once

typedef enum {
    TYPE_ANY,
    TYPE_UNIT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_BYTE,
    TYPE_INT,
    TYPE_SHORT,
    TYPE_LONG,
    TYPE_STRING,
} type_kind_t;

static const char type_to_str[][20] = {
    [TYPE_ANY] = "Any",     [TYPE_UNIT] = "Unit", [TYPE_BOOL] = "Bool",
    [TYPE_CHAR] = "Char",   [TYPE_BYTE] = "Byte", [TYPE_INT] = "Int",
    [TYPE_SHORT] = "Short", [TYPE_LONG] = "Long", [TYPE_STRING] = "String",
};

typedef struct {
    int ty_size;
    type_kind_t ty_kind;
} type_t;

const unsigned short FN_FLAG_SYNTHETIC = 0x1;
const unsigned short FN_FLAG_PUBLIC = 0x2;
const unsigned short FN_FLAG_PRIVATE = 0x4;

struct node_t;

typedef struct node_t node_t;

typedef struct {
    int bp_arg_i, bp_keyword_print_i, bp_rparen_i;
} builtin_println_t;

typedef struct {
    int bi_type_i, bi_lhs_i, bi_rhs_i;
} binary_t;

typedef enum {
    NODE_BUILTIN_PRINTLN,
    NODE_KEYWORD_BOOL,
    NODE_STRING,
    NODE_LONG,
    NODE_CHAR,
    NODE_ADD,
    NODE_SUBTRACT,
    NODE_MULTIPLY,
    NODE_DIVIDE,
    NODE_MODULO,
    NODE_LT,
    NODE_LE,
    NODE_EQ,
    NODE_NEQ,
    NODE_NOT,
    NODE_IF,
    NODE_BLOCK,
    NODE_VAR_DEF,
    NODE_VAR,
    NODE_ASSIGN,
    NODE_WHILE,
    NODE_FN_DECL,
    NODE_CALL,
    NODE_RETURN,
} node_kind_t;

const char node_kind_to_str[][30] = {
    [NODE_BUILTIN_PRINTLN] = "Print",
    [NODE_KEYWORD_BOOL] = "Bool",
    [NODE_STRING] = "String",
    [NODE_LONG] = "LONG",
    [NODE_CHAR] = "Char",
    [NODE_ADD] = "Plus",
    [NODE_SUBTRACT] = "Subtract",
    [NODE_MULTIPLY] = "Multiply",
    [NODE_MODULO] = "Modulo",
    [NODE_DIVIDE] = "Divide",
    [NODE_LT] = "Lt",
    [NODE_LE] = "Le",
    [NODE_EQ] = "Eq",
    [NODE_NEQ] = "Neq",
    [NODE_NOT] = "Not",
    [NODE_IF] = "If",
    [NODE_BLOCK] = "Block",
    [NODE_VAR_DEF] = "VarDef",
    [NODE_VAR] = "Var",
    [NODE_ASSIGN] = "Assign",
    [NODE_WHILE] = "While",
    [NODE_FN_DECL] = "FnDecl",
    [NODE_RETURN] = "Return",
    [NODE_CALL] = "Call",
};

typedef struct {
    long long int nu_val;
    int nu_tok_i;
} node_number_t;

typedef struct {
    int if_first_tok_i, if_last_tok_i, if_node_cond_i, if_node_then_i,
        if_node_else_i;
} if_t;

static const unsigned short BLOCK_FLAGS_SEEN_RETURN = 0x01;
typedef struct {
    int bl_first_tok_i, bl_last_tok_i, *bl_nodes_i, bl_parent_scope_i;
    unsigned short bl_flags;
} block_t;

typedef struct {
    int vd_first_tok_i, vd_last_tok_i, vd_name_tok_i, vd_init_node_i,
        vd_stack_offset;
    unsigned short vd_flags;
} var_def_t;

static const unsigned short VAR_FLAGS_VAL = 0x01;
static const unsigned short VAR_FLAGS_VAR = 0x02;

typedef struct {
    int va_tok_i, va_var_node_i;  // Node the variable refers to
} var_t;

typedef struct {
    int wh_first_tok_i, wh_last_tok_i, wh_cond_i, wh_body_i;
} while_t;

typedef struct {
    int fd_first_tok_i, fd_last_tok_i, fd_name_tok_i, fd_body_node_i;
    unsigned short fd_flags;
} fn_decl_t;

typedef struct {
    int ca_first_tok_i, ca_last_tok_i, ca_arity, ca_var_node_i;
} call_t;

typedef struct {
    int un_first_tok_i, un_last_tok_i, un_node_i;
} unary_t;

struct node_t {
    node_kind_t node_kind;
    int node_type_i;
    union {
        builtin_println_t node_builtin_println;  // NODE_BUILTIN_PRINTLN
        int node_string;                         // NODE_STRING, int = tok_i
        node_number_t node_num;  // NODE_LONG, NODE_CHAR, NODE_BOOL
        binary_t node_binary;    // NODE_ADD, NODE_SUBTRACT, NODE_MULTIPLY,
        // NODE_DIVIDE, NODE_MODULO
        unary_t node_unary;      // NODE_NOT, NODE_RETURN
        if_t node_if;            // NODE_IF
        block_t node_block;      // NODE_BLOCK
        var_def_t node_var_def;  // NODE_VAR_DEF
        var_t node_var;          // NODE_VAR
        while_t node_while;      // NODE_WHILE
        fn_decl_t node_fn_decl;  // NODE_FN_DECL
        call_t node_call;        // NODE_CALL
    } node_n;
};

#define NODE_PRINTLN(arg, keyword, rparen, type_i)                         \
    ((node_t){                                                             \
        .node_kind = NODE_BUILTIN_PRINTLN,                                 \
        .node_type_i = type_i,                                             \
        .node_n = {.node_builtin_println = {.bp_arg_i = arg,               \
                                            .bp_keyword_print_i = keyword, \
                                            .bp_rparen_i = rparen}}})
#define NODE_LONG(tok_i, type_i, val)                                   \
    ((node_t){.node_kind = NODE_LONG,                                   \
              .node_type_i = type_i,                                    \
              .node_n = {.node_num = (node_number_t){.nu_tok_i = tok_i, \
                                                     .nu_val = val}}})

#define NODE_CHAR(tok_i, type_i, val)                                   \
    ((node_t){.node_kind = NODE_CHAR,                                   \
              .node_type_i = type_i,                                    \
              .node_n = {.node_num = (node_number_t){.nu_tok_i = tok_i, \
                                                     .nu_val = val}}})

#define NODE_BOOL(n, type_i)                  \
    ((node_t){.node_kind = NODE_KEYWORD_BOOL, \
              .node_type_i = type_i,          \
              .node_n = {.node_boolean = n}})

#define NODE_STRING(n, type_i)          \
    ((node_t){.node_kind = NODE_STRING, \
              .node_type_i = type_i,    \
              .node_n = {.node_string = n}})

#define NODE_BINARY(kind, lhs_i, rhs_i, type_i)                          \
    ((node_t){.node_kind = kind,                                         \
              .node_type_i = type_i,                                     \
              .node_n = {.node_binary = ((binary_t){.bi_type_i = type_i, \
                                                    .bi_lhs_i = lhs_i,   \
                                                    .bi_rhs_i = rhs_i})}})

#define NODE_UNARY(kind, type_i, lhs_i) \
    ((node_t){.node_kind = kind,        \
              .node_type_i = type_i,    \
              .node_n = {.node_unary = lhs_i}})

#define NODE_IF(type_i, first_tok_i, last_tok_i, node_cond_i, node_then_i, \
                node_else_i)                                               \
    ((node_t){.node_kind = NODE_IF,                                        \
              .node_type_i = type_i,                                       \
              .node_n = {.node_if = ((if_t){.if_first_tok_i = first_tok_i, \
                                            .if_last_tok_i = last_tok_i,   \
                                            .if_node_cond_i = node_cond_i, \
                                            .if_node_then_i = node_then_i, \
                                            .if_node_else_i = node_else_i})}})

#define NODE_VAR_DEF(type_i, first_tok_i, name_tok_i, last_tok_i, init_node_i, \
                     offset, flags)                                            \
    ((node_t){.node_kind = NODE_VAR_DEF,                                       \
              .node_type_i = type_i,                                           \
              .node_n = {.node_var_def = {.vd_first_tok_i = first_tok_i,       \
                                          .vd_last_tok_i = last_tok_i,         \
                                          .vd_name_tok_i = name_tok_i,         \
                                          .vd_init_node_i = init_node_i,       \
                                          .vd_stack_offset = offset,           \
                                          .vd_flags = flags}}})

#define NODE_VAR(type_i, tok_i, var_node_i)              \
    ((node_t){.node_kind = NODE_VAR,                     \
              .node_type_i = type_i,                     \
              .node_n = {.node_var = {.va_tok_i = tok_i, \
                                      .va_var_node_i = var_node_i}}})

#define NODE_WHILE(type_i, first_tok_i, last_tok_i, cond_i, body_i)    \
    ((node_t){.node_kind = NODE_WHILE,                                 \
              .node_type_i = type_i,                                   \
              .node_n = {.node_while = {.wh_first_tok_i = first_tok_i, \
                                        .wh_last_tok_i = last_tok_i,   \
                                        .wh_cond_i = cond_i,           \
                                        .wh_body_i = body_i}}})


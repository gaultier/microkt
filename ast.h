#pragma once
#include <stdbool.h>

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
    TYPE_FN,
    TYPE_CLASS,
    TYPE_PTR,
    TYPE_COUNT,
} mkt_type_kind_t;

static const char mkt_type_to_str[TYPE_COUNT][20] = {
    [TYPE_ANY] = "Any",     [TYPE_UNIT] = "Unit",   [TYPE_BOOL] = "Boolean",
    [TYPE_CHAR] = "Char",   [TYPE_BYTE] = "Byte",   [TYPE_INT] = "Int",
    [TYPE_SHORT] = "Short", [TYPE_LONG] = "Long",   [TYPE_STRING] = "String",
    [TYPE_CLASS] = "Class", [TYPE_FN] = "Function", [TYPE_PTR] = "Pointer"};

typedef struct {
    mkt_type_kind_t ty_kind;
    int ty_size, ty_class_i /* only for TYPE_CLASS */,
        ty_ptr_type_i /* only for TYPE_PTR */;
} mkt_type_t;

typedef struct {
    int bp_arg_i, bp_keyword_print_i, bp_rparen_i;
} mkt_builtin_println_t;

typedef struct {
    int bi_lhs_i, bi_rhs_i;
} mkt_binary_t;

typedef struct {
    int st_tok_i;
    bool st_multiline;
} mkt_string_t;

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
    NODE_VAR,
    NODE_ASSIGN,
    NODE_WHILE,
    NODE_FN_DECL,
    NODE_CALL,
    NODE_RETURN,
    NODE_SYSCALL,
    NODE_CLASS_DECL,
    NODE_INSTANCE,
    NODE_MEMBER,
    NODE_COUNT,
} mkt_node_kind_t;

const char mkt_node_kind_to_str[NODE_COUNT][30] = {
    [NODE_BUILTIN_PRINTLN] = "Print",
    [NODE_KEYWORD_BOOL] = "Bool",
    [NODE_STRING] = "String",
    [NODE_LONG] = "Long",
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
    [NODE_VAR] = "Var",
    [NODE_ASSIGN] = "Assign",
    [NODE_WHILE] = "While",
    [NODE_FN_DECL] = "FnDecl",
    [NODE_RETURN] = "Return",
    [NODE_CALL] = "Call",
    [NODE_SYSCALL] = "Syscall",
    [NODE_CLASS_DECL] = "Class",
    [NODE_INSTANCE] = "Instance",
    [NODE_MEMBER] = "Member",
};

typedef struct {
    long long int nu_val;
    int nu_tok_i;
} mkt_number_t;

typedef struct {
    int if_first_tok_i, if_last_tok_i, if_node_cond_i, if_node_then_i,
        if_node_else_i;
} mkt_if_t;

typedef struct {
    int bl_first_tok_i, bl_last_tok_i, *bl_nodes_i, bl_parent_scope_i;
} mkt_block_t;

static const unsigned short MKT_VAR_FLAGS_VAL = 0x1;
static const unsigned short MKT_VAR_FLAGS_VAR = 0x2;

typedef struct {
    int va_tok_i, va_var_node_i /* Node the variable refers to */, va_offset;
    unsigned short va_flags;
} mkt_var_t;

typedef struct {
    int wh_first_tok_i, wh_last_tok_i, wh_cond_i, wh_body_i;
} mkt_while_t;

static const unsigned short FN_FLAGS_SYNTHETIC = 0x1;
static const unsigned short FN_FLAGS_PUBLIC = 0x2;
static const unsigned short FN_FLAGS_PRIVATE = 0x4;
static const unsigned short FN_FLAGS_SEEN_RETURN = 0x8;

typedef struct {
    int fd_first_tok_i, fd_last_tok_i, fd_name_tok_i, fd_return_type_tok_i,
        fd_body_node_i, fd_stack_size, *fd_arg_nodes_i, fd_return_type_i;
    unsigned short fd_flags;
} mkt_fn_decl_t;

typedef struct {
    int ca_first_tok_i, ca_last_tok_i, ca_lhs_node_i, *ca_arg_nodes_i;
} mkt_call_t;

typedef struct {
    int un_first_tok_i, un_last_tok_i, un_node_i;
} unary_t;

typedef struct {
    int sy_first_tok_i, sy_last_tok_i;
    int* sy_arg_nodes_i;
} mkt_syscall_t;

static const unsigned char CLASS_FLAGS_PUBLIC = 1;

typedef struct {
    int cl_first_tok_i, cl_last_tok_i, cl_name_tok_i, cl_body_node_i,
        *cl_members;
    unsigned char cl_flags;
} mkt_class_decl_t;

typedef struct {
    int in_class, in_first_tok_i, in_last_tok_i;
} mkt_instance_t;

typedef struct {
    mkt_node_kind_t no_kind;
    int no_type_i;
    union {
        mkt_builtin_println_t no_builtin_println;  // NODE_BUILTIN_PRINTLN
        mkt_string_t no_string;                    // NODE_STRING
        mkt_number_t no_num;                       // NODE_LONG, NODE_CHAR
        mkt_binary_t no_binary;  // NODE_ADD, NODE_SUBTRACT, NODE_MULTIPLY,
        // NODE_DIVIDE, NODE_MODULO, NODE_MEMBER_GET
        unary_t no_unary;                // NODE_NOT, NODE_RETURN
        mkt_if_t no_if;                  // NODE_IF
        mkt_block_t no_block;            // NODE_BLOCK
        mkt_var_t no_var;                // NODE_VAR
        mkt_while_t no_while;            // NODE_WHILE
        mkt_fn_decl_t no_fn_decl;        // NODE_FN_DECL
        mkt_call_t no_call;              // NODE_CALL
        mkt_syscall_t no_syscall;        // NODE_SYSCALL
        mkt_class_decl_t no_class_decl;  // NODE_CLASS_DECL
        mkt_instance_t no_instance;      // NODE_INSTANCE
    } no_n;
} mkt_node_t;


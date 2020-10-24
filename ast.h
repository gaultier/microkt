#pragma once

struct node;
typedef struct node node_t;

struct builtin_print {
    node_t* arg;
};
typedef struct builtin_print builtin_print_t;

enum node_kind {
    NODE_BUILTPRINT,
    NODE_K_BOOL,
};
typedef enum node_kind node_kind_t;

struct node {
    node_kind_t kind;
    union {
        builtin_print_t builtin_print;
        int boolean;
    } n;
};

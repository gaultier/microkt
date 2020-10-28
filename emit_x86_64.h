#pragma once

#include <stdarg.h>

#include "ast.h"
#include "common.h"
#include "parse.h"

typedef enum {
    REG_RAX,
    REG_RBX,
    REG_RCX,
    REG_RDX,
    REG_RBP,
    REG_RSP,
    REG_RSI,
    REG_RDI,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
    REG_RIP,
} reg_t;

const u8 reg_t_to_str[][5] = {
    [REG_RAX] = "%rax", [REG_RBX] = "%rbx", [REG_RCX] = "%rcx",
    [REG_RDX] = "%rdx", [REG_RBP] = "%rbp", [REG_RSP] = "%rsp",
    [REG_RSI] = "%rsi", [REG_RDI] = "%rdi", [REG_R8] = "%r8",
    [REG_R9] = "%r9",   [REG_R10] = "%r10", [REG_R11] = "%r11",
    [REG_R12] = "%r12", [REG_R13] = "%r13", [REG_R14] = "%r14",
    [REG_R15] = "%r15", [REG_RIP] = "%rip"};

reg_t emit_fn_arg(u16 position) {
    switch (position) {
        case 0:
            return REG_RAX;
        case 1:
            return REG_RDI;
        case 2:
            return REG_RSI;
        case 3:
            return REG_RDX;
        case 4:
            return REG_RCX;
        case 5:
            return REG_R8;
        case 6:
            return REG_R9;
        case 7:
            return REG_R10;
        case 8:
            return REG_R11;
        default:
            UNIMPLEMENTED();
    }
}

typedef enum {
    OP_KIND_SYSCALL,
    OP_KIND_INT_LITERAL,
    OP_KIND_LABEL_ADDRESS,
    OP_KIND_STRING_LABEL,
    OP_KIND_CALLABLE_BLOCK,
    OP_KIND_ASSIGN,
    OP_KIND_REGISTER,
} emit_op_kind_t;

typedef usize emit_op_id_t;

typedef struct {
    emit_op_id_t* op_sys_args;
} emit_op_syscall_t;

typedef struct {
    usize sl_label_id;
    const u8* sl_string;
    usize sl_string_len;
} emit_op_string_label_t;

typedef struct {
    const u8* cb_name;
    usize cb_name_len;
    emit_op_id_t* cb_body;
} emit_op_callable_block_t;

typedef struct {
    emit_op_id_t as_src;
    emit_op_id_t as_dest;
} emit_op_assign_t;

typedef struct {
    emit_op_kind_t op_kind;
    union {
        emit_op_syscall_t op_syscall;
        usize op_int_literal;
        usize op_label_address;
        emit_op_string_label_t op_string_label;
        emit_op_callable_block_t op_callable_block;
        emit_op_assign_t op_assign;
        reg_t op_register;
    } op_o;
} emit_op_t;

#define OP_INT_LITERAL(n) \
    ((emit_op_t){.op_kind = OP_KIND_INT_LITERAL, .op_o = {.op_int_literal = n}})

#define OP_LABEL_ADDRESS(n)                        \
    ((emit_op_t){.op_kind = OP_KIND_LABEL_ADDRESS, \
                 .op_o = {.op_label_address = n}})

#define OP_ASSIGN(src, dest)                \
    ((emit_op_t){.op_kind = OP_KIND_ASSIGN, \
                 .op_o = {.op_assign = {.as_src = src, .as_dest = dest}}})

#define OP_REGISTER(n) \
    ((emit_op_t){.op_kind = OP_KIND_REGISTER, .op_o = {.op_register = n}})

#define OP_SYSCALL(args)                     \
    ((emit_op_t){.op_kind = OP_KIND_SYSCALL, \
                 .op_o = {.op_syscall = {.op_sys_args = args}}})

#define OP_STRING_LABEL(string, string_len, label_id)                      \
    ((emit_op_t){.op_kind = OP_KIND_STRING_LABEL,                          \
                 .op_o = {.op_string_label = {.sl_string = string,         \
                                              .sl_string_len = string_len, \
                                              .sl_label_id = label_id}}})

const usize syscall_exit_osx = (usize)0x2000001;
const usize syscall_write_osx = (usize)0x2000004;

const usize STIN = 0;
const usize STDOUT = 1;
const usize STDERR = 2;

typedef struct {
    emit_op_t* em_ops_arena;
    usize em_label_id;
    emit_op_id_t* em_text_section;
    emit_op_id_t* em_data_section;
} emit_emitter_t;

emit_emitter_t emit_emitter_init() {
    return (emit_emitter_t){.em_ops_arena = NULL, .em_label_id = 0};
}

emit_op_id_t emit_emitter_make_op(emit_emitter_t* emitter) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    buf_push(emitter->em_ops_arena, ((emit_op_t){0}));

    return buf_size(emitter->em_ops_arena) - 1;
}

emit_op_t* emit_emitter_op_get(const emit_emitter_t* emitter, emit_op_id_t id) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter->em_ops_arena, !=, NULL, "%p");

    const usize len = buf_size(emitter->em_ops_arena);
    PG_ASSERT_COND(id, <, len, "%llu");

    return &emitter->em_ops_arena[id];
}

emit_op_id_t emit_op_make_syscall(emit_emitter_t* emitter, int count, ...) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    va_list args;
    va_start(args, count);

    emit_op_id_t* syscall_args = NULL;
    buf_grow(syscall_args, count);

    for (int i = 0; i < count; i++) {
        const usize arg_id = emit_emitter_make_op(emitter);
        const usize src_id = emit_emitter_make_op(emitter);
        const usize dest_id = emit_emitter_make_op(emitter);

        *(emit_emitter_op_get(emitter, arg_id)) = OP_ASSIGN(src_id, dest_id);
        buf_push(syscall_args, arg_id);

        const emit_op_t o = va_arg(args, emit_op_t);
        *(emit_emitter_op_get(emitter, src_id)) = o;

        *(emit_emitter_op_get(emitter, dest_id)) = OP_REGISTER(emit_fn_arg(i));
    }
    va_end(args);

    const emit_op_id_t syscall_op_id = emit_emitter_make_op(emitter);
    *(emit_emitter_op_get(emitter, syscall_op_id)) = OP_SYSCALL(syscall_args);
    return syscall_op_id;
}

usize emit_add_string_label_if_not_exists(emit_emitter_t* emitter,
                                          const u8* string, usize string_len) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)string, !=, NULL, "%p");

    const usize new_label_id = emitter->em_label_id;

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t op = *(emit_emitter_op_get(emitter, op_id));
        if (op.op_kind != OP_KIND_STRING_LABEL) continue;

        const emit_op_string_label_t s = op.op_o.op_string_label;
        if (memcmp(s.sl_string, string, MIN(s.sl_string_len, string_len)) == 0)
            return s.sl_label_id;
    }

    emitter->em_label_id += 1;

    const emit_op_id_t string_label_id = emit_emitter_make_op(emitter);
    *(emit_emitter_op_get(emitter, string_label_id)) =
        OP_STRING_LABEL(string, string_len, new_label_id);

    buf_push(emitter->em_data_section, string_label_id);

    return new_label_id;
}

usize emit_node_to_string_label(const parser_t* parser, emit_emitter_t* emitter,
                                const ast_node_t* node, usize* string_len) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND((void*)string_len, !=, NULL, "%p");

    switch (node->node_kind) {
        case NODE_STRING_LITERAL:
        case NODE_KEYWORD_BOOL: {
            const u8* string = NULL;
            parser_ast_node_source(parser, node, &string, string_len);

            const usize new_label_id = emit_add_string_label_if_not_exists(
                emitter, string, *string_len);
            return new_label_id;
        }
        case NODE_INT_LITERAL: {
            // TODO
            return 0;
        }
        case NODE_BUILTIN_PRINT:
            assert(0 && "Unreachable");
    }
}

// void emit_stdlib(emit_asm_t* a) {
//     PG_ASSERT_COND((void*)a, !=, NULL, "%p");
//
//     emit_op_t* int_to_string_body = NULL;
//     buf_grow(int_to_string_body, 20);
//     /* buf_push(int_to_string_body, ); */
//
//     buf_push(a->asm_text_section,
//              ((emit_op_t){.op_kind = OP_KIND_CALLABLE_BLOCK,
//                           .op_o = {.op_callable_block = {
//                                        .cb_name = "int_to_string",
//                                        .cb_name_len =
//                                        sizeof("int_to_string"), .cb_body =
//                                        int_to_string_body,
//                                    }}}));
// }

void emit_emit(emit_emitter_t* emitter, parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_stmt_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    for (usize i = 0; i < buf_size(parser->par_stmt_nodes); i++) {
        const usize stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];

        switch (stmt->node_kind) {
            case NODE_BUILTIN_PRINT: {
                const ast_builtin_print_t builtin_print =
                    stmt->node_n.node_builtin_print;
                const ast_node_t arg =
                    parser->par_nodes[builtin_print.bp_arg_i];

                usize string_len = 0;
                const usize new_label_id = emit_node_to_string_label(
                    parser, emitter, &arg, &string_len);

                const emit_op_id_t syscall_id = emit_op_make_syscall(
                    emitter, 4, OP_INT_LITERAL(syscall_write_osx),
                    OP_INT_LITERAL(STDOUT), OP_LABEL_ADDRESS(new_label_id),
                    OP_INT_LITERAL(string_len));

                buf_push(emitter->em_text_section, syscall_id);
                break;
            }
            default:
                assert(0);  // Unreachable
        }
    }

    const emit_op_id_t syscall_id = emit_op_make_syscall(
        emitter, 2, OP_INT_LITERAL(syscall_exit_osx), OP_INT_LITERAL(0));

    buf_push(emitter->em_text_section, syscall_id);
}

void emit_asm_dump_op(const emit_emitter_t* emitter, const emit_op_t* op,
                      FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)op, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    switch (op->op_kind) {
        case OP_KIND_SYSCALL: {
            const emit_op_syscall_t syscall = op->op_o.op_syscall;
            for (usize j = 0; j < buf_size(syscall.op_sys_args); j++) {
                const emit_op_id_t arg_id = syscall.op_sys_args[j];
                const emit_op_t* const arg =
                    emit_emitter_op_get(emitter, arg_id);
                emit_asm_dump_op(emitter, arg, file);
            }
            fprintf(file, "\tsyscall\n");

            // Zero all registers after syscall
            for (usize j = 0; j < buf_size(syscall.op_sys_args); j++) {
                const reg_t reg = emit_fn_arg(j);
                fprintf(file, "\tmovq $0, %s\n", reg_t_to_str[reg]);
            }
            fprintf(file, "\n");  // For prettyness

            break;
        }
        case OP_KIND_CALLABLE_BLOCK: {
            const emit_op_callable_block_t block = op->op_o.op_callable_block;
            fprintf(file, "\n%.*s:\n", (int)block.cb_name_len, block.cb_name);

            for (usize j = 0; j < buf_size(block.cb_body); j++) {
                const emit_op_id_t body_id = block.cb_body[j];
                const emit_op_t* const body =
                    emit_emitter_op_get(emitter, body_id);
                emit_asm_dump_op(emitter, body, file);
            }

            break;
        }
        case OP_KIND_ASSIGN: {
            const emit_op_assign_t assign = op->op_o.op_assign;
            const emit_op_id_t src_id = assign.as_src;
            const emit_op_id_t dest_id = assign.as_dest;
            const emit_op_t* const src = emit_emitter_op_get(emitter, src_id);
            const emit_op_t* const dest = emit_emitter_op_get(emitter, dest_id);

            switch (src->op_kind) {
                case OP_KIND_INT_LITERAL: {
                    const usize n = src->op_o.op_int_literal;
                    switch (dest->op_kind) {
                        case OP_KIND_REGISTER: {
                            const reg_t reg = dest->op_o.op_register;
                            fprintf(file, "movq $%llu, %s\n", n,
                                    reg_t_to_str[reg]);
                            break;
                        }
                        default:
                            assert(0 && "Unreachable");
                    }
                    break;
                }
                case OP_KIND_LABEL_ADDRESS: {
                    const usize label = src->op_o.op_label_address;
                    switch (dest->op_kind) {
                        case OP_KIND_REGISTER: {
                            const reg_t reg = dest->op_o.op_register;
                            fprintf(file, "leaq .L%llu(%s), %s\n", label,
                                    reg_t_to_str[REG_RIP], reg_t_to_str[reg]);
                            break;
                        }
                        default:
                            assert(0 && "Unreachable");
                    }

                    break;
                }
                default:
                    assert(0 && "Unreachable");
            }

            break;
        }
        case OP_KIND_REGISTER:
        case OP_KIND_INT_LITERAL:
        case OP_KIND_STRING_LABEL:
        case OP_KIND_LABEL_ADDRESS:
            assert(0 && "Unreachable");
    }
}

void emit_asm_dump(const emit_emitter_t* emitter, FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    fprintf(file, "\n.data\n");

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t* const op = emit_emitter_op_get(emitter, op_id);
        switch (op->op_kind) {
            case OP_KIND_STRING_LABEL: {
                const emit_op_string_label_t s = op->op_o.op_string_label;
                fprintf(file, ".L%llu: .asciz \"%.*s\"\n", s.sl_label_id,
                        (int)s.sl_string_len, s.sl_string);
                break;
            }
            default:
                assert(0);  // Unreachable
        }
    }

    // TODO: non hardcoded main
    fprintf(file, "\n.text\n.globl _main\n_main:\n");

    for (usize i = 0; i < buf_size(emitter->em_text_section); i++) {
        const emit_op_id_t op_id = emitter->em_text_section[i];
        const emit_op_t* const op = emit_emitter_op_get(emitter, op_id);
        emit_asm_dump_op(emitter, op, file);
    }
}

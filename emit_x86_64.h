#pragma once

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
    OP_KIND_INTEGER_LITERAL,
    OP_KIND_LABEL_ADDRESS,
    OP_KIND_STRING_LABEL,
} emit_op_kind_t;

typedef struct emit_op_t emit_op_t;

typedef struct {
    emit_op_t* op_sys_args;
} emit_op_syscall_t;

typedef struct {
    usize op_sl_label_id;
    const u8* op_sl_string;
    usize op_sl_string_len;
} emit_op_string_label_t;

struct emit_op_t {
    emit_op_kind_t op_kind;
    union {
        emit_op_syscall_t op_syscall;
        usize op_integer_literal;
        usize op_label_address;
        emit_op_string_label_t op_string_label;
    } op_o;
};

typedef struct {
    emit_op_t* asm_text_section;
    emit_op_t* asm_data_section;
} emit_asm_t;

const usize syscall_exit_osx = (usize)0x2000001;
const usize syscall_write_osx = (usize)0x2000004;

const usize STIN = 0;
const usize STDOUT = 1;
const usize STDERR = 2;

// TODO: if not exists
usize emit_add_string_label(emit_op_t** data_section, const u8* string,
                            usize string_len, usize* label_id) {
    const usize new_label_id = *label_id;
    *label_id += 1;

    buf_push(*data_section,
             ((emit_op_t){.op_kind = OP_KIND_STRING_LABEL,
                          .op_o = {.op_string_label = {
                                       .op_sl_string = string,
                                       .op_sl_string_len = string_len,
                                       .op_sl_label_id = new_label_id}}}));

    return new_label_id;
}

void emit_emit(parser_t* parser, emit_asm_t* a) {
    PG_ASSERT_COND(parser, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND(parser->par_stmt_nodes, !=, NULL, "%p");
    PG_ASSERT_COND(a, !=, NULL, "%p");

    emit_op_t* text_section = NULL;
    emit_op_t* data_section = NULL;

    usize label_id = 0;

    for (usize i = 0; i < buf_size(parser->par_stmt_nodes); i++) {
        const usize stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];

        switch (stmt->node_kind) {
            case NODE_BUILTIN_PRINT: {
                const ast_builtin_print_t builtin_print =
                    stmt->node_n.node_builtin_print;
                const ast_node_t arg =
                    parser->par_nodes[builtin_print.bp_arg_i];

                const u8* string = NULL;
                usize string_len = 0;
                parser_ast_node_source(parser, &arg, &string, &string_len);

                const usize new_label_id = emit_add_string_label(
                    &data_section, string, string_len, &label_id);

                emit_op_t* args = NULL;
                buf_grow(args, 4);
                buf_push(args, ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                            .op_o = {.op_integer_literal =
                                                         syscall_write_osx}}));
                buf_push(args,
                         ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                      .op_o = {.op_integer_literal = STDOUT}}));
                buf_push(
                    args,
                    ((emit_op_t){.op_kind = OP_KIND_LABEL_ADDRESS,
                                 .op_o = {.op_label_address = new_label_id}}));
                buf_push(
                    args,
                    ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                 .op_o = {.op_integer_literal = string_len}}));

                const emit_op_t syscall = {
                    .op_kind = OP_KIND_SYSCALL,
                    .op_o = {.op_syscall = {.op_sys_args = args}}};
                buf_push(text_section, syscall);
                break;
            }
            default:
                abort();  // Unreachable
        }
    }

    emit_op_t* args = NULL;
    buf_grow(args, 4);
    buf_push(args,
             ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                          .op_o = {.op_integer_literal = syscall_exit_osx}}));
    buf_push(args, ((emit_op_t){.op_kind = OP_KIND_INTEGER_LITERAL,
                                .op_o = {.op_integer_literal = 0}}));

    const emit_op_t syscall = {.op_kind = OP_KIND_SYSCALL,
                               .op_o = {.op_syscall = {.op_sys_args = args}}};
    buf_push(text_section, syscall);

    *a = (emit_asm_t){.asm_text_section = text_section,
                      .asm_data_section = data_section};
}

void emit_asm_dump(emit_asm_t* a, FILE* file) {
    PG_ASSERT_COND(a, !=, NULL, "%p");
    PG_ASSERT_COND(file, !=, NULL, "%p");

    fprintf(file, ".data\n");

    for (usize i = 0; i < buf_size(a->asm_data_section); i++) {
        const emit_op_t op = a->asm_data_section[i];
        switch (op.op_kind) {
            case OP_KIND_STRING_LABEL: {
                const emit_op_string_label_t s = op.op_o.op_string_label;
                fprintf(file, ".L%llu: .asciz \"%.*s\"\n", s.op_sl_label_id,
                        (int)s.op_sl_string_len, s.op_sl_string);
                break;
            }
            default:
                abort();  // Unreachable
        }
    }

    // TODO: non hardcoded main
    fprintf(file, "\n.text\n.globl _main\n_main:\n");

    for (usize i = 0; i < buf_size(a->asm_text_section); i++) {
        const emit_op_t op = a->asm_text_section[i];
        switch (op.op_kind) {
            case OP_KIND_SYSCALL: {
                const emit_op_syscall_t syscall = op.op_o.op_syscall;
                for (usize j = 0; j < buf_size(syscall.op_sys_args); j++) {
                    const emit_op_t arg = syscall.op_sys_args[i];
                    const reg_t reg = emit_fn_arg(i);

                    switch (arg.op_kind) {
                        case OP_KIND_INTEGER_LITERAL: {
                            fprintf(file, "\tmovq $%llu, %s\n",
                                    arg.op_o.op_integer_literal,
                                    reg_t_to_str[reg]);
                            break;
                        }
                        case OP_KIND_LABEL_ADDRESS: {
                            fprintf(
                                file, "\tleaq .L%llu(%s), %s\n",
                                arg.op_o.op_label_address,
                                reg_t_to_str[REG_RIP],  // TODO: understand why
                                reg_t_to_str[reg]);
                            break;
                        }
                        default:
                            abort();  // Unreachable
                    }
                }
                fprintf(file, "\tsyscall\n");

                // Zero all registers after syscall
                for (usize j = 0; j < buf_size(syscall.op_sys_args); j++) {
                    const reg_t reg = emit_fn_arg(i);
                    fprintf(file, "\tmovq $0, %s\n", reg_t_to_str[reg]);
                }
                fprintf(file, "\n");  // For prettyness

                break;
            }
            default:
                abort();  // Unreachable
        }
    }
}

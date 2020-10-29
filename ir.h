#pragma once

#include "common.h"

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
    OP_KIND_CALL_SYSCALL,
    OP_KIND_INT_LITERAL,
    OP_KIND_LABEL_ADDRESS,
    OP_KIND_STRING_LABEL,
    OP_KIND_CALLABLE_BLOCK,
    OP_KIND_ASSIGN,
    OP_KIND_REGISTER,
    OP_KIND_RET,
    OP_KIND_INT_ADD,
    OP_KIND_ASM,
} emit_op_kind_t;

typedef usize emit_op_id_t;

typedef struct {
    emit_op_id_t* sc_instructions;
} emit_op_call_syscall_t;

typedef struct {
    usize sl_label_id;
    const u8* sl_string;
    usize sl_string_len;
} emit_op_string_label_t;

#define CALLABLE_BLOCK_FLAG_DEFAULT 0
#define CALLABLE_BLOCK_FLAG_GLOBAL 1
typedef struct {
    const u8* cb_name;
    usize cb_name_len;
    emit_op_id_t* cb_body;
    u16 cb_flags;
} emit_op_callable_block_t;

typedef struct {
    emit_op_id_t pa_src;
    emit_op_id_t pa_dst;
} emit_op_pair_t;

typedef struct {
    emit_op_kind_t op_kind;
    union {
        emit_op_call_syscall_t op_call_syscall;      // OP_KIND_SYSCALL
        usize op_int_literal;                        // OP_KIND_INT_LITERAL
        usize op_label_address;                      // OP_KIND_LABEL_ADDRESS
        emit_op_string_label_t op_string_label;      // OP_KIND_STRING_LABEL
        emit_op_callable_block_t op_callable_block;  // OP_KIND_CALLABLE_BLOCK
        emit_op_pair_t op_assign;                    // OP_KIND_ASSIGN
        reg_t op_register;                           // OP_KIND_REGISTER
        emit_op_pair_t op_int_add;                   // OP_KIND_INT_ADD
        const u8* op_asm0;                           // OP_KIND_ASM
    } op_o;
} emit_op_t;

#define OP_SYSCALL() ((emit_op_t){.op_kind = OP_KIND_SYSCALL})
#define OP_RET() ((emit_op_t){.op_kind = OP_KIND_RET})

#define OP_INT_LITERAL(n) \
    ((emit_op_t){.op_kind = OP_KIND_INT_LITERAL, .op_o = {.op_int_literal = n}})

#define OP_LABEL_ADDRESS(n)                        \
    ((emit_op_t){.op_kind = OP_KIND_LABEL_ADDRESS, \
                 .op_o = {.op_label_address = n}})

#define OP_ASSIGN(src, dst)                 \
    ((emit_op_t){.op_kind = OP_KIND_ASSIGN, \
                 .op_o = {.op_assign = {.pa_src = src, .pa_dst = dst}}})

#define OP_REGISTER(n) \
    ((emit_op_t){.op_kind = OP_KIND_REGISTER, .op_o = {.op_register = n}})

#define OP_CALL_SYSCALL(instructions)    \
    ((emit_op_t){                        \
        .op_kind = OP_KIND_CALL_SYSCALL, \
        .op_o = {.op_call_syscall = {.sc_instructions = instructions}}})

#define OP_STRING_LABEL(string, string_len, label_id)                      \
    ((emit_op_t){.op_kind = OP_KIND_STRING_LABEL,                          \
                 .op_o = {.op_string_label = {.sl_string = string,         \
                                              .sl_string_len = string_len, \
                                              .sl_label_id = label_id}}})

#define OP_CALLABLE_BLOCK(name, name_len, body, flags)                   \
    ((emit_op_t){.op_kind = OP_KIND_CALLABLE_BLOCK,                      \
                 .op_o = {.op_callable_block = {.cb_name = name,         \
                                                .cb_name_len = name_len, \
                                                .cb_body = body,         \
                                                .cb_flags = flags}}})

#define OP_INT_ADD(src, dst)                 \
    ((emit_op_t){.op_kind = OP_KIND_INT_ADD, \
                 .op_o = {.op_int_add = {.pa_src = src, .pa_dst = dst}}})

#define OP_ASM(asm0) \
    ((emit_op_t){.op_kind = OP_KIND_ASM, .op_o = {.op_asm0 = asm0}})

#define OP(emitter, op) (emit_make_op_with(emitter, op))


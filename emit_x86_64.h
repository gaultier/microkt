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

const u8 reg_t_to_str[][3] = {
    [REG_RAX] = "rax", [REG_RBX] = "rbx", [REG_RCX] = "rcx", [REG_RDX] = "rdx",
    [REG_RBP] = "rbp", [REG_RSP] = "rsp", [REG_RSI] = "rsi", [REG_RDI] = "rdi",
    [REG_R8] = "r8",   [REG_R9] = "r9",   [REG_R10] = "r10", [REG_R11] = "r11",
    [REG_R12] = "r12", [REG_R13] = "r13", [REG_R14] = "r14", [REG_R15] = "r15",
    [REG_RIP] = "rip"};

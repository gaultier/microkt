#pragma once

#include <stdarg.h>

#include "ir.h"
#include "parse.h"

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
} emit_t;

emit_op_t* emit_op_get(const emit_t* emitter, emit_op_id_t id) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter->em_ops_arena, !=, NULL, "%p");

    const usize len = buf_size(emitter->em_ops_arena);
    PG_ASSERT_COND(id, <, len, "%llu");

    return &emitter->em_ops_arena[id];
}

emit_op_id_t emit_make_op_with(emit_t* emitter, emit_op_t op) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    buf_push(emitter->em_ops_arena, ((emit_op_t){0}));

    const usize op_id = buf_size(emitter->em_ops_arena) - 1;
    *(emit_op_get(emitter, op_id)) = op;

    return op_id;
}

emit_op_id_t emit_zero_register(emit_t* emitter, reg_t reg) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    const emit_op_id_t r = OP(emitter, OP_REGISTER(reg));
    const emit_op_id_t zero = OP(emitter, OP_INT_LITERAL(0));
    return OP(emitter, OP_ASSIGN(zero, r));
}

emit_op_id_t emit_op_callable_block(emit_t* emitter, const u8* name,
                                    usize name_len, u16 flags, int count, ...) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    va_list args;
    va_start(args, count);

    emit_op_id_t* body = NULL;
    buf_grow(body, count);

    for (int i = 0; i < count; i++) {
        const emit_op_id_t o = va_arg(args, emit_op_id_t);
        buf_push(body, o);
    }
    va_end(args);

    return OP(emitter, OP_CALLABLE_BLOCK(name, name_len, body, flags));
}

void emit_stdlib(emit_t* emitter) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    const emit_op_id_t int_to_string_end = emit_op_callable_block(
        emitter, "int_to_string_end", sizeof("int_to_string_end"),
        CALLABLE_BLOCK_FLAG_DEFAULT, 1, OP(emitter, OP_RET()));

    const emit_op_id_t int_to_string_loop = emit_op_callable_block(
        emitter, "int_to_string_loop", sizeof("int_to_string_loop"),
        CALLABLE_BLOCK_FLAG_DEFAULT, 0);

    const emit_op_id_t int_to_string = emit_op_callable_block(
        emitter, "int_to_string", sizeof("int_to_string"),
        CALLABLE_BLOCK_FLAG_DEFAULT, 4, emit_zero_register(emitter, REG_R8),
        OP(emitter, OP_INT_ADD(OP(emitter, OP_INT_LITERAL(3)),
                               OP(emitter, OP_REGISTER(REG_R12)))),
        int_to_string_loop, int_to_string_end);

    buf_push(emitter->em_text_section, int_to_string);
}

emit_t emit_init() {
    emit_t emitter = {.em_ops_arena = NULL, .em_label_id = 0};
    buf_grow(emitter.em_ops_arena, 100);
    buf_grow(emitter.em_data_section, 100);
    buf_grow(emitter.em_text_section, 100);

    emit_stdlib(&emitter);

    const emit_op_id_t main =
        OP(&emitter, OP_CALLABLE_BLOCK("_main", sizeof("main"), NULL,
                                       CALLABLE_BLOCK_FLAG_GLOBAL));
    buf_push(emitter.em_text_section, main);

    return emitter;
}

emit_op_id_t emit_op_make_call_syscall(emit_t* emitter, int count, ...) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    va_list args;
    va_start(args, count);

    emit_op_id_t* syscall_instructions = NULL;
    buf_grow(syscall_instructions, count);

    for (int i = 0; i < count; i++) {
        const emit_op_id_t src = va_arg(args, emit_op_id_t);
        const emit_op_id_t dst = OP(emitter, OP_REGISTER(emit_fn_arg(i)));
        const emit_op_id_t assign = OP(emitter, OP_ASSIGN(src, dst));
        buf_push(syscall_instructions, assign);
    }
    va_end(args);

    buf_push(syscall_instructions, OP(emitter, OP_SYSCALL()));

    // Zero all registers after call_syscall
    for (int i = 0; i < count; i++) {
        buf_push(syscall_instructions,
                 emit_zero_register(emitter, emit_fn_arg(i)));
    }

    return OP(emitter, OP_CALL_SYSCALL(syscall_instructions));
}

usize emit_add_string_label_if_not_exists(emit_t* emitter, const u8* string,
                                          usize string_len) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)string, !=, NULL, "%p");

    const usize new_label_id = emitter->em_label_id;

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t op = *(emit_op_get(emitter, op_id));
        if (op.op_kind != OP_KIND_STRING_LABEL) continue;

        const emit_op_string_label_t s = op.op_o.op_string_label;
        if (memcmp(s.sl_string, string, MIN(s.sl_string_len, string_len)) ==
            0)  // Found
            return s.sl_label_id;
    }

    emitter->em_label_id += 1;

    const emit_op_id_t string_label =
        OP(emitter, OP_STRING_LABEL(string, string_len, new_label_id));

    buf_push(emitter->em_data_section, string_label);

    return new_label_id;
}

usize emit_node_to_string_label(const parser_t* parser, emit_t* emitter,
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

void emit_emit(emit_t* emitter, const parser_t* parser) {
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

                const emit_op_id_t call_syscall_id = emit_op_make_call_syscall(
                    emitter, 4, OP(emitter, OP_INT_LITERAL(syscall_write_osx)),
                    OP(emitter, OP_INT_LITERAL(STDOUT)),
                    OP(emitter, OP_LABEL_ADDRESS(new_label_id)),
                    OP(emitter, OP_INT_LITERAL(string_len)));

                buf_push(emitter->em_text_section, call_syscall_id);
                break;
            }
            default:
                assert(0);  // Unreachable
        }
    }

    const emit_op_id_t call_syscall_id = emit_op_make_call_syscall(
        emitter, 2, OP(emitter, OP_INT_LITERAL(syscall_exit_osx)),
        OP(emitter, OP_INT_LITERAL(0)));

    buf_push(emitter->em_text_section, call_syscall_id);
}

void emit_asm_dump_op(const emit_t* emitter, const emit_op_id_t op_id,
                      FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    const emit_op_t* const op = emit_op_get(emitter, op_id);

    switch (op->op_kind) {
        case OP_KIND_CALL_SYSCALL: {
            fprintf(file, "\n");

            const emit_op_call_syscall_t call_syscall =
                op->op_o.op_call_syscall;
            for (usize j = 0; j < buf_size(call_syscall.sc_instructions); j++) {
                const emit_op_id_t arg_id = call_syscall.sc_instructions[j];
                emit_asm_dump_op(emitter, arg_id, file);
            }

            fprintf(file, "\n");
            break;
        }
        case OP_KIND_CALLABLE_BLOCK: {
            fprintf(file, "\n");

            const emit_op_callable_block_t block = op->op_o.op_callable_block;
            if (block.cb_flags & CALLABLE_BLOCK_FLAG_GLOBAL)
                fprintf(file, ".global %.*s\n", (int)block.cb_name_len,
                        block.cb_name);

            fprintf(file, "%.*s:\n", (int)block.cb_name_len, block.cb_name);

            for (usize j = 0; j < buf_size(block.cb_body); j++) {
                const emit_op_id_t body_id = block.cb_body[j];
                emit_asm_dump_op(emitter, body_id, file);
            }
            fprintf(file, "\n");

            break;
        }
        case OP_KIND_INT_ADD: {
            const emit_op_pair_t assign = op->op_o.op_assign;
            const emit_op_id_t src = assign.pa_src;
            const emit_op_id_t dst = assign.pa_dst;

            fprintf(file, "addq ");
            emit_asm_dump_op(emitter, src, file);
            fprintf(file, ", ");
            emit_asm_dump_op(emitter, dst, file);
            fprintf(file, "\n");
            break;
        }
        case OP_KIND_ASSIGN: {
            const emit_op_pair_t assign = op->op_o.op_assign;
            const emit_op_id_t src_id = assign.pa_src;
            const emit_op_id_t dst_id = assign.pa_dst;
            const emit_op_t* const src = emit_op_get(emitter, src_id);
            const emit_op_t* const dst = emit_op_get(emitter, dst_id);

            switch (src->op_kind) {
                case OP_KIND_INT_LITERAL: {
                    const usize n = src->op_o.op_int_literal;
                    switch (dst->op_kind) {
                        case OP_KIND_REGISTER: {
                            const reg_t reg = dst->op_o.op_register;
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
                    switch (dst->op_kind) {
                        case OP_KIND_REGISTER: {
                            const reg_t reg = dst->op_o.op_register;
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
        case OP_KIND_SYSCALL: {
            fprintf(file, "syscall\n");
            break;
        }
        case OP_KIND_RET: {
            fprintf(file, "ret\n");
            break;
        }
        case OP_KIND_REGISTER: {
            fprintf(file, "%s ", reg_t_to_str[op->op_o.op_register]);
            break;
        }
        case OP_KIND_INT_LITERAL: {
            fprintf(file, "%lld ", op->op_o.op_int_literal);
            break;
        }
        case OP_KIND_STRING_LABEL:
        case OP_KIND_LABEL_ADDRESS:
            assert(0 && "Unreachable");
    }
}

void emit_asm_dump(const emit_t* emitter, FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    fprintf(file, "\n.data\n");

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t* const op = emit_op_get(emitter, op_id);
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

    fprintf(file, "\n.text\n");

    for (usize i = 0; i < buf_size(emitter->em_text_section); i++) {
        const emit_op_id_t op_id = emitter->em_text_section[i];
        emit_asm_dump_op(emitter, op_id, file);
    }
}

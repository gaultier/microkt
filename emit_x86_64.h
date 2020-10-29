#pragma once

#include <stdarg.h>

#include "ir.h"
#include "macos_x86_64_stdlib.h"
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

emit_t emit_init() {
    emit_t emitter = {.em_ops_arena = NULL, .em_label_id = 0};
    buf_grow(emitter.em_ops_arena, 100);
    buf_grow(emitter.em_data_section, 100);
    buf_grow(emitter.em_text_section, 100);

    const emit_op_id_t main =
        OP(&emitter, OP_CALLABLE_BLOCK("_main", sizeof("main"), NULL,
                                       CALLABLE_BLOCK_FLAG_GLOBAL));
    buf_push(emitter.em_text_section, main);

    return emitter;
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
            /* const usize n = 42;  // FIXME */
            /* emit_op_id_t* instructions = NULL; */
            /* buf_push(instructions, */
            /*          OP(emitter, OP_ASSIGN(OP(emitter, OP_INT_LITERAL(n)), */
            /*                                OP(emitter,
             * OP_REGISTER(REG_RAX))))); */

            /* buf_push(instructions, OP(emitter, OP_ASM("call"))); */

            /* return OP(emitter, OP_CALL(instructions)); */
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

                emit_op_id_t* call_args = NULL;
                buf_push(
                    call_args,
                    OP(emitter,
                       OP_ASSIGN(OP(emitter, OP_LABEL_ADDRESS(new_label_id)),
                                 OP(emitter, OP_REGISTER(emit_fn_arg(0))))));

                buf_push(
                    call_args,
                    OP(emitter,
                       OP_ASSIGN(OP(emitter, OP_INT_LITERAL(string_len)),
                                 OP(emitter, OP_REGISTER(emit_fn_arg(1))))));
                const emit_op_id_t call_id =
                    OP(emitter, OP_CALL("print", sizeof("print"), call_args));

                buf_push(emitter->em_text_section, call_id);
                break;
            }
            default:
                assert(0);  // Unreachable
        }
    }

    buf_push(emitter->em_text_section,
             OP(emitter, OP_CALL("exit_ok", sizeof("exit_ok"), NULL)));
}

void emit_asm_dump_op(const emit_t* emitter, const emit_op_id_t op_id,
                      FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    const emit_op_t* const op = emit_op_get(emitter, op_id);

    switch (op->op_kind) {
        case OP_KIND_CALL: {
            fprintf(file, "\n");

            const emit_op_call_t call = op->op_o.op_call;
            for (usize j = 0; j < buf_size(call.sc_instructions); j++) {
                const emit_op_id_t arg_id = call.sc_instructions[j];
                emit_asm_dump_op(emitter, arg_id, file);
            }

            fprintf(file, "call %.*s\n", (int)(call.sc_name_len), call.sc_name);
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
        case OP_KIND_LABEL_ADDRESS: {
            fprintf(file, ".L%lld(%s) ", op->op_o.op_int_literal,
                    reg_t_to_str[REG_RSI]);
            break;
        }
        case OP_KIND_STRING_LABEL:
            assert(0 && "Unreachable");
    }
}

void emit_asm_dump(const emit_t* emitter, FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    fprintf(file, "%s\n.data\n", stdlib);

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

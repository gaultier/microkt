#pragma once

#include "common.h"
#include "ir.h"
#include "macos_x86_64_stdlib.h"
#include "parse.h"

typedef struct {
    emit_op_t* em_ops_arena;
    usize em_label_id;
    emit_op_id_t* em_text_section;
    emit_op_id_t* em_data_section;
    emit_op_id_t em_current_block;
} emit_t;

static emit_op_t* emit_op_get(const emit_t* emitter, emit_op_id_t id) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter->em_ops_arena, !=, NULL, "%p");

    const usize len = buf_size(emitter->em_ops_arena);
    PG_ASSERT_COND(id, <, len, "%llu");

    return &emitter->em_ops_arena[id];
}

static emit_op_id_t emit_add_to_current_block(emit_t* emitter,
                                              emit_op_id_t op) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    emit_op_t* const current = emit_op_get(emitter, emitter->em_current_block);

    PG_ASSERT_COND((void*)current, !=, NULL, "%p");
    PG_ASSERT_COND(current->op_kind, ==, OP_KIND_CALLABLE_BLOCK, "%d");

    buf_push(current->op_o.op_callable_block.cb_body, op);

    return op;
}

static emit_op_id_t emit_make_op_with(emit_t* emitter, emit_op_t op) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    buf_push(emitter->em_ops_arena, ((emit_op_t){0}));

    const usize op_id = buf_size(emitter->em_ops_arena) - 1;
    *(emit_op_get(emitter, op_id)) = op;

    return op_id;
}

static emit_t emit_init() {
    emit_t emitter = {.em_ops_arena = NULL, .em_label_id = 0};
    buf_grow(emitter.em_ops_arena, 1);
    buf_grow(emitter.em_data_section, 1);
    buf_grow(emitter.em_text_section, 1);

    emit_op_id_t* main_body = NULL;
    buf_grow(main_body, 1);
    const emit_op_id_t main =
        OP(&emitter, OP_CALLABLE_BLOCK("_main", sizeof("main"), main_body,
                                       CALLABLE_BLOCK_FLAG_GLOBAL));
    buf_push(emitter.em_text_section, main);

    emitter.em_current_block = main;

    return emitter;
}

static usize emit_add_string_label_if_not_exists(emit_t* emitter,
                                                 const u8* string,
                                                 usize string_len) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)string, !=, NULL, "%p");

    const usize new_label_id = emitter->em_label_id;

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t op = *(emit_op_get(emitter, op_id));
        if (op.op_kind != OP_KIND_STRING_LABEL) continue;

        const emit_op_string_label_t s = AS_STRING_LABEL(op);
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

static usize emit_node_to_string_label(const parser_t* parser, emit_t* emitter,
                                       const ast_node_t* node,
                                       usize* string_len) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)node, !=, NULL, "%p");
    PG_ASSERT_COND((void*)string_len, !=, NULL, "%p");
    PG_ASSERT_COND(
        node->node_kind == NODE_STRING || node->node_kind == NODE_KEYWORD_BOOL,
        ==, 1, "%d");

    const u8* string = NULL;
    parser_ast_node_source(parser, node, &string, string_len);

    const usize new_label_id =
        emit_add_string_label_if_not_exists(emitter, string, *string_len);
    return new_label_id;
}

static void emit_call_print_integer(emit_t* emitter, emit_op_id_t arg_id) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    const emit_op_t arg = *(emit_op_get(emitter, arg_id));
    PG_ASSERT_COND(arg.op_kind, ==, OP_KIND_INT, "%d");

    emit_op_id_t* print_int_args = NULL;
    buf_push(print_int_args,
             OP(emitter,
                OP_ASSIGN(arg_id, OP(emitter, OP_REGISTER(emit_fn_arg(0))))));

    emit_add_to_current_block(
        emitter,
        OP(emitter, OP_CALL("print_int", sizeof("print_int"), print_int_args)));
}

static void emit_call_print_char(emit_t* emitter, emit_op_id_t arg_id) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    const emit_op_t arg = *(emit_op_get(emitter, arg_id));
    PG_ASSERT_COND(arg.op_kind, ==, OP_KIND_INT, "%d");

    emit_op_id_t* print_int_args = NULL;
    buf_push(print_int_args,
             OP(emitter,
                OP_ASSIGN(arg_id, OP(emitter, OP_REGISTER(emit_fn_arg(0))))));

    emit_add_to_current_block(
        emitter, OP(emitter, OP_CALL("print_char", sizeof("print_char"),
                                     print_int_args)));
}

static void emit_call_print_string(emit_t* emitter, usize label_id,
                                   usize string_len) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    emit_op_id_t* call_args = NULL;
    buf_push(call_args,
             OP(emitter, OP_ASSIGN(OP(emitter, OP_LABEL_ID(label_id)),
                                   OP(emitter, OP_REGISTER(emit_fn_arg(0))))));

    buf_push(call_args,
             OP(emitter, OP_ASSIGN(OP(emitter, OP_INT(string_len)),
                                   OP(emitter, OP_REGISTER(emit_fn_arg(1))))));

    emit_add_to_current_block(
        emitter, OP(emitter, OP_CALL("print", sizeof("print"), call_args)));
}

static void emit_emit(emit_t* emitter, const parser_t* parser) {
    PG_ASSERT_COND((void*)parser, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)parser->par_stmt_nodes, !=, NULL, "%p");
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");

    for (usize i = 0; i < buf_size(parser->par_stmt_nodes); i++) {
        const usize stmt_i = parser->par_stmt_nodes[i];
        const ast_node_t* stmt = &parser->par_nodes[stmt_i];

        switch (stmt->node_kind) {
            case NODE_BUILTIN_PRINT: {
                const ast_builtin_print_t builtin_print = AS_PRINT(*stmt);
                const ast_node_t arg =
                    parser->par_nodes[builtin_print.bp_arg_i];

                if (arg.node_kind == NODE_KEYWORD_BOOL ||
                    arg.node_kind == NODE_STRING) {
                    usize string_len = 0;

                    const usize label_id = emit_node_to_string_label(
                        parser, emitter, &arg, &string_len);

                    emit_call_print_string(emitter, label_id, string_len);
                } else if (arg.node_kind == NODE_INT) {
                    const isize n = parse_node_to_int(parser, &arg);
                    emit_call_print_integer(emitter, OP(emitter, OP_INT(n)));
                } else if (arg.node_kind == NODE_CHAR) {
                    const char n = parse_node_to_char(parser, &arg);
                    emit_call_print_char(emitter, OP(emitter, OP_INT(n)));
                } else {
                    UNREACHABLE();
                }

                break;
            }
            case NODE_INT:
            case NODE_CHAR:
            case NODE_STRING:
            case NODE_KEYWORD_BOOL:
                UNREACHABLE();
        }
    }
}

static void emit_asm_dump_op(const emit_t* emitter, const emit_op_id_t op_id,
                             FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    const emit_op_t* const op = emit_op_get(emitter, op_id);

    switch (op->op_kind) {
        case OP_KIND_CALL: {
            fprintf(file, "\n");

            const emit_op_call_t call = AS_CALL(*op);
            for (usize j = 0; j < buf_size(call.sc_instructions); j++) {
                const emit_op_id_t arg_id = call.sc_instructions[j];
                emit_asm_dump_op(emitter, arg_id, file);
            }

            fprintf(file, "call %.*s\n", (int)(call.sc_name_len), call.sc_name);
            break;
        }
        case OP_KIND_CALLABLE_BLOCK: {
            fprintf(file, "\n");

            const emit_op_callable_block_t block = AS_CALLABLE_BLOCK(*op);
            if (block.cb_flags & CALLABLE_BLOCK_FLAG_GLOBAL)
                fprintf(file, ".global %.*s\n", (int)block.cb_name_len,
                        block.cb_name);

            fprintf(file, "%.*s:\n", (int)block.cb_name_len, block.cb_name);

            for (usize j = 0; j < buf_size(block.cb_body); j++) {
                const emit_op_id_t body_id = block.cb_body[j];
                emit_asm_dump_op(emitter, body_id, file);
            }
            fprintf(file, "\nret\n");

            break;
        }
        case OP_KIND_ASSIGN: {
            const emit_op_pair_t assign = AS_ASSIGN(*op);
            const emit_op_id_t src_id = assign.pa_src;
            const emit_op_id_t dst_id = assign.pa_dst;
            const emit_op_t* const src = emit_op_get(emitter, src_id);

            switch (src->op_kind) {
                case OP_KIND_INT: {
                    fprintf(file, "movq ");
                    emit_asm_dump_op(emitter, src_id, file);
                    fprintf(file, ", ");
                    emit_asm_dump_op(emitter, dst_id, file);
                    fprintf(file, "\n");
                    break;
                }
                case OP_KIND_LABEL_ID:
                case OP_KIND_PTR: {
                    fprintf(file, "leaq ");
                    emit_asm_dump_op(emitter, src_id, file);
                    fprintf(file, ", ");
                    emit_asm_dump_op(emitter, dst_id, file);
                    fprintf(file, "\n");

                    break;
                }
                default:
                    UNREACHABLE();
            }

            break;
        }
        case OP_KIND_REGISTER: {
            fprintf(file, "%s ", reg_t_to_str[AS_REGISTER(*op)]);
            break;
        }
        case OP_KIND_INT: {
            fprintf(file, "$%lld ", AS_INT(*op));
            break;
        }
        case OP_KIND_LABEL_ID: {
            fprintf(file, ".L%lld(%s) ", AS_INT(*op), reg_t_to_str[REG_RIP]);
            break;
        }
        case OP_KIND_PTR: {
            emit_op_ptr_t p = AS_PTR(*op);
            fprintf(file, "%.*s+%lld(%s) ", (int)p.pt_name_len, p.pt_name,
                    p.pt_offset, reg_t_to_str[REG_RIP]);
            break;
        }
        case OP_KIND_STRING_LABEL:
            UNREACHABLE();
    }
}

static void emit_asm_dump(const emit_t* emitter, FILE* file) {
    PG_ASSERT_COND((void*)emitter, !=, NULL, "%p");
    PG_ASSERT_COND((void*)file, !=, NULL, "%p");

    fprintf(file, "%s\n.data\n", stdlib);

    for (usize i = 0; i < buf_size(emitter->em_data_section); i++) {
        const emit_op_id_t op_id = emitter->em_data_section[i];
        const emit_op_t* const op = emit_op_get(emitter, op_id);
        switch (op->op_kind) {
            case OP_KIND_STRING_LABEL: {
                const emit_op_string_label_t s = AS_STRING_LABEL(*op);
                fprintf(file, ".L%llu: .ascii \"%.*s\"\n", s.sl_label_id,
                        (int)s.sl_string_len, s.sl_string);
                break;
            }
            default:
                UNREACHABLE();
        }
    }

    fprintf(file, "\n.text\n");

    for (usize i = 0; i < buf_size(emitter->em_text_section); i++) {
        const emit_op_id_t op_id = emitter->em_text_section[i];
        emit_asm_dump_op(emitter, op_id, file);
    }
}

#pragma once

#include "common.h"
#include "lex.h"

typedef struct {
    lex_token_id_t* par_token_ids;
    usize par_token_ids_len;
    const u8* par_source;
    const usize par_source_len;
    const u8* par_file_name0;
    usize par_tok_i;
} parser_t;

parser_t parser_init(const u8* file_name0, const u8* source, usize source_len) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");
    PG_ASSERT_COND(source, !=, NULL, "%p");

    lex_t lex = lex_init(source, source_len);

    usize token_ids_len = 0;
    usize token_ids_capacity = source_len / 8;  // Estimate
    lex_token_id_t* token_ids =
        realloc(NULL, token_ids_capacity * sizeof(lex_token_id_t));
    PG_ASSERT_COND(token_ids, !=, NULL, "%p");

    while (1) {
        const token_t token = lex_next(&lex);
        token_dump(&token);

        if (token_ids_len == token_ids_capacity) {
            token_ids_capacity *= 2;
            token_ids =
                realloc(token_ids, token_ids_capacity * sizeof(lex_token_id_t));
            PG_ASSERT_COND(token_ids, !=, NULL, "%p");
        }
        token_ids[token_ids_len++] = token.tok_id;

        if (token.tok_id == LEX_TOKEN_ID_EOF) break;
    }

    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len,
                      .par_token_ids = token_ids,
                      .par_token_ids_len = token_ids_len,
                      .par_tok_i = 0};
}

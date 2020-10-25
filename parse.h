#pragma once

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
    return (parser_t){.par_file_name0 = file_name0,
                      .par_source = source,
                      .par_source_len = source_len};
}

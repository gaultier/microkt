#pragma once

#include "common.h"

typedef enum {
    LEX_TOKEN_ID_BUILTIN_PRINT,
    LEX_TOKEN_ID_LPAREN,
    LEX_TOKEN_ID_RPAREN,
    LEX_TOKEN_ID_TRUE,
    LEX_TOKEN_ID_FALSE,
    LEX_TOKEN_ID_EOF,
    LEX_TOKEN_ID_INVALID,
} lex_token_id_t;

typedef struct {
    usize loc_start, loc_end;
} loc_t;

typedef struct {
    lex_token_id_t tok_id;
    loc_t tok_loc;
} token_t;

typedef struct {
    lex_token_id_t key_id;
    const u8 key_str[20];
} keyword_t;

static const keyword_t keywords[] = {
    {.key_id = LEX_TOKEN_ID_TRUE, .key_str = "true"},
    {.key_id = LEX_TOKEN_ID_FALSE, .key_str = "false"},
    {.key_id = LEX_TOKEN_ID_BUILTIN_PRINT, .key_str = "print"},
};

typedef struct {
    const u8* lex_source;
    usize lex_index;
} lex_t;

typedef enum {
    LEX_STATE_START,
    LEX_STATE_IDENTIFIER,
} lex_state_t;

lex_t lex_init(const u8* source);
lex_t lex_init(const u8* source) {
    return (lex_t){.lex_source = source, .lex_index = 0};
}

token_t lex_next(lex_t* lex) {
    token_t result = {
        .tok_id = LEX_TOKEN_ID_EOF,
        .tok_loc = {.loc_start = lex->lex_index, .loc_end = 0xAA}};

    return result;
}

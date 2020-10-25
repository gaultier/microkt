#pragma once

#include <string.h>

#include "common.h"

typedef enum {
    LEX_TOKEN_ID_BUILTIN_PRINT,
    LEX_TOKEN_ID_LPAREN,
    LEX_TOKEN_ID_RPAREN,
    LEX_TOKEN_ID_TRUE,
    LEX_TOKEN_ID_FALSE,
    LEX_TOKEN_ID_IDENTIFIER,
    LEX_TOKEN_ID_EOF,
    LEX_TOKEN_ID_INVALID,
} token_id_t;

const u8 token_id_t_to_str[][30] = {
    [LEX_TOKEN_ID_BUILTIN_PRINT] = "print",
    [LEX_TOKEN_ID_LPAREN] = "(",
    [LEX_TOKEN_ID_RPAREN] = ")",
    [LEX_TOKEN_ID_TRUE] = "true",
    [LEX_TOKEN_ID_FALSE] = "false",
    [LEX_TOKEN_ID_IDENTIFIER] = "Identifier",
    [LEX_TOKEN_ID_EOF] = "EOF",
    [LEX_TOKEN_ID_INVALID] = "INVALID",

};

typedef struct {
    usize loc_start, loc_end;
} loc_t;

typedef struct {
    token_id_t tok_id;
    loc_t tok_loc;
} token_t;

typedef struct {
    token_id_t key_id;
    const u8 key_str[20];
} keyword_t;

static const keyword_t keywords[] = {
    {.key_id = LEX_TOKEN_ID_TRUE, .key_str = "true"},
    {.key_id = LEX_TOKEN_ID_FALSE, .key_str = "false"},
    {.key_id = LEX_TOKEN_ID_BUILTIN_PRINT, .key_str = "print"},
};

typedef struct {
    const u8* lex_source;
    const usize lex_source_len;
    usize lex_index;
} lex_t;

typedef enum {
    LEX_STATE_START,
    LEX_STATE_IDENTIFIER,
} lex_state_t;

// TODO: trie
const token_id_t* token_get_keyword(const u8* source_start, usize len) {
    const usize keywords_len = sizeof(keywords) / sizeof(keywords[0]);
    for (usize i = 0; i < keywords_len; i++) {
        const keyword_t* k = &keywords[i];
        if (memcmp(source_start, k->key_str, MIN(len, sizeof(k->key_str))) == 0)
            return &k->key_id;
    }

    return NULL;
}

lex_t lex_init(const u8* source, const usize source_len) {
    return (lex_t){
        .lex_source = source, .lex_source_len = source_len, .lex_index = 0};
}

token_t lex_next(lex_t* lex) {
    PG_ASSERT_COND(lex, !=, NULL, "%p");
    PG_ASSERT_COND(lex->lex_source, !=, NULL, "%p");

    token_t result = {
        .tok_id = LEX_TOKEN_ID_EOF,
        .tok_loc = {.loc_start = lex->lex_index, .loc_end = 0xAA}};

    lex_state_t state = LEX_STATE_START;

    while (lex->lex_index < lex->lex_source_len) {
        const u8 c = lex->lex_source[lex->lex_index];

        switch (state) {
            case LEX_STATE_START: {
                switch (c) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t': {
                        result.tok_loc.loc_start = lex->lex_index + 1;
                        break;
                    }
                    case '(': {
                        result.tok_id = LEX_TOKEN_ID_LPAREN;
                        lex->lex_index += 1;
                        goto outer;
                    }
                    case ')': {
                        result.tok_id = LEX_TOKEN_ID_RPAREN;
                        lex->lex_index += 1;
                        goto outer;
                    }
                    case '_':
                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                    case 'g':
                    case 'h':
                    case 'i':
                    case 'j':
                    case 'k':
                    case 'l':
                    case 'm':
                    case 'n':
                    case 'o':
                    case 'p':
                    case 'q':
                    case 'r':
                    case 's':
                    case 't':
                    case 'u':
                    case 'v':
                    case 'w':
                    case 'x':
                    case 'y':
                    case 'z':
                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                    case 'G':
                    case 'H':
                    case 'I':
                    case 'J':
                    case 'K':
                    case 'L':
                    case 'M':
                    case 'N':
                    case 'O':
                    case 'P':
                    case 'Q':
                    case 'R':
                    case 'S':
                    case 'T':
                    case 'U':
                    case 'V':
                    case 'W':
                    case 'X':
                    case 'Y':
                    case 'Z': {
                        state = LEX_STATE_IDENTIFIER;
                        result.tok_id = LEX_TOKEN_ID_IDENTIFIER;
                        break;
                    }
                    default: {
                        result.tok_id = LEX_TOKEN_ID_INVALID;
                        lex->lex_index += 1;
                        goto outer;
                    }
                }
                break;
            }
            case LEX_STATE_IDENTIFIER: {
                switch (c) {
                    case '_':
                    case 'a':
                    case 'b':
                    case 'c':
                    case 'd':
                    case 'e':
                    case 'f':
                    case 'g':
                    case 'h':
                    case 'i':
                    case 'j':
                    case 'k':
                    case 'l':
                    case 'm':
                    case 'n':
                    case 'o':
                    case 'p':
                    case 'q':
                    case 'r':
                    case 's':
                    case 't':
                    case 'u':
                    case 'v':
                    case 'w':
                    case 'x':
                    case 'y':
                    case 'z':
                    case 'A':
                    case 'B':
                    case 'C':
                    case 'D':
                    case 'E':
                    case 'F':
                    case 'G':
                    case 'H':
                    case 'I':
                    case 'J':
                    case 'K':
                    case 'L':
                    case 'M':
                    case 'N':
                    case 'O':
                    case 'P':
                    case 'Q':
                    case 'R':
                    case 'S':
                    case 'T':
                    case 'U':
                    case 'V':
                    case 'W':
                    case 'X':
                    case 'Y':
                    case 'Z': {
                        break;
                    }
                    default: {
                        PG_ASSERT_COND(lex->lex_index, <, lex->lex_source_len,
                                       "%llu");
                        PG_ASSERT_COND(lex->lex_index, >=,
                                       result.tok_loc.loc_start, "%llu");

                        const token_id_t* id = NULL;

                        if ((id = token_get_keyword(
                                 lex->lex_source + result.tok_loc.loc_start,
                                 lex->lex_index - result.tok_loc.loc_start))) {
                            result.tok_id = *id;
                            printf("[debug] lex keyword=%s\n",
                                   token_id_t_to_str[*id]);
                        }
                        goto outer;
                    }
                }
                break;
            }
        }

        lex->lex_index += 1;
    }
outer:
    result.tok_loc.loc_end = lex->lex_index;

    return result;
}

void token_dump(const token_t* t) {
    printf("tok_id=%s tok_loc_start=%llu tok_loc_end=%llu\n",
           token_id_t_to_str[t->tok_id], t->tok_loc.loc_start,
           t->tok_loc.loc_end);
}

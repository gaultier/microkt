#pragma once

#define _POSIX_C_SOURCE 200112L
#include <string.h>

#include "common.h"

typedef enum {
    LEX_TOKEN_ID_BUILTIN_PRINT,
    LEX_TOKEN_ID_LPAREN,
    LEX_TOKEN_ID_RPAREN,
    LEX_TOKEN_ID_TRUE,
    LEX_TOKEN_ID_FALSE,
    LEX_TOKEN_ID_IDENTIFIER,
    LEX_TOKEN_ID_STRING_LITERAL,
    LEX_TOKEN_ID_INT,
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
    [LEX_TOKEN_ID_STRING_LITERAL] = "StringLiteral",
    [LEX_TOKEN_ID_INT] = "IntLiteral",
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
} lexer_t;

typedef enum {
    LEX_STATE_START,
    LEX_STATE_IDENTIFIER,
    LEX_STATE_STRING_LITERAL,
    LEX_STATE_INT,
} lex_state_t;

// TODO: trie?
static const token_id_t* token_get_keyword(const u8* source_start, usize len) {
    const usize keywords_len = sizeof(keywords) / sizeof(keywords[0]);
    for (usize i = 0; i < keywords_len; i++) {
        const keyword_t* k = &keywords[i];
        if (memcmp(source_start, k->key_str, MIN(len, sizeof(k->key_str))) == 0)
            return &k->key_id;
    }

    return NULL;
}

// TODO: unicode
static res_t lex_is_identifier_char(u8 c) {
    return ('0' <= c && c <= '9') || ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') || c == '_';
}

static lexer_t lex_init(const u8* source, const usize source_len) {
    return (lexer_t){
        .lex_source = source, .lex_source_len = source_len, .lex_index = 0};
}

static token_t lex_next(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");

    token_t result = {
        .tok_id = LEX_TOKEN_ID_EOF,
        .tok_loc = {.loc_start = lexer->lex_index, .loc_end = 0xAAAAAAAA}};

    lex_state_t state = LEX_STATE_START;

    while (lexer->lex_index < lexer->lex_source_len) {
        const u8 c = lexer->lex_source[lexer->lex_index];

        switch (state) {
            case LEX_STATE_START: {
                switch (c) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t': {
                        result.tok_loc.loc_start = lexer->lex_index + 1;
                        break;
                    }
                    case '(': {
                        result.tok_id = LEX_TOKEN_ID_LPAREN;
                        lexer->lex_index += 1;
                        goto outer;
                    }
                    case ')': {
                        result.tok_id = LEX_TOKEN_ID_RPAREN;
                        lexer->lex_index += 1;
                        goto outer;
                    }
                    case '"': {
                        result.tok_id = LEX_TOKEN_ID_STRING_LITERAL;
                        state = LEX_STATE_STRING_LITERAL;
                        break;
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
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9': {
                        state = LEX_STATE_INT;
                        result.tok_id = LEX_TOKEN_ID_INT;
                        break;
                    }
                    default: {
                        result.tok_id = LEX_TOKEN_ID_INVALID;
                        lexer->lex_index += 1;
                        goto outer;
                    }
                }
                break;
            }
            case LEX_STATE_INT: {
                switch (c) {
                    case '_': {
                        break;
                    }
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9': {
                        break;
                    }
                    default: {
                        if (lex_is_identifier_char(c)) {
                            result.tok_id = LEX_TOKEN_ID_INVALID;
                        }
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
                        PG_ASSERT_COND(lexer->lex_index, <,
                                       lexer->lex_source_len, "%llu");
                        PG_ASSERT_COND(lexer->lex_index, >=,
                                       result.tok_loc.loc_start, "%llu");

                        const token_id_t* id = NULL;

                        if ((id = token_get_keyword(
                                 lexer->lex_source + result.tok_loc.loc_start,
                                 lexer->lex_index -
                                     result.tok_loc.loc_start))) {
                            result.tok_id = *id;
                        }
                        goto outer;
                    }
                }
                break;
            }
            case LEX_STATE_STRING_LITERAL: {
                switch (c) {
                    case '"': {
                        lexer->lex_index += 1;
                        goto outer;
                    }
                    default: {
                    }
                }

                break;
            }
        }
        if (lexer->lex_index == lexer->lex_source_len) {
            switch (state) {
                case LEX_STATE_STRING_LITERAL: {
                    result.tok_id = LEX_TOKEN_ID_INVALID;
                    break;
                }
                default: {
                }
            }
        }

        lexer->lex_index += 1;
    }
outer:
    result.tok_loc.loc_end = lexer->lex_index;

    return result;
}

static void token_dump(const token_t* t) {
    log_debug("tok_id=%s tok_loc_start=%llu tok_loc_end=%llu",
              token_id_t_to_str[t->tok_id], t->tok_loc.loc_start,
              t->tok_loc.loc_end);
}

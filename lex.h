#pragma once

#include <stdbool.h>
#include <string.h>

#include "buf.h"
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
    LEX_TOKEN_ID_COMMENT,
    LEX_TOKEN_ID_CHAR,
    LEX_TOKEN_ID_EOF,
    LEX_TOKEN_ID_INVALID,
} token_id_t;

const u8 token_id_t_to_str[][30] = {
    [LEX_TOKEN_ID_BUILTIN_PRINT] = "Print",
    [LEX_TOKEN_ID_LPAREN] = "(",
    [LEX_TOKEN_ID_RPAREN] = ")",
    [LEX_TOKEN_ID_TRUE] = "true",
    [LEX_TOKEN_ID_FALSE] = "false",
    [LEX_TOKEN_ID_IDENTIFIER] = "Identifier",
    [LEX_TOKEN_ID_STRING_LITERAL] = "StringLiteral",
    [LEX_TOKEN_ID_COMMENT] = "Comment",
    [LEX_TOKEN_ID_CHAR] = "Char",
    [LEX_TOKEN_ID_INT] = "IntLiteral",
    [LEX_TOKEN_ID_EOF] = "Eof",
    [LEX_TOKEN_ID_INVALID] = "Invalid",
};

typedef struct {
    usize loc_start, loc_end;
} loc_t;

typedef struct {
    usize pos_line, pos_column;
} loc_pos_t;

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
    usize* lex_lines;
} lexer_t;

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

static loc_pos_t lex_pos(const lexer_t* lexer, usize position) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");

    loc_pos_t pos = {.pos_line = 1};

    usize i = 0;
    for (i = 0; i < buf_size(lexer->lex_lines); i++) {
        const usize line_pos = lexer->lex_lines[i];
        if (position < line_pos) break;

        pos.pos_line += 1;
        // TODO: column
    }

    pos.pos_column = position - (i > 0 ? lexer->lex_lines[i - 1] : 0);
    return pos;
}

// TODO: expand
static bool lex_is_digit(u8 c) { return ('0' <= c && c <= '9'); }

// TODO: unicode
static bool lex_is_identifier_char(u8 c) {
    return lex_is_digit(c) || ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') || c == '_';
}

static lexer_t lex_init(const u8* source, const usize source_len) {
    return (lexer_t){.lex_source = source,
                     .lex_source_len = source_len,
                     .lex_index = 0,
                     .lex_lines = NULL};
}

static u8 lex_advance(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");

    lexer->lex_index += 1;

    return lexer->lex_source[lexer->lex_index - 1];
}

static bool lex_is_at_end(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");

    return lexer->lex_index == lexer->lex_source_len;
}

static u8 lex_peek(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lex_is_at_end(lexer), !=, true, "%d");

    return lexer->lex_source[lexer->lex_index];
}

static u8 lex_peek_next(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len - 1, "%llu");

    return lex_is_at_end(lexer) ? '\0'
                                : lexer->lex_source[lexer->lex_index + 1];
}

static bool lex_match(lexer_t* lexer, u8 c) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");

    if (lex_is_at_end(lexer)) return false;

    if (lex_peek(lexer) != c) return false;

    lex_advance(lexer);
    return true;
}

static void lex_newline(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND(lex_peek(lexer), ==, '\n', "%c");

    buf_push(lexer->lex_lines, lexer->lex_index);
    log_debug("newline at position %llu", lexer->lex_index);

    lexer->lex_index += 1;
}

static void lex_advance_until_newline_or_eof(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len - 1, "%llu");

    while (true) {
        const u8 c = lex_peek(lexer);
        if (c == '\n' || c == '\0') break;

        lex_advance(lexer);
    }
}

static void lex_identifier(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    u8 c = lex_advance(lexer);
    PG_ASSERT_COND(lex_is_identifier_char(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!lex_is_identifier_char(c)) break;

        lex_advance(lexer);
    }

    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len, "%llu");
    PG_ASSERT_COND(lexer->lex_index, >=, result->tok_loc.loc_start, "%llu");

    const token_id_t* id = NULL;

    if ((id =
             token_get_keyword(lexer->lex_source + result->tok_loc.loc_start,
                               lexer->lex_index - result->tok_loc.loc_start))) {
        result->tok_id = *id;
    } else {
        result->tok_id = LEX_TOKEN_ID_IDENTIFIER;
    }
}

static res_t lex_number(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    u8 c = lex_advance(lexer);
    PG_ASSERT_COND(lex_is_digit(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!lex_is_digit(c)) break;

        lex_advance(lexer);
    }

    result->tok_id = LEX_TOKEN_ID_INT;
    return RES_OK;
}

// TODO: escape sequences
// TODO: multiline
static void lex_string(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    u8 c = lex_peek(lexer);
    PG_ASSERT_COND(c, ==, '"', "%c");
    lex_advance(lexer);

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (c == '"') break;

        lex_advance(lexer);
    }

    if (c != '"') {
        log_debug(
            "Unterminated string, missing trailing quote: c=`%c` "
            "lex_index=%llu",
            c, lexer->lex_index);
        result->tok_id = LEX_TOKEN_ID_INVALID;
    } else {
        PG_ASSERT_COND(c, ==, '"', "%c");
        result->tok_id = LEX_TOKEN_ID_STRING_LITERAL;
        lex_advance(lexer);
    }
}

// TODO: escape sequences
// TODO: unicode literals
static void lex_char(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    u8 c = lex_peek(lexer);
    PG_ASSERT_COND(c, ==, '\'', "%c");
    lex_advance(lexer);

    usize len = 0;
    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (c == '\'') break;

        lex_advance(lexer);
        len++;
    }

    if (c != '\'') {
        log_debug(
            "Unterminated char, missing trailing quote: c=`%c` "
            "lex_index=%llu",
            c, lexer->lex_index);
        result->tok_id = LEX_TOKEN_ID_INVALID;
    } else {
        PG_ASSERT_COND(c, ==, '\'', "%c");
        result->tok_id = LEX_TOKEN_ID_CHAR;
        lex_advance(lexer);
    }

    if (len != 1) {
        UNIMPLEMENTED();
    }
}

static token_t lex_next(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");

    token_t result = {
        .tok_id = LEX_TOKEN_ID_EOF,
        .tok_loc = {.loc_start = lexer->lex_index, .loc_end = 0xAAAAAAAA}};

    while (lexer->lex_index < lexer->lex_source_len) {
        const u8 c = lexer->lex_source[lexer->lex_index];

        switch (c) {
            case ' ':
            case '\r':
            case '\t': {
                result.tok_loc.loc_start = lexer->lex_index + 1;
                break;
            }
            case '\n': {
                result.tok_loc.loc_start = lexer->lex_index + 1;
                lex_newline(lexer);
                continue;
            }
            case '/': {
                if (lex_peek_next(lexer) == '/') {
                    lex_advance_until_newline_or_eof(lexer);
                    result.tok_id = LEX_TOKEN_ID_COMMENT;
                    goto outer;
                } else {
                    UNREACHABLE();  // TODO
                }
            }
            case '(': {
                result.tok_id = LEX_TOKEN_ID_LPAREN;
                lex_advance(lexer);
                goto outer;
            }
            case ')': {
                result.tok_id = LEX_TOKEN_ID_RPAREN;
                lex_advance(lexer);
                goto outer;
            }
            case '"': {
                lex_string(lexer, &result);
                goto outer;
            }
            case '\'': {
                lex_char(lexer, &result);
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
                lex_identifier(lexer, &result);
                goto outer;
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
                const res_t res = lex_number(lexer, &result);
                IGNORE(res);  // TODO: correct?
                goto outer;
            }
            default: {
                result.tok_id = LEX_TOKEN_ID_INVALID;
                lex_advance(lexer);
                goto outer;
            }
        }
        lex_advance(lexer);
    }
outer:
    result.tok_loc.loc_end = lexer->lex_index;

    return result;
}

static void token_dump(const token_t* t, const lexer_t* lexer) {
    PG_ASSERT_COND((void*)t, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");

    const loc_pos_t pos_start = lex_pos(lexer, t->tok_loc.loc_start);
    const loc_pos_t pos_end = lex_pos(lexer, t->tok_loc.loc_end);
    log_debug(
        "id=%s loc_start=%llu loc_end=%llu start_line=%llu "
        "start_column=%llu end_line=%llu end_column=%llu",
        token_id_t_to_str[t->tok_id], t->tok_loc.loc_start, t->tok_loc.loc_end,
        pos_start.pos_line, pos_start.pos_column, pos_end.pos_line,
        pos_end.pos_column);
}

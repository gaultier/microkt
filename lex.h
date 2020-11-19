#pragma once

#include <string.h>

#include "buf.h"
#include "common.h"

typedef enum {
    TOK_ID_BUILTIN_PRINTLN,
    TOK_ID_LPAREN,
    TOK_ID_RPAREN,
    TOK_ID_TRUE,
    TOK_ID_FALSE,
    TOK_ID_IDENTIFIER,
    TOK_ID_STRING,
    TOK_ID_LONG,
    TOK_ID_COMMENT,
    TOK_ID_CHAR,
    TOK_ID_PLUS,
    TOK_ID_STAR,
    TOK_ID_SLASH,
    TOK_ID_MINUS,
    TOK_ID_PERCENT,
    TOK_ID_LT,
    TOK_ID_GT,
    TOK_ID_LE,
    TOK_ID_GE,
    TOK_ID_EQ,
    TOK_ID_NEQ,
    TOK_ID_EQ_EQ,
    TOK_ID_NOT,
    TOK_ID_IF,
    TOK_ID_ELSE,
    TOK_ID_LCURLY,
    TOK_ID_RCURLY,
    TOK_ID_VAL,
    TOK_ID_VAR,
    TOK_ID_COLON,
    TOK_ID_EOF,
    TOK_ID_INVALID,
} token_id_t;

const char token_id_to_str[][30] = {
    [TOK_ID_BUILTIN_PRINTLN] = "Println",
    [TOK_ID_LPAREN] = "(",
    [TOK_ID_RPAREN] = ")",
    [TOK_ID_TRUE] = "true",
    [TOK_ID_FALSE] = "false",
    [TOK_ID_IDENTIFIER] = "Identifier",
    [TOK_ID_STRING] = "String",
    [TOK_ID_COMMENT] = "Comment",
    [TOK_ID_CHAR] = "Char",
    [TOK_ID_LONG] = "Int",
    [TOK_ID_PLUS] = "+",
    [TOK_ID_MINUS] = "-",
    [TOK_ID_STAR] = "*",
    [TOK_ID_SLASH] = "/",
    [TOK_ID_PERCENT] = "%",
    [TOK_ID_LT] = "<",
    [TOK_ID_LE] = "<=",
    [TOK_ID_GT] = ">",
    [TOK_ID_GE] = ">=",
    [TOK_ID_EQ] = "=",
    [TOK_ID_NEQ] = "!=",
    [TOK_ID_EQ_EQ] = "==",
    [TOK_ID_NOT] = "!",
    [TOK_ID_IF] = "if",
    [TOK_ID_ELSE] = "else",
    [TOK_ID_LCURLY] = "{",
    [TOK_ID_RCURLY] = "}",
    [TOK_ID_VAL] = "val",
    [TOK_ID_VAR] = "var",
    [TOK_ID_COLON] = ":",
    [TOK_ID_EOF] = "Eof",
    [TOK_ID_INVALID] = "Invalid",
};

// Position of a token in the file in term of file offsets. Start at 0.
typedef struct {
    int pr_start, pr_end;
} pos_range_t;

// Human readable line and column, computed from a file offset. Start at 1.
typedef struct {
    int loc_line, loc_column;
} loc_t;

typedef struct {
    token_id_t tok_id;
    pos_range_t tok_pos_range;
} token_t;

typedef struct {
    token_id_t key_id;
    const char key_str[20];
} keyword_t;

static const keyword_t keywords[] = {
    {.key_id = TOK_ID_TRUE, .key_str = "true"},
    {.key_id = TOK_ID_FALSE, .key_str = "false"},
    {.key_id = TOK_ID_BUILTIN_PRINTLN, .key_str = "println"},
    {.key_id = TOK_ID_IF, .key_str = "if"},
    {.key_id = TOK_ID_ELSE, .key_str = "else"},
    {.key_id = TOK_ID_VAL, .key_str = "val"},
    {.key_id = TOK_ID_VAR, .key_str = "var"},
};

typedef struct {
    const char* lex_source;
    int lex_source_len, lex_index, *lex_lines /* file offset of line # */;
    loc_t* lex_locs;
    token_t* lex_tokens;
    token_id_t* lex_tok_ids;
    pos_range_t* lex_tok_pos_ranges;
} lexer_t;

// TODO: trie?
static const token_id_t* token_get_keyword(const char* source_start, int len) {
    const int keywords_len = sizeof(keywords) / sizeof(keywords[0]);
    for (int i = 0; i < keywords_len; i++) {
        const keyword_t* k = &keywords[i];
        const int keyword_len = strlen(k->key_str);

        if (len == keyword_len && memcmp(source_start, k->key_str, len) == 0)
            return &k->key_id;
    }

    return NULL;
}

static loc_t lex_pos_to_loc(const lexer_t* lexer, int position) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((int)buf_size(lexer->lex_lines), >, 0, "%d");

    loc_t loc = {.loc_line = 1};

    int i = 0;
    const int last_line = lexer->lex_lines[buf_size(lexer->lex_lines) - 1];
    for (i = 0; i < (int)buf_size(lexer->lex_lines); i++) {
        const int line_pos = lexer->lex_lines[i];
        if (position < line_pos) break;

        loc.loc_line += 1;
    }

    loc.loc_column = position - (i > 0 ? lexer->lex_lines[i - 1] : 0);
    if (loc.loc_line > last_line) loc.loc_line = last_line;
    return loc;
}

// TODO: expand
static bool lex_is_digit(char c) { return ('0' <= c && c <= '9'); }

// TODO: unicode
static bool lex_is_identifier_char(char c) {
    return lex_is_digit(c) || ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') || c == '_';
}

static char lex_advance(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");

    lexer->lex_index += 1;

    return lexer->lex_source[lexer->lex_index - 1];
}

static bool lex_is_at_end(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");

    return lexer->lex_index == lexer->lex_source_len;
}

static char lex_peek(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lex_is_at_end(lexer), !=, true, "%d");

    return lexer->lex_source[lexer->lex_index];
}

static char lex_peek_next(const lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len - 1, "%d");

    return lex_is_at_end(lexer) ? '\0'
                                : lexer->lex_source[lexer->lex_index + 1];
}

static bool lex_match(lexer_t* lexer, char c) {
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
    log_debug("newline at position %d", lexer->lex_index);

    lexer->lex_index += 1;
}

static void lex_advance_until_newline_or_eof(lexer_t* lexer) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len - 1, "%d");

    while (true) {
        const char c = lex_peek(lexer);
        if (c == '\n' || c == '\0') break;

        lex_advance(lexer);
    }
}

static void lex_identifier(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    char c = lex_advance(lexer);
    PG_ASSERT_COND(lex_is_identifier_char(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!lex_is_identifier_char(c)) break;

        lex_advance(lexer);
    }

    PG_ASSERT_COND(lexer->lex_index, <, lexer->lex_source_len, "%d");
    PG_ASSERT_COND(lexer->lex_index, >=, result->tok_pos_range.pr_start, "%d");

    const token_id_t* id = NULL;

    if ((id = token_get_keyword(
             lexer->lex_source + result->tok_pos_range.pr_start,
             lexer->lex_index - result->tok_pos_range.pr_start))) {
        result->tok_id = *id;
    } else {
        result->tok_id = TOK_ID_IDENTIFIER;
    }
}

static res_t lex_number(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    char c = lex_advance(lexer);
    PG_ASSERT_COND(lex_is_digit(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!lex_is_digit(c)) break;

        lex_advance(lexer);
    }

    result->tok_id = TOK_ID_LONG;
    return RES_OK;
}

// TODO: escape sequences
// TODO: multiline
static void lex_string(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    char c = lex_peek(lexer);
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
            "lex_index=%d",
            c, lexer->lex_index);
        result->tok_id = TOK_ID_INVALID;
    } else {
        PG_ASSERT_COND(c, ==, '"', "%c");
        result->tok_id = TOK_ID_STRING;
        lex_advance(lexer);
    }
}

// TODO: escape sequences
// TODO: unicode literals
static void lex_char(lexer_t* lexer, token_t* result) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)result, !=, NULL, "%p");

    char c = lex_peek(lexer);
    PG_ASSERT_COND(c, ==, '\'', "%c");
    lex_advance(lexer);

    int len = 0;
    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (c == '\'') break;

        lex_advance(lexer);
        len++;
    }

    if (c != '\'') {
        log_debug(
            "Unterminated char, missing trailing quote: c=`%c` "
            "lex_index=%d",
            c, lexer->lex_index);
        result->tok_id = TOK_ID_INVALID;
    } else {
        PG_ASSERT_COND(c, ==, '\'', "%c");
        result->tok_id = TOK_ID_CHAR;
        lex_advance(lexer);
    }

    if (len != 1) {
        UNIMPLEMENTED();
    }
}

static token_t lex_next(lexer_t* lexer, int* line, int* col) {
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer->lex_source, !=, NULL, "%p");
    PG_ASSERT_COND((void*)line, !=, NULL, "%p");
    PG_ASSERT_COND((void*)col, !=, NULL, "%p");

    token_t result = {.tok_id = TOK_ID_EOF,
                      .tok_pos_range = {.pr_start = lexer->lex_index}};

    while (lexer->lex_index < lexer->lex_source_len) {
        const char c = lexer->lex_source[lexer->lex_index];
        *col += 1;

        switch (c) {
            case ' ':
            case '\r':
            case '\t': {
                result.tok_pos_range.pr_start = lexer->lex_index + 1;
                break;
            }
            case '\n': {
                result.tok_pos_range.pr_start = lexer->lex_index + 1;
                lex_newline(lexer);
                *col = 0;
                *line += 1;
                continue;
            }
            case '/': {
                if (lex_peek_next(lexer) == '/') {
                    lex_advance_until_newline_or_eof(lexer);
                    result.tok_id = TOK_ID_COMMENT;
                    goto outer;
                } else {
                    lex_match(lexer, '/');
                    result.tok_id = TOK_ID_SLASH;
                    goto outer;
                }
            }
            case '=': {
                lex_match(lexer, '=');
                result.tok_id =
                    lex_match(lexer, '=') ? TOK_ID_EQ_EQ : TOK_ID_EQ;
                goto outer;
            }
            case '!': {
                lex_match(lexer, '!');
                result.tok_id = lex_match(lexer, '=') ? TOK_ID_NEQ : TOK_ID_NOT;
                goto outer;
            }
            case '<': {
                lex_match(lexer, '<');
                result.tok_id = lex_match(lexer, '=') ? TOK_ID_LE : TOK_ID_LT;
                goto outer;
            }
            case '>': {
                lex_match(lexer, '>');
                result.tok_id = lex_match(lexer, '=') ? TOK_ID_GE : TOK_ID_GT;
                goto outer;
            }
            case ':': {
                lex_match(lexer, ':');
                result.tok_id = TOK_ID_COLON;

                goto outer;
            }
            case '{': {
                lex_match(lexer, '{');
                result.tok_id = TOK_ID_LCURLY;

                goto outer;
            }
            case '}': {
                lex_match(lexer, '}');
                result.tok_id = TOK_ID_RCURLY;

                goto outer;
            }
            case '+': {
                lex_match(lexer, '+');
                result.tok_id = TOK_ID_PLUS;

                goto outer;
            }
            case '*': {
                lex_match(lexer, '*');
                result.tok_id = TOK_ID_STAR;

                goto outer;
            }
            case '-': {
                lex_match(lexer, '-');
                result.tok_id = TOK_ID_MINUS;

                goto outer;
            }
            case '%': {
                lex_match(lexer, '%');
                result.tok_id = TOK_ID_PERCENT;

                goto outer;
            }
            case '(': {
                result.tok_id = TOK_ID_LPAREN;
                lex_advance(lexer);
                goto outer;
            }
            case ')': {
                result.tok_id = TOK_ID_RPAREN;
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
                const res_t res = lex_number(lexer, &result);
                IGNORE(res);  // TODO: correct?
                goto outer;
            }
            default: {
                result.tok_id = TOK_ID_INVALID;
                lex_advance(lexer);
                goto outer;
            }
        }
        lex_advance(lexer);
    }
outer:
    result.tok_pos_range.pr_end = lexer->lex_index;

    return result;
}

static void token_dump(const token_t* t, int i, const lexer_t* lexer) {
    PG_ASSERT_COND((void*)t, !=, NULL, "%p");
    PG_ASSERT_COND((void*)lexer, !=, NULL, "%p");

    loc_t loc = lexer->lex_locs[i];
#ifndef WITH_LOGS
    IGNORE(i);
    IGNORE(loc);
#endif
    log_debug("%d:%d:id=%s #%d %d..%d `%.*s`", loc.loc_line, loc.loc_column,
              token_id_to_str[t->tok_id], i, t->tok_pos_range.pr_start,
              t->tok_pos_range.pr_end,
              t->tok_pos_range.pr_end - t->tok_pos_range.pr_start,
              &lexer->lex_source[t->tok_pos_range.pr_start]);
}

static lexer_t lex_init(const char* source, const int source_len) {
    PG_ASSERT_COND((void*)source, !=, NULL, "%p");

    lexer_t lexer = {.lex_source = source, .lex_source_len = source_len};
    buf_grow(lexer.lex_tokens, source_len / 8);
    buf_grow(lexer.lex_tok_pos_ranges, source_len / 8);
    buf_grow(lexer.lex_tok_ids, source_len / 8);

    int i = 0, col = 0, line = 1;
    while (true) {
        const token_t token = lex_next(&lexer, &line, &col);

        buf_push(lexer.lex_tokens, token);
        buf_push(lexer.lex_tok_pos_ranges, token.tok_pos_range);
        buf_push(lexer.lex_tok_ids, token.tok_id);
        buf_push(lexer.lex_locs,
                 ((loc_t){.loc_line = line, .loc_column = col}));

        token_dump(&token, i, &lexer);

        if (token.tok_id == TOK_ID_EOF) break;
        i++;
    }
    PG_ASSERT_COND((int)buf_size(lexer.lex_tok_ids), ==,
                   (int)buf_size(lexer.lex_tok_pos_ranges), "%d");

    return lexer;
}


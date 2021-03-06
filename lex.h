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
    TOK_ID_NUM,
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
    TOK_ID_WHILE,
    TOK_ID_FUN,
    TOK_ID_RETURN,
    TOK_ID_COMMA,
    TOK_ID_CLASS,
    TOK_ID_DOT,
    TOK_ID_EOF,
    TOK_ID_INVALID,
} mkt_token_id_t;

const char mkt_token_id_to_str[][30] = {
    [TOK_ID_BUILTIN_PRINTLN] = "Println",
    [TOK_ID_LPAREN] = "(",
    [TOK_ID_RPAREN] = ")",
    [TOK_ID_TRUE] = "true",
    [TOK_ID_FALSE] = "false",
    [TOK_ID_IDENTIFIER] = "Identifier",
    [TOK_ID_STRING] = "String",
    [TOK_ID_COMMENT] = "Comment",
    [TOK_ID_CHAR] = "Char",
    [TOK_ID_NUM] = "Int",
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
    [TOK_ID_WHILE] = "while",
    [TOK_ID_FUN] = "fun",
    [TOK_ID_RETURN] = "return",
    [TOK_ID_COMMA] = ",",
    [TOK_ID_CLASS] = "class",
    [TOK_ID_DOT] = ".",
    [TOK_ID_EOF] = "Eof",
    [TOK_ID_INVALID] = "Invalid",
};

// Position of a token in the file in term of file offsets. Start at 0.
typedef struct {
    i32 pr_start, pr_end;
} mkt_pos_range_t;

// Human readable line and column, computed from a file offset. Start at 1.
typedef struct {
    i32 loc_line, loc_column;
} mkt_loc_t;

typedef struct {
    mkt_token_id_t tok_id;
    mkt_pos_range_t tok_pos_range;
} mkt_token_t;

typedef struct {
    mkt_token_id_t key_id;
    const char key_str[20];
} mkt_keyword_t;

static const mkt_keyword_t keywords[] = {
    {.key_id = TOK_ID_TRUE, .key_str = "true"},
    {.key_id = TOK_ID_FALSE, .key_str = "false"},
    {.key_id = TOK_ID_BUILTIN_PRINTLN, .key_str = "println"},
    {.key_id = TOK_ID_IF, .key_str = "if"},
    {.key_id = TOK_ID_ELSE, .key_str = "else"},
    {.key_id = TOK_ID_VAL, .key_str = "val"},
    {.key_id = TOK_ID_VAR, .key_str = "var"},
    {.key_id = TOK_ID_WHILE, .key_str = "while"},
    {.key_id = TOK_ID_FUN, .key_str = "fun"},
    {.key_id = TOK_ID_RETURN, .key_str = "return"},
    {.key_id = TOK_ID_CLASS, .key_str = "class"},
};

typedef struct {
    const char* lex_source;
    i32 lex_source_len, lex_index;
    mkt_loc_t* lex_locs;
    mkt_token_t* lex_tokens;
    mkt_pos_range_t* lex_tok_pos_ranges;
} mkt_lexer_t;

// TODO: trie?
static const mkt_token_id_t* token_get_keyword(const char* source_start,
                                               i32 len) {
    const i32 keywords_len = sizeof(keywords) / sizeof(keywords[0]);
    for (i32 i = 0; i < keywords_len; i++) {
        const mkt_keyword_t* k = &keywords[i];
        const i32 keyword_len = strlen(k->key_str);

        if (len == keyword_len && memcmp(source_start, k->key_str, len) == 0)
            return &k->key_id;
    }

    return NULL;
}

// TODO: expand
static bool lex_is_digit(char c) { return ('0' <= c && c <= '9'); }

// TODO: unicode
static bool lex_is_identifier_char(char c) {
    return lex_is_digit(c) || ('a' <= c && c <= 'z') ||
           ('A' <= c && c <= 'Z') || c == '_';
}

static char lex_advance(mkt_lexer_t* lexer, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");
    CHECK(lexer->lex_index, <=, lexer->lex_source_len, "%d");

    lexer->lex_index += 1;
    *col += 1;

    return lexer->lex_source[lexer->lex_index - 1];
}

static bool lex_is_at_end(const mkt_lexer_t* lexer) {
    CHECK((void*)lexer, !=, NULL, "%p");

    return lexer->lex_index == lexer->lex_source_len;
}

static char lex_peek(const mkt_lexer_t* lexer) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");

    if (lex_is_at_end(lexer)) return '\0';

    CHECK(lexer->lex_index, <, lexer->lex_source_len, "%d");
    return lexer->lex_source[lexer->lex_index];
}

static char lex_peek_next(const mkt_lexer_t* lexer) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");

    return lex_is_at_end(lexer) ? '\0'
                                : lexer->lex_source[lexer->lex_index + 1];
}

static char lex_peek_next_next(const mkt_lexer_t* lexer) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");

    return lexer->lex_index >= (lexer->lex_source_len + 2)
               ? '\0'
               : lexer->lex_source[lexer->lex_index + 2];
}

static bool lex_match(mkt_lexer_t* lexer, char c, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    if (lex_is_at_end(lexer)) return false;

    if (lex_peek(lexer) != c) return false;

    lex_advance(lexer, col);
    return true;
}

static void lex_advance_until_newline_or_eof(mkt_lexer_t* lexer, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK(lexer->lex_index, <, lexer->lex_source_len - 1, "%d");
    CHECK((void*)col, !=, NULL, "%p");

    while (true) {
        const char c = lex_peek(lexer);
        if (c == '\n' || c == '\0') {
            break;
        }

        lex_advance(lexer, col);
    }
}

static void lex_identifier(mkt_lexer_t* lexer, mkt_token_t* result, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)result, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    char c = lex_advance(lexer, col);
    CHECK(lex_is_identifier_char(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!lex_is_identifier_char(c)) break;

        lex_advance(lexer, col);
    }

    CHECK(lexer->lex_index, >=, 0, "%d");
    CHECK(lexer->lex_index, <, lexer->lex_source_len, "%d");
    CHECK(lexer->lex_index, >=, result->tok_pos_range.pr_start, "%d");

    const mkt_token_id_t* id = NULL;

    if ((id = token_get_keyword(
             lexer->lex_source + result->tok_pos_range.pr_start,
             lexer->lex_index - result->tok_pos_range.pr_start))) {
        result->tok_id = *id;
    } else {
        result->tok_id = TOK_ID_IDENTIFIER;
    }
}

static mkt_res_t lex_number(mkt_lexer_t* lexer, mkt_token_t* result, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)result, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    char c = lex_advance(lexer, col);
    CHECK(lex_is_digit(c), ==, true, "%d");

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (!(lex_is_digit(c) || c == 'L')) break;

        lex_advance(lexer, col);
    }

    result->tok_id = TOK_ID_NUM;
    return RES_OK;
}

// TODO: escape sequences
// TODO: multiline
static void lex_string(mkt_lexer_t* lexer, mkt_token_t* result, i32* line,
                       i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)result, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    char c = lex_peek(lexer);
    CHECK(c, ==, '"', "%c");
    lex_advance(lexer, col);

    const bool multiline =
        lex_peek(lexer) == '"' && lex_peek_next(lexer) == '"';
    if (multiline) {
        lex_advance(lexer, col);
        lex_advance(lexer, col);
    }

    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (c == '"' && !multiline) {
            result->tok_id = TOK_ID_STRING;
            lex_advance(lexer, col);
            return;
        }
        if (c == '"' && lex_peek_next(lexer) == '"' &&
            lex_peek_next_next(lexer) == '"' && multiline) {
            result->tok_id = TOK_ID_STRING;
            lex_advance(lexer, col);
            lex_advance(lexer, col);
            lex_advance(lexer, col);
            return;
        }
        if (c == '\n') {
            *line += 1;
            *col = 1;
        }

        lex_advance(lexer, col);
    }

    log_debug(
        "Unterminated string, missing terminating quote%s: c=`%c` "
        "lex_index=%d",
        multiline ? "s" : "", c, lexer->lex_index);
    result->tok_id = TOK_ID_INVALID;
}

// TODO: escape sequences
// TODO: unicode literals
static void lex_char(mkt_lexer_t* lexer, mkt_token_t* result, i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)result, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    char c = lex_peek(lexer);
    CHECK(c, ==, '\'', "%c");
    lex_advance(lexer, col);

    i32 len = 0;
    while (lexer->lex_index < lexer->lex_source_len) {
        c = lex_peek(lexer);
        if (c == '\'') break;

        lex_advance(lexer, col);
        len++;
    }

    if (c != '\'') {
        log_debug(
            "Unterminated char, missing trailing quote: c=`%c` "
            "lex_index=%d",
            c, lexer->lex_index);
        result->tok_id = TOK_ID_INVALID;
    } else {
        CHECK(c, ==, '\'', "%c");
        result->tok_id = TOK_ID_CHAR;
        lex_advance(lexer, col);
    }

    if (len != 1) {
        UNIMPLEMENTED();
    }
}

static mkt_token_t lex_next(mkt_lexer_t* lexer, i32* line, i32* start_col,
                            i32* col) {
    CHECK((void*)lexer, !=, NULL, "%p");
    CHECK((void*)lexer->lex_source, !=, NULL, "%p");
    CHECK((void*)line, !=, NULL, "%p");
    CHECK((void*)start_col, !=, NULL, "%p");
    CHECK((void*)col, !=, NULL, "%p");

    mkt_token_t result = {.tok_id = TOK_ID_EOF,
                          .tok_pos_range = {.pr_start = lexer->lex_index}};

    while (lexer->lex_index < lexer->lex_source_len) {
        const char c = lexer->lex_source[lexer->lex_index];

        switch (c) {
            case ' ':
            case '\r':
            case '\t': {
                result.tok_pos_range.pr_start = lexer->lex_index + 1;
                *start_col += 1;
                break;
            }
            case '\n': {
                result.tok_pos_range.pr_start = lexer->lex_index + 1;
                lexer->lex_index += 1;
                *col = *start_col = 1;
                *line += 1;
                continue;
            }
            case '/': {
                if (lex_peek_next(lexer) == '/') {
                    lex_advance_until_newline_or_eof(lexer, col);
                    result.tok_id = TOK_ID_COMMENT;
                    goto outer;
                } else {
                    lex_match(lexer, '/', col);
                    result.tok_id = TOK_ID_SLASH;
                    goto outer;
                }
            }
            case '.': {
                lex_match(lexer, '.', col);
                result.tok_id = TOK_ID_DOT;

                goto outer;
            }
            case '=': {
                lex_match(lexer, '=', col);
                result.tok_id =
                    lex_match(lexer, '=', col) ? TOK_ID_EQ_EQ : TOK_ID_EQ;
                goto outer;
            }
            case '!': {
                lex_match(lexer, '!', col);
                result.tok_id =
                    lex_match(lexer, '=', col) ? TOK_ID_NEQ : TOK_ID_NOT;
                goto outer;
            }
            case '<': {
                lex_match(lexer, '<', col);
                result.tok_id =
                    lex_match(lexer, '=', col) ? TOK_ID_LE : TOK_ID_LT;
                goto outer;
            }
            case '>': {
                lex_match(lexer, '>', col);
                result.tok_id =
                    lex_match(lexer, '=', col) ? TOK_ID_GE : TOK_ID_GT;
                goto outer;
            }
            case ':': {
                lex_match(lexer, ':', col);
                result.tok_id = TOK_ID_COLON;

                goto outer;
            }
            case '{': {
                lex_match(lexer, '{', col);
                result.tok_id = TOK_ID_LCURLY;

                goto outer;
            }
            case '}': {
                lex_match(lexer, '}', col);
                result.tok_id = TOK_ID_RCURLY;

                goto outer;
            }
            case '+': {
                lex_match(lexer, '+', col);
                result.tok_id = TOK_ID_PLUS;

                goto outer;
            }
            case '*': {
                lex_match(lexer, '*', col);
                result.tok_id = TOK_ID_STAR;

                goto outer;
            }
            case '-': {
                lex_match(lexer, '-', col);
                result.tok_id = TOK_ID_MINUS;

                goto outer;
            }
            case '%': {
                lex_match(lexer, '%', col);
                result.tok_id = TOK_ID_PERCENT;

                goto outer;
            }
            case '(': {
                result.tok_id = TOK_ID_LPAREN;
                lex_advance(lexer, col);
                goto outer;
            }
            case ')': {
                result.tok_id = TOK_ID_RPAREN;
                lex_advance(lexer, col);
                goto outer;
            }
            case ',': {
                result.tok_id = TOK_ID_COMMA;
                lex_advance(lexer, col);
                goto outer;
            }
            case '"': {
                lex_string(lexer, &result, line, col);
                goto outer;
            }
            case '\'': {
                lex_char(lexer, &result, col);
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
                lex_identifier(lexer, &result, col);
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
                const mkt_res_t res = lex_number(lexer, &result, col);
                IGNORE(res);  // TODO: correct?
                goto outer;
            }
            default: {
                result.tok_id = TOK_ID_INVALID;
                log_debug("Invalid token: `%c`", c);
                lex_advance(lexer, col);
                goto outer;
            }
        }
        lex_advance(lexer, col);
    }
outer:
    result.tok_pos_range.pr_end = lexer->lex_index;
    CHECK(result.tok_pos_range.pr_end, >=, result.tok_pos_range.pr_start, "%d");

    return result;
}

static void token_dump(const mkt_token_t* t, i32 i, const mkt_lexer_t* lexer) {
    CHECK((void*)t, !=, NULL, "%p");
    CHECK((void*)lexer, !=, NULL, "%p");

    mkt_loc_t loc = lexer->lex_locs[i];
#if WITH_LOGS == 0
    IGNORE(i);
    IGNORE(loc);
#endif
    log_debug("%d:%d:id=%s #%d %d..%d `%.*s`", loc.loc_line, loc.loc_column,
              mkt_token_id_to_str[t->tok_id], i, t->tok_pos_range.pr_start,
              t->tok_pos_range.pr_end,
              t->tok_pos_range.pr_end - t->tok_pos_range.pr_start,
              &lexer->lex_source[t->tok_pos_range.pr_start]);
}

static mkt_res_t lex_init(const char* file_name0, const char* source,
                          const i32 source_len, mkt_lexer_t* lexer) {
    CHECK((void*)file_name0, !=, NULL, "%p");
    CHECK((void*)source, !=, NULL, "%p");
    CHECK((void*)lexer, !=, NULL, "%p");

    lexer->lex_source = source;
    lexer->lex_source_len = source_len;
    buf_grow(lexer->lex_tokens, source_len / 8);
    buf_grow(lexer->lex_tok_pos_ranges, source_len / 8);

    bool err = false;
    i32 i = 0, col = 1, line = 1;
    while (true) {
        i32 start_col = col;
        const mkt_token_t token = lex_next(lexer, &line, &start_col, &col);
        if (token.tok_id == TOK_ID_INVALID) {
            fprintf(stderr, "%s%s:%d:%d:Invalid token: %s%c\n",
                    mkt_colors[is_tty][COL_GRAY], file_name0, line, start_col,
                    mkt_colors[is_tty][COL_RESET],
                    source[token.tok_pos_range.pr_start]);
            err = true;
        }

        buf_push(lexer->lex_tokens, token);
        buf_push(lexer->lex_tok_pos_ranges, token.tok_pos_range);
        buf_push(lexer->lex_locs,
                 ((mkt_loc_t){.loc_line = line, .loc_column = start_col}));

        token_dump(&token, i, lexer);

        if (token.tok_id == TOK_ID_EOF) break;
        i++;
    }
    if (err) return RES_ERR;

    CHECK(buf_size(lexer->lex_tokens), ==, buf_size(lexer->lex_tok_pos_ranges),
          "%zu");
    CHECK(buf_size(lexer->lex_tokens), ==, buf_size(lexer->lex_locs), "%zu");

    return RES_OK;
}


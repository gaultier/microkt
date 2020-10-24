#include "lex.h"

int main() {
    const u8 *source = (u8 *)"true";
    lex_t lex = lex_init(source, sizeof(source));
    token_t t = lex_next(&lex);
    token_dump(&t);
}

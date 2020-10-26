#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "ast.h"
#include "emit_x86_64.h"
#include "lex.h"
#include "parse.h"

res_t driver_run(const u8* file_name0) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");

    FILE* file = NULL;
    if ((file = fopen(file_name0, "r")) == NULL) {
        fprintf(stderr, "Could not open the file `%s`: errno=%d error=%s\n",
                file_name0, errno, strerror(errno));
        return errno;
    }

    int ret = 0;
    if ((ret = fseek(file, 0, SEEK_END)) != 0) {
        fprintf(stderr,
                "Could not move the file cursor to the end of the file `%s`: "
                "errno=%d error=%s\n",
                file_name0, errno, strerror(errno));
        fclose(file);
        return errno;
    }
    const usize file_size = (size_t)ftell(file);

    const u8* source =
        mmap(NULL, file_size, PROT_READ, MAP_SHARED, fileno(file), 0);
    if (source == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap the file %s: %s", file_name0,
                strerror(errno));
        fclose(file);
        return RES_OK;
    }

    parser_t parser = parser_init(file_name0, source, file_size);

    if (parser_parse(&parser) == RES_ERR) {
        fprintf(stderr, "%s: error\n", file_name0);
        return RES_ERR;
    }

    emit_asm_t a;
    emit_emit(&parser, &a);
    emit_asm_dump(&a, stdout);

    munmap((void*)source, file_size);
    fclose(file);

    return RES_OK;
}

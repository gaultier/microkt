#pragma once

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ast.h"
#include "common.h"
#include "emit_x86_64.h"
#include "lex.h"
#include "parse.h"

res_t driver_is_file_name_valid(const u8* file_name0) {
    const usize len = strlen(file_name0);
    return (len > (3 + 1) && memcmp(&file_name0[len - 4], ".kts", 3) == 0)
               ? RES_OK
               : RES_ERR;
}

const u8* driver_base_source_file_name(const u8* file_name0) {
    PG_ASSERT_COND(driver_is_file_name_valid(file_name0), ==, RES_OK, "%d");

    u8* base_file_name0 = strdup(file_name0);
    base_file_name0[strlen(file_name0) - 4] = 0;
    return base_file_name0;
}

res_t driver_run(const u8* file_name0) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");

    if (driver_is_file_name_valid(file_name0) != RES_OK) {
        fprintf(stderr, "Invalid file name: %s\n", file_name0);
        return RES_ERR;
    }

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
        return RES_OK;
    }

    parser_t parser = parser_init(file_name0, source, file_size);

    if (parser_parse(&parser) == RES_ERR) {
        fprintf(stderr, "%s: error\n", file_name0);
        return RES_ERR;
    }

    emit_asm_t a;
    emit_emit(&parser, &a);

    const usize file_name_len = strlen(file_name0);
    u8* const asm_file_name0 = strdup(file_name0);
    asm_file_name0[file_name_len - 3] = 'a';
    asm_file_name0[file_name_len - 2] = 's';
    asm_file_name0[file_name_len - 1] = 'm';
    FILE* asm_file = fopen(asm_file_name0, "w");
    if (asm_file == NULL) return RES_ERR;

    emit_asm_dump(&a, asm_file);

    // as
    const u8* const base_file_name0 = driver_base_source_file_name(file_name0);
    usize argv_len = 2 * strlen(file_name0) + 100;
    u8* argv = calloc(argv_len, 1);
    PG_ASSERT_COND(argv, !=, NULL, "%p");
    {
        snprintf(argv, argv_len, "/usr/bin/as %s -o %s.o", asm_file_name0,
                 base_file_name0);
        fprintf(stderr, "[debug] %s\n", argv);

        FILE* as_process = popen(argv, "w");
        if (as_process == NULL) {
            fprintf(stderr, "Failed to run `as`: `%s` %s\n", argv,
                    strerror(errno));
            return RES_ERR;
        }
        if (pclose(as_process) != 0) {
            fprintf(stderr, "Failed to run `as`: `%s` %s\n", argv,
                    strerror(errno));
            return RES_ERR;
        }
    }

    // ld
    {
        memset(argv, 0, argv_len);
        snprintf(argv, argv_len, "/usr/bin/ld %s.o -o %s -lSystem",
                 base_file_name0, base_file_name0);
        fprintf(stderr, "[debug] %s\n", argv);

        FILE* ld_process = popen(argv, "w");
        if (ld_process == NULL) {
            fprintf(stderr, "Failed to run `ld`: `%s` %s\n", argv,
                    strerror(errno));
            return RES_ERR;
        }
        if (pclose(ld_process) != 0) {
            fprintf(stderr, "Failed to run `ld`: `%s` %s\n", argv,
                    strerror(errno));
            return RES_ERR;
        }
    }

    return RES_OK;
}

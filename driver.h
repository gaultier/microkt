#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

#include "common.h"
#include "emit_x86_64.h"

static bool driver_is_file_name_valid(const u8* file_name0) {
    const usize len = strlen(file_name0);
    return (len > (3 + 1) && memcmp(&file_name0[len - 4], ".kts", 3) == 0);
}

static void driver_base_source_file_name(const u8* file_name0,
                                         u8* base_file_name0) {
    PG_ASSERT_COND(driver_is_file_name_valid(file_name0), ==, RES_OK, "%d");

    const usize len = strlen(file_name0);

    base_file_name0[len - 4] = 0;
    base_file_name0[len - 3] = 0;
    base_file_name0[len - 2] = 0;
    base_file_name0[len - 1] = 0;
}

static res_t driver_run(const u8* file_name0) {
    PG_ASSERT_COND((void*)file_name0, !=, NULL, "%p");

    res_t res = RES_NONE;
    if (!driver_is_file_name_valid(file_name0)) {
        res = RES_INVALID_SOURCE_FILE_NAME;
        fprintf(stderr, res_to_str[res], file_name0);
        return res;
    }

    FILE* file = NULL;
    if ((file = fopen(file_name0, "r")) == NULL) {
        res = RES_SOURCE_FILE_READ_FAILED;
        fprintf(stderr, res_to_str[res], file_name0, strerror(errno));
        return res;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        res = RES_SOURCE_FILE_READ_FAILED;
        fprintf(stderr, res_to_str[res], file_name0, strerror(errno));
        return res;
    }
    const usize file_size = (size_t)ftell(file);

    const u8* source =
        mmap(NULL, file_size, PROT_READ, MAP_SHARED, fileno(file), 0);
    if (source == MAP_FAILED) {
        res = RES_SOURCE_FILE_READ_FAILED;
        fprintf(stderr, res_to_str[res], file_name0, strerror(errno));
        return res;
    }

    parser_t parser = parser_init(file_name0, source, file_size);

    if ((res = parser_parse(&parser)) != RES_OK) return res;

    emit_t emitter = emit_init();
    emit_emit(&emitter, &parser);

    const usize file_name_len = strlen(file_name0);
    u8 asm_file_name0[MAXPATHLEN + 1] = "\0";
    memcpy(asm_file_name0, file_name0, MAXPATHLEN);
    asm_file_name0[file_name_len - 3] = 'a';
    asm_file_name0[file_name_len - 2] = 's';
    asm_file_name0[file_name_len - 1] = 'm';
    FILE* asm_file = fopen(asm_file_name0, "w");
    if (asm_file == NULL) {
        res = RES_ASM_FILE_READ_FAILED;
        fprintf(stderr, res_to_str[res], asm_file_name0, strerror(errno));
        return res;
    }

    log_debug("writing asm output to `%s`", asm_file_name0);

    emit_asm_dump(&emitter, asm_file);
    // Make sure everything has been written to the file and not simply buffered
    fflush(asm_file);
    fclose(file);

    // as
    static u8 base_file_name0[MAXPATHLEN + 1] = "\0";
    memcpy(base_file_name0, file_name0, MAXPATHLEN);
    driver_base_source_file_name(file_name0, base_file_name0);
    {
        static u8 argv0[3 * MAXPATHLEN] = "\0";
        snprintf(argv0, sizeof(argv0), "as %s -o %s.o", asm_file_name0,
                 base_file_name0);
        log_debug("%s", base_file_name0);

        fflush(stdout);
        fflush(stderr);
        FILE* as_process = popen(argv0, "r");
        if (as_process == NULL) {
            res = RES_FAILED_AS;
            fprintf(stderr, res_to_str[res], argv0, strerror(errno));
            return res;
        }
        if (pclose(as_process) != 0) {
            res = RES_FAILED_AS;
            fprintf(stderr, res_to_str[res], argv0, strerror(errno));
            return res;
        }
        fflush(stdout);
        fflush(stderr);
    }

    // ld
    {
        static u8 argv0[3 * MAXPATHLEN] = "\0";
        snprintf(argv0, sizeof(argv0), "ld %s.o -lSystem -o %s -e _main",
                 base_file_name0, base_file_name0);
        log_debug("%s", argv0);

        fflush(stdout);
        fflush(stderr);
        FILE* ld_process = popen(argv0, "r");
        if (ld_process == NULL) {
            res = RES_FAILED_LD;
            fprintf(stderr, res_to_str[res], argv0, strerror(errno));
            return res;
        }
        if (pclose(ld_process) != 0) {
            res = RES_FAILED_LD;
            fprintf(stderr, res_to_str[res], argv0, strerror(errno));
            return res;
        }
        fflush(stdout);
        fflush(stderr);
    }
    log_debug("created executable `%s`", base_file_name0);

    return RES_OK;
}


#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "codegen.h"

static bool is_file_name_valid(const char* file_name0) {
    const int len = strlen(file_name0);
    return (len > (3 + 1) && memcmp(&file_name0[len - 4], ".kts", 3UL) == 0);
}

static void base_source_file_name(const char* file_name0,
                                  char* base_file_name0) {
    CHECK(is_file_name_valid(file_name0), ==, true, "%d");

    const int len = strlen(file_name0);

    base_file_name0[len - 4] = 0;
    base_file_name0[len - 3] = 0;
    base_file_name0[len - 2] = 0;
    base_file_name0[len - 1] = 0;
}

static res_t run(const char* file_name0) {
    CHECK((void*)file_name0, !=, NULL, "%p");

    static char base_file_name0[MAXPATHLEN + 1] = "";
    static char argv0[3 * MAXPATHLEN] = "";

    res_t res = RES_NONE;
    if (!is_file_name_valid(file_name0)) {
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
    const int file_size = ftell(file);

    const char* source =
        mmap(NULL, (size_t)file_size, PROT_READ, MAP_SHARED, fileno(file), 0);
    if (source == MAP_FAILED) {
        res = RES_SOURCE_FILE_READ_FAILED;
        fprintf(stderr, res_to_str[res], file_name0, strerror(errno));
        return res;
    }

    parser_t parser = {0};
    if ((res = parser_init(file_name0, source, file_size, &parser)) != RES_OK)
        return res;

    if ((res = parser_parse(&parser)) != RES_OK) return res;

    const int file_name_len = strlen(file_name0);
    char asm_file_name0[MAXPATHLEN + 1] = "\0";
    memcpy(asm_file_name0, file_name0, (size_t)file_name_len);
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

    emit(&parser, asm_file);
    // Make sure everything has been written to the file and not simply buffered
    fflush(asm_file);
    fclose(file);

    // as
    memset(base_file_name0, 0, sizeof(base_file_name0));
    memcpy(base_file_name0, file_name0, (size_t)file_name_len);
    base_source_file_name(file_name0, base_file_name0);
    {
        memset(argv0, 0, sizeof(argv0));
        snprintf(argv0, sizeof(argv0), AS " %s -o %s.o", asm_file_name0,
                 base_file_name0);
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
        const char asan_opts[] =
            " "
#if WITH_ASAN == 1 && defined(__APPLE__)
            "-lclang_rt.asan_osx_dynamic -L " ASAN_DIR " -rpath " ASAN_DIR " "
#elif WITH_ASAN == 1
            "-lclang_rt.asan-x86_64 -L " ASAN_DIR " -rpath " ASAN_DIR " "
#endif
            ;

        const char link_opts[] =
            "-fPIC -pie "
#ifdef __APPLE__
            " -lSystem "
#endif
            ;

        memset(argv0, 0, sizeof(argv0));
        snprintf(argv0, sizeof(argv0),
                 LD " %s.o stdlib.o -o %s.exe -e %sstart %s %s",
                 base_file_name0, base_file_name0, name_prefix, asan_opts,
                 link_opts);
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
    log_debug("created executable `%s.exe`", base_file_name0);

    return RES_OK;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 0;
    };

    if (run(argv[1]) != RES_OK) return 1;
}

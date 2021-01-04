#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "codegen.h"

static bool is_file_name_valid(const char* file_name0) {
    const char suffix[] = ".kt";
    const int suffix_len = sizeof(suffix) - 1;
    const int len = strlen(file_name0);

    return (len > suffix_len) &&
           memcmp(&file_name0[len - suffix_len], suffix, suffix_len) == 0;
}

static char* stdlib_obj_path() {
    struct stat st = {0};
    if (stat("mkt_stdlib.o", &st) == 0) return "mkt_stdlib.o";
    if (stat("/usr/local/lib/mkt_stdlib.o", &st) == 0)
        return "/usr/local/lib/mkt_stdlib.o";

    return NULL;
}

static void base_source_file_name(const char* file_name0,
                                  char* base_file_name0) {
    CHECK(is_file_name_valid(file_name0), ==, true, "%d");

    const int len = strlen(file_name0);

    base_file_name0[len - 3] = 0;
    base_file_name0[len - 2] = 0;
    base_file_name0[len - 1] = 0;
}

static int run(const char* file_name0) {
    CHECK((void*)file_name0, !=, NULL, "%p");

    static char base_file_name0[MAXPATHLEN + 1] = "";
    static char argv0[3 * MAXPATHLEN] = "";

    mkt_res_t res = RES_NONE;
    if (!is_file_name_valid(file_name0)) {
        res = RES_INVALID_SOURCE_FILE_NAME;
        fprintf(stderr, "Invalid source file name %s, must end with `.kt`\n",
                file_name0);
        return res;
    }

    struct stat st = {0};
    if (stat(file_name0, &st) != 0) {
        fprintf(stderr, "Failed to `stat(2)` the source file %s: %s\n",
                file_name0, strerror(errno));
        return errno;
    }

    const int file_size = st.st_size;

    char* const source = malloc(file_size);
    if (source == NULL) return ENOMEM;

    int file = open(file_name0, O_RDONLY);
    if (file == -1) {
        fprintf(stderr, "Failed to `open(2)` the source file %s: %s\n",
                file_name0, strerror(errno));
        return errno;
    }
    const ssize_t read_bytes = read(file, source, file_size);
    if (read_bytes == -1) {
        fprintf(stderr, "Failed to `read(2)` the source file %s: %s\n",
                file_name0, strerror(errno));
        return errno;
    }
    if (read_bytes != file_size) {
        fprintf(stderr,
                "Failed to fully `read(2)` the source file %s: bytes to "
                "read=%d, bytes read=%zd errno=%d err=%s\n",
                file_name0, file_size, read_bytes, errno, strerror(errno));
        return errno ? errno : EIO;
    }
    close(file);

    parser_t parser = {0};
    if ((res = parser_init(file_name0, source, file_size, &parser)) != RES_OK)
        return res;

    if ((res = parser_parse(&parser)) != RES_OK) return res;

    for (int i = 0; i < (int)buf_size(parser.par_node_decls); i++)
        node_dump(&parser, parser.par_node_decls[i], 0);

    const int file_name_len = strlen(file_name0);
    char asm_file_name0[MAXPATHLEN + 1] = "";
    snprintf(asm_file_name0, MAXPATHLEN, "%.*s.asm", (int)(file_name_len - 3),
             file_name0);
    FILE* asm_file = fopen(asm_file_name0, "w");
    if (asm_file == NULL) {
        res = RES_ASM_FILE_READ_FAILED;
        fprintf(stderr, "Failed to read asm file %s: %s\n", asm_file_name0,
                strerror(errno));
        return res;
    }

    log_debug("writing asm output to `%s`", asm_file_name0);

    emit(&parser, asm_file);
    // Make sure everything has been written to the file and not simply buffered
    fflush(asm_file);

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
            fprintf(stderr, "Failed to run `%s`: %s\n", argv0, strerror(errno));
            return res;
        }
        if (pclose(as_process) != 0) {
            res = RES_FAILED_AS;
            fprintf(stderr, "Failed to run `%s`: %s\n", argv0, strerror(errno));
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
            " "
#ifdef __APPLE__
            "-lSystem "
#else
            "-static "
#endif
            ;

        const char* const stdlib = stdlib_obj_path();
        CHECK((void*)stdlib, !=, NULL, "%p");

        memset(argv0, 0, sizeof(argv0));
        snprintf(argv0, sizeof(argv0),
                 LD " %s.o %s -o %s.exe -e " MKT_NAME_PREFIX "start %s %s",
                 base_file_name0, stdlib, base_file_name0, asan_opts,
                 link_opts);
        log_debug("%s", argv0);

        fflush(stdout);
        fflush(stderr);
        FILE* ld_process = popen(argv0, "r");
        if (ld_process == NULL) {
            res = RES_FAILED_LD;
            fprintf(stderr, "Failed to run `%s`: %s\n", argv0, strerror(errno));
            return res;
        }
        if (pclose(ld_process) != 0) {
            res = RES_FAILED_LD;
            fprintf(stderr, "Failed to run `%s`: %s\n", argv0, strerror(errno));
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
        printf("microkt: Tiny Kotlin compiler\nUsage: %s <file>\n", argv[0]);
        return 0;
    };
    is_tty = isatty(2);

    int err = 0;
    if ((err = run(argv[1])) != RES_OK) return err;
}

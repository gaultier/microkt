#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "buf.h"
#include "common.h"

#define LENGTH (1L << 11)

typedef struct {
    size_t str_len;
    const char* str_s;
} str;

static mkt_res_t proc_run(const char* exe_name, char output[LENGTH],
                          size_t* read_bytes, i32* ret_code) {
    CHECK((void*)exe_name, !=, NULL, "%p");
    CHECK((void*)read_bytes, !=, NULL, "%p");
    CHECK((void*)ret_code, !=, NULL, "%p");

    FILE* exe_process = popen(exe_name, "r");
    if (exe_process == NULL) {
        fprintf(stderr, "Error launching `%s`: errno=%d err=%s\n", exe_name,
                errno, strerror(errno));
        return RES_ERR;
    }

    *read_bytes = fread(output, 1, LENGTH, exe_process);
    const i32 err = ferror(exe_process);
    if (err) {
        fprintf(stderr, "Error reading output of `%s`: errno=%d err=%s\n",
                exe_name, errno, strerror(errno));
        return RES_ERR;
    }
    CHECK(*read_bytes, <=, LENGTH, "%zu");

    fflush(exe_process);
    *ret_code = pclose(exe_process);

    return RES_OK;
}

static str* expects_from_string(const char* string, size_t string_len) {
    CHECK((void*)string, !=, NULL, "%p");
    CHECK(string_len, >, 0UL, "%lu");

    const char needle[] = "// expect: ";
    const size_t needle_len = sizeof(needle) - 1;
    str* expects = NULL;
    buf_grow(expects, 100);
    const char* src = string;
    const char* const string_end = string + string_len;

    while (src < string + string_len - needle_len) {
        src = memchr(src, '/', string_end - src);
        if (!src) break;

        if (memcmp(src, needle, needle_len) == 0) {
            src += needle_len;
            const char* end = memchr(src, '\n', string_end - src);
            if (!end) end = string + string_len;
            CHECK((void*)src, <, (void*)end, "%p");

            buf_push(expects, ((str){.str_s = src, .str_len = end - src}));
            src = end + 1;
        } else
            src += 1;
    }

    return expects;
}

static mkt_res_t simple_test_run(const char* source_file_name) {
    const size_t source_file_name_len = strlen(source_file_name);
    CHECK(source_file_name_len, >, 1L + 3, "%zu");
    CHECK(source_file_name_len, <, (size_t)MAXPATHLEN, "%zu");
    CHECK(source_file_name[source_file_name_len - 3], ==, '.', "%c");
    CHECK(source_file_name[source_file_name_len - 2], ==, 'k', "%c");
    CHECK(source_file_name[source_file_name_len - 1], ==, 't', "%c");

    fprintf(stderr, "%s| %s%s\n", mkt_colors[is_tty][COL_GRAY],
            source_file_name, mkt_colors[is_tty][COL_RESET]);

    char argv[MAXPATHLEN] = "";
    snprintf(argv, MAXPATHLEN, "%.*s.exe", (i32)(source_file_name_len - 3),
             source_file_name);

    struct stat st = {0};
    if (stat(source_file_name, &st) != 0) {
        fprintf(stderr, "Failed to `stat(2)` the source file %s: %s\n",
                source_file_name, strerror(errno));
        return errno;
    }

    const i32 file_size = st.st_size;

    char* const source = malloc(file_size);
    if (source == NULL) return ENOMEM;

    i32 file = open(source_file_name, O_RDONLY);
    if (file == -1) {
        fprintf(stderr, "Failed to `open(2)` the source file %s: %s\n",
                source_file_name, strerror(errno));
        return errno;
    }
    const ssize_t read_bytes = read(file, source, file_size);
    if (read_bytes == -1) {
        fprintf(stderr, "Failed to `read(2)` the source file %s: %s\n",
                source_file_name, strerror(errno));
        return errno;
    }
    if (read_bytes != file_size) {
        fprintf(stderr,
                "Failed to fully `read(2)` the source file %s: bytes to "
                "read=%d, bytes read=%zd errno=%d err=%s\n",
                source_file_name, file_size, read_bytes, errno,
                strerror(errno));
        return errno ? errno : EIO;
    }
    close(file);

    const str* const expects = expects_from_string(source, read_bytes);
    const size_t expects_count = buf_size(expects);

    char output[LENGTH] = "";
    i32 ret_code = 0;
    size_t proc_read_bytes = 0;
    if (proc_run(argv, output, &proc_read_bytes, &ret_code) != RES_OK)
        return RES_ERR;
    if (ret_code != 0) {
        fprintf(stderr, "%s✘ %s:%s ret_code=%d\n", mkt_colors[is_tty][COL_RED],
                source_file_name, mkt_colors[is_tty][COL_RESET], ret_code);
        return RES_ERR;
    }

    const char* out = output;
    size_t line = 0;
    bool differed = false;
    while (out < output + proc_read_bytes) {
        const char* end = strchr(out, '\n');
        if (!end) end = output + proc_read_bytes;

        CHECK((void*)out, <=, (void*)end, "%p");
        const size_t out_len = end - out;
        const str expect_line = (line >= expects_count)
                                    ? ((str){.str_len = 0, .str_s = ""})
                                    : expects[line];

        if (expect_line.str_len != out_len ||
            memcmp(expect_line.str_s, out, out_len) != 0) {
            fprintf(stderr,
                    "%s"
                    "✘ %s #%lu: [expected]%s len=%zu str=%.*s%s\n"
                    "✘ %s #%lu: [actual  ]%s len=%zu str=%.*s\n",
                    mkt_colors[is_tty][COL_RED], source_file_name, line + 1,
                    mkt_colors[is_tty][COL_RESET], expect_line.str_len,
                    (i32)expect_line.str_len, expect_line.str_s,
                    mkt_colors[is_tty][COL_RED], source_file_name, line + 1,
                    mkt_colors[is_tty][COL_RESET], out_len, (i32)out_len, out);
            differed = true;
        }
        out = end + 1;
        line += 1;
    }

    if (!differed)
        fprintf(stderr, "%s✔ %s%s\n", mkt_colors[is_tty][COL_GREEN],
                source_file_name, mkt_colors[is_tty][COL_RESET]);

    return differed ? RES_ERR : RES_OK;
}

static mkt_res_t err_test_run(const char* source_file_name) {
    const size_t source_file_name_len = strlen(source_file_name);
    CHECK(source_file_name_len, >, 1L + 3, "%zu");
    CHECK(source_file_name_len, <, (size_t)MAXPATHLEN, "%zu");
    CHECK(source_file_name[source_file_name_len - 3], ==, '.', "%c");
    CHECK(source_file_name[source_file_name_len - 2], ==, 'k', "%c");
    CHECK(source_file_name[source_file_name_len - 1], ==, 't', "%c");

    char argv[MAXPATHLEN] = "";
    snprintf(argv, MAXPATHLEN, "./mktc %s", source_file_name);

    char output[LENGTH] = "";
    size_t read_bytes = 0;
    i32 ret_code = 0;
    if (proc_run(argv, output, &read_bytes, &ret_code) != RES_OK)
        return RES_ERR;
    if (ret_code == 0) {
        fprintf(stderr, "%s✘ %s:%s ret_code=%d\n", mkt_colors[is_tty][COL_RED],
                source_file_name, mkt_colors[is_tty][COL_RESET], ret_code);
        return RES_ERR;
    } else
        fprintf(stderr, "%s✔ %s%s\n", mkt_colors[is_tty][COL_GREEN],
                source_file_name, mkt_colors[is_tty][COL_RESET]);

    return RES_OK;
}

i32 main() {
    is_tty = isatty(2);

    const char simple_tests[][MAXPATHLEN] = {
        "./tests/assign.kt",
        "./tests/bool.kt",
        "./tests/char.kt",
        "./tests/class.kt",
        "./tests/comparison.kt",
        "./tests/fibo_iter.kt",
        "./tests/fibonacci_rec.kt",
        "./tests/fn.kt",
        "./tests/grouping.kt",
        "./tests/hello_world.kt",
        "./tests/if.kt",
        "./tests/integers.kt",
        "./tests/math_integers.kt",
        "./tests/negation.kt",
        "./tests/string.kt",
        "./tests/var.kt",
        "./tests/while.kt",
    };
    const char err_tests[][MAXPATHLEN] = {
        "./err/empty.kt",
        "./err/fn_mismatched_types.kt",
        "./err/fn_missing_return.kt",
        "./err/invalid_token.kt",
        "./err/member_get_non_instance.kt",
        "./err/missing_param_println.kt",
        "./err/multiplication_type.kt",
        "./err/non_matching_types.kt",
        "./err/non_top_level_declaration.kt",
        "./err/unexpected_token.kt",
        "./err/unexpected_token.kt",
        "./err/unexpected_token_on_first_line.kt",
        "./err/unknown_type.kt",
        "./err/val_assign.kt",
        "./err/var_undefined.kt",
        "err/non_matching_fn_return_type.kt",
        "err/non_matching_fn_return_type_unit.kt",
        "err/non_matching_type_class_member_assign.kt",
        "err/return_not_in_fn.kt",
    };

    bool failed = false;
    for (size_t i = 0; i < sizeof(simple_tests) / sizeof(simple_tests[0]);
         i++) {
        if (simple_test_run(simple_tests[i]) != RES_OK) failed = true;
    }

    for (size_t i = 0; i < sizeof(err_tests) / sizeof(err_tests[0]); i++) {
        if (err_test_run(err_tests[i]) != RES_OK) failed = true;
    }
    return failed;
}


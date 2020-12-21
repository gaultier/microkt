#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "buf.h"
#include "common.h"

bool is_tty = true;
#define LENGTH (1L << 11)

typedef struct {
    size_t str_len;
    const char* str_s;
} str;

static res_t proc_run(const char* exe_name, char output[LENGTH],
                      size_t* read_bytes, int* ret_code) {
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
    const int err = ferror(exe_process);
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
    while (src < string + string_len - needle_len) {
        src = strchr(src, '/');
        if (!src) break;

        if (memcmp(src, needle, needle_len) == 0) {
            src += needle_len;
            const char* end = strchr(src, '\n');
            if (!end) end = string + string_len;
            CHECK((void*)src, <, (void*)end, "%p");

            buf_push(expects, ((str){.str_s = src, .str_len = end - src}));
            src = end + 1;
        } else
            src += 1;
    }

    return expects;
}

static bool simple_test_run(const char* source_file_name) {
    const size_t source_file_name_len = strlen(source_file_name);
    CHECK(source_file_name_len, >, 1L + 3, "%zu");
    CHECK(source_file_name_len, <, (size_t)MAXPATHLEN, "%zu");
    CHECK(source_file_name[source_file_name_len - 1], ==, 's', "%c");
    CHECK(source_file_name[source_file_name_len - 2], ==, 't', "%c");
    CHECK(source_file_name[source_file_name_len - 3], ==, 'k', "%c");
    CHECK(source_file_name[source_file_name_len - 4], ==, '.', "%c");

    char argv[MAXPATHLEN] = "";
    memcpy(argv, source_file_name, source_file_name_len);
    argv[source_file_name_len - 1] = 'e';
    argv[source_file_name_len - 2] = 'x';
    argv[source_file_name_len - 3] = 'e';

    FILE* source_file = fopen(source_file_name, "r");
    if (!source_file) {
        fprintf(stderr, "Error reading file `%s`: err=%d errno=%s\n",
                source_file_name, errno, strerror(errno));
        return false;
    }
    char source_file_content[LENGTH] = "";
    size_t read_bytes = fread(source_file_content, 1, LENGTH, source_file);
    if (ferror(source_file)) {
        fprintf(stderr, "Error reading content of `%s`: errno=%d err=%s\n",
                source_file_name, errno, strerror(errno));
        return false;
    }
    CHECK(read_bytes, <=, LENGTH, "%zu");

    const str* const expects =
        expects_from_string(source_file_content, read_bytes);
    const size_t expects_count = buf_size(expects);

    char output[LENGTH] = "";
    int ret_code = 0;
    if (proc_run(argv, output, &read_bytes, &ret_code) != RES_OK) return false;
    if (ret_code != 0) {
        fprintf(stderr, "%s✘ %s:%s ret_code=%d\n", is_tty ? color_red : "",
                source_file_name, is_tty ? color_reset : "", ret_code);
        return false;
    }

    const char* out = output;
    size_t line = 0;
    bool differed = false;
    while (out < output + read_bytes) {
        const char* end = strchr(out, '\n');
        if (!end) end = output + read_bytes;

        CHECK((void*)out, <, (void*)end, "%p");
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
                    is_tty ? color_red : "", source_file_name, line + 1,
                    is_tty ? color_reset : "", expect_line.str_len,
                    (int)expect_line.str_len, expect_line.str_s,
                    is_tty ? color_red : "", source_file_name, line + 1,
                    is_tty ? color_reset : "", out_len, (int)out_len, out);
            differed = true;
        }
        out = end + 1;
        line += 1;
    }

    if (!differed)
        printf("%s✔ %s%s\n", is_tty ? color_green : "", source_file_name,
               is_tty ? color_reset : "");

    return !differed;
}

static bool err_test_run(const char* source_file_name) {
    const size_t source_file_name_len = strlen(source_file_name);
    CHECK(source_file_name_len, >, 1L + 3, "%zu");
    CHECK(source_file_name_len, <, (size_t)MAXPATHLEN, "%zu");
    CHECK(source_file_name[source_file_name_len - 1], ==, 's', "%c");
    CHECK(source_file_name[source_file_name_len - 2], ==, 't', "%c");
    CHECK(source_file_name[source_file_name_len - 3], ==, 'k', "%c");
    CHECK(source_file_name[source_file_name_len - 4], ==, '.', "%c");

    char argv[MAXPATHLEN] = "";
    snprintf(argv, MAXPATHLEN, "./mktc %s", source_file_name);

    char output[LENGTH] = "";
    size_t read_bytes = 0;
    int ret_code = 0;
    if (proc_run(argv, output, &read_bytes, &ret_code) != RES_OK) return false;
    if (ret_code == 0) {
        fprintf(stderr, "%s✘ %s:%s ret_code=%d\n", is_tty ? color_red : "",
                source_file_name, is_tty ? color_reset : "", ret_code);
        return false;
    } else
        printf("%s✔ %s%s\n", is_tty ? color_green : "", source_file_name,
               is_tty ? color_reset : "");

    return true;
}

int main() {
    is_tty = isatty(2);

    const char* simple_tests[] = {
        "tests/assign.kts",      "tests/bool.kts",
        "tests/char.kts",        "tests/comparison.kts",
        "tests/fibo_iter.kts",   "tests/fibonacci_rec.kts",
        "tests/fn.kts",          "tests/grouping.kts",
        "tests/hello_world.kts", "tests/if.kts",
        "tests/integers.kts",    "tests/math_integers.kts",
        "tests/negation.kts",    "tests/string.kts",
        "tests/var.kts",         "tests/while.kts",
    };
    const char* err_tests[] = {
        "err/err_fn_missing_return.kts",
        "err/err_multiplication_type.kts",
        "err/err_non_matching_types.kts",
        "err/err_unexpected_token.kts",
        "err/err_unexpected_token_on_first_line.kts",
        "err/error_empty.kts",
        "err/error_unexpected_token.kts",
        "err/fn_mismatched_types.kts",
        "err/invalid_token.kts",
        "err/missing_param_println.kts",
        "err/val_assign.kts",
    };

    bool failed = false;
    for (size_t i = 0; i < sizeof(simple_tests) / sizeof(simple_tests[0]);
         i++) {
        if (!simple_test_run(simple_tests[i])) failed = true;
    }

    for (size_t i = 0; i < sizeof(err_tests) / sizeof(err_tests[0]); i++) {
        if (!err_test_run(err_tests[i])) failed = true;
    }
    return failed;
}


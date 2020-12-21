#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "buf.h"
#include "common.h"

bool is_tty = true;

bool test_run(const char* source_file_name) {
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
    char source_file_content[1 << 11];
    size_t read_bytes =
        fread(source_file_content, 1, sizeof(source_file_content), source_file);
    if (ferror(source_file)) {
        fprintf(stderr, "Error reading content of `%s`: errno=%d err=%s\n",
                source_file_name, errno, strerror(errno));
        return false;
    }
    CHECK(read_bytes, <=, sizeof(source_file_content), "%zu");

    const size_t len = read_bytes;
    const char needle[] = "// expect: ";
    const size_t needle_len = sizeof(needle) - 1;
    const char** expects = NULL;
    const char* src = source_file_content;
    while (src < source_file_content + len - needle_len) {
        src = strchr(src, '/');
        if (!src) break;

        if (memcmp(src, needle, needle_len) == 0) {
            src += needle_len;
            char* end = strchr(src, '\n');
            if (!end) end = source_file_content + read_bytes;
            *end = '\0';
            buf_push(expects, src);
            src = end + 1;
        } else
            src += 1;
    }
    const size_t expects_count = buf_size(expects);

    FILE* exe_process = popen(argv, "r");
    if (exe_process == NULL) {
        fprintf(stderr, "Error launching `%s`: errno=%d err=%s\n", argv, errno,
                strerror(errno));
        return false;
    }

    char output[1 << 11];
    read_bytes = fread(output, 1, sizeof(output), exe_process);
    if (ferror(exe_process)) {
        fprintf(stderr, "Error reading output of `%s`: errno=%d err=%s\n", argv,
                errno, strerror(errno));
        return false;
    }
    CHECK(read_bytes, <=, sizeof(output), "%zu");

    if (pclose(exe_process) != 0) {
        fprintf(stderr, "Error closing process %s: errno=%d error=%s\n", argv,
                errno, strerror(errno));
        return false;
    }

    char* out = output;
    size_t line = 0;
    bool differed = false;
    while (out < output + read_bytes) {
        char* end = strchr(out, '\n');
        if (!end) end = output + read_bytes;
        *end = '\0';

        const char* expect_line = (line >= expects_count) ? "" : expects[line];
        if (strcmp(expect_line, out) != 0) {
            fprintf(stderr,
                    "%s"
                    "✘ %s #%lu: [expected] %s%s%s\n"
                    "✘ %s #%lu: [actual  ] %s%s\n",
                    is_tty ? color_red : "", source_file_name, line + 1,
                    is_tty ? color_reset : "", expect_line,
                    is_tty ? color_red : "", source_file_name, line + 1,
                    is_tty ? color_reset : "", out);
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
        if (!test_run(simple_tests[i])) failed = true;
    }

    for (size_t i = 0; i < sizeof(err_tests) / sizeof(err_tests[0]); i++) {
        if (test_run(err_tests[i])) failed = true;
    }
    return failed;
}


#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "buf.h"
#include "common.h"

bool is_tty = true;

bool test() {
    const char* source_file_name = "./tests/string.kts";
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

    const char argv[] = "./tests/string.exe";
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
        return false;
    }

    char* out = output;
    size_t line = 0;
    bool differed = false;
    while (out < output + read_bytes) {
        char* end = strchr(out, '\n');
        if (!end) end = output + read_bytes;
        *end = '\0';

        if (strcmp(expects[line], out) != 0) {
            fprintf(stderr,
                    "%s"
                    "✘ %s #%lu: [expected] %s%s%s\n"
                    "✘ %s #%lu: [actual  ] %s%s\n",
                    is_tty ? color_red : "", source_file_name, line + 1,
                    is_tty ? color_reset : "", expects[line],
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
    if (!test()) return 1;
}


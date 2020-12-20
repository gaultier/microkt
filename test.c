#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

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

    printf("%.*s", (int)read_bytes, output);

    if (pclose(exe_process) != 0) {
        return false;
    }
    fflush(stdout);
    fflush(stderr);

    return true;
}

int main() {
    if (!test()) return 1;
}


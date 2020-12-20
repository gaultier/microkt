#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

bool test() {
    /* const char* source_file_name = "./tests/string.kts"; */
    /* FILE* source_file = NULL;  // FIXME */

    const char argv0[] = "./tests/string.exe";
    FILE* exe_process = popen(argv0, "r");
    if (exe_process == NULL) {
        fprintf(stderr, "Error launching `%s`: errno=%d err=%s\n", argv0, errno,
                strerror(errno));
        return false;
    }

    char buf[1 << 11];
    const size_t read_bytes = fread(buf, 1, sizeof(buf), exe_process);
    if (ferror(exe_process)) {
        fprintf(stderr, "Error reading output of `%s`: errno=%d err=%s\n",
                argv0, errno, strerror(errno));
        return false;
    }
    CHECK(read_bytes, <=, sizeof(buf), "%zu");

    printf("%.*s", (int)read_bytes, buf);

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


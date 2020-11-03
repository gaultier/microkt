#include "common.h"
#include "driver.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 0;
    };

    if (driver_run(argv[1]) != RES_OK) return 1;
}

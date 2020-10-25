#include "driver.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 0;
    };

    driver_run(argv[1]);
}

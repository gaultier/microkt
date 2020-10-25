#pragma once

#include "common.h"
#include "parse.h"

res_t driver_run(const u8* file_name0) {
    PG_ASSERT_COND(file_name0, !=, NULL, "%p");

    parser_t parser = parser_init(file_name0, "", 0);

    return RES_OK;
}

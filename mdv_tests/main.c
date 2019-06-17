#include "minunit.h"
#include "mdv_platform.h"
#include <mdv_alloc.h>


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    mdv_alloc_initialize();
    MU_RUN_SUITE(platform);
    MU_REPORT();
    mdv_alloc_finalize();
    return minunit_status;
}


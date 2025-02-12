#include "mdv_binn.h"
#include "mdv_alloc.h"


static void * mdv_alloc_binn(size_t size)
{
    return mdv_alloc(size, "binn");
}


static void * mdv_realloc_binn(void *ptr, size_t size)
{
    return mdv_realloc(ptr, size, "binn");
}


static void mdv_free_binn(void *ptr)
{
    mdv_free(ptr, "binn");
}


void mdv_binn_set_allocator()
{
    binn_set_alloc_functions(&mdv_alloc_binn,
                             &mdv_realloc_binn,
                             &mdv_free_binn);
}

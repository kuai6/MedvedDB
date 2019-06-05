#pragma once
#include <stddef.h>
#include <string.h>


typedef struct
{
    size_t  length;
    char   *data;
} mdv_string_t;


#define mdv_str_is_empty(str)   !(str).length
#define mdv_str_static(str)     { sizeof(str) - 1, str }
#define mdv_str(str)            { strlen(str), str }
#define mdv_str_null            { 0, 0 }

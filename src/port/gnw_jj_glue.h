/*
 * Forced include (-include) of the OpenJazz, miniz and stb sources of the
 * port: GNW is defined, and the C allocation functions of the C libraries
 * use the game heap (jj_alloc.c). OpenJazz itself only uses new / delete
 * (jj_cxx.cpp): no macros in C++, they would break the C++ library
 * (std::realloc...).
 */
#ifndef GNW_JJ_GLUE_H
#define GNW_JJ_GLUE_H

#ifndef GNW
#define GNW 1
#endif

#include <stdlib.h>
#include <string.h>
#include "jj_platform.h"

#ifndef __cplusplus
#define malloc(n)     jj_malloc(n)
#define calloc(n, s)  jj_calloc(n, s)
#define realloc(p, n) jj_realloc(p, n)
#define free(p)       jj_free(p)
#endif

#endif

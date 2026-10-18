#ifdef GNW
/* Game & Watch port: a stable sort instead of qsort, so that the font
 * atlases are packed the same way on every computer and on the console
 * (the pictures are matched by their pixels: src/port/jj_bake.c). */
#include <string.h>
static void gnw_stable_sort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
	unsigned char *b = (unsigned char *)base;
	unsigned char tmp[64];
	if (size > sizeof(tmp))
		return;
	for (size_t i = 1; i < n; i++) {
		memcpy(tmp, b + i * size, size);
		size_t j = i;
		while (j > 0 && cmp(b + (j - 1) * size, tmp) > 0) {
			memcpy(b + j * size, b + (j - 1) * size, size);
			j--;
		}
		memcpy(b + j * size, tmp, size);
	}
}
#define STBRP_SORT gnw_stable_sort
#endif
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

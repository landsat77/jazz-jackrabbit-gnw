#ifndef JJ_PACK_H
#define JJ_PACK_H

#include <stdint.h>
#include <stdbool.h>

#define JJ_PACK_MAGIC "GWJJ01\0\0"

enum {
    JJ_PACK_OK = 0,
    JJ_PACK_ERR_MAGIC = -1,
    JJ_PACK_ERR_CORRUPT = -2,
};

#ifdef __cplusplus
extern "C" {
#endif

int jj_pack_open(const uint8_t *data, uint32_t size);
bool jj_pack_find(const char *name, const uint8_t **data, uint32_t *size);
uint32_t jj_pack_count(void);
const char *jj_pack_name(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif

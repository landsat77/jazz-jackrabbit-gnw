/*
 * Files of OpenJazz on the Game & Watch (io/file.cpp): the game data is read
 * in place from the asset pack (external flash, jj_plat_find_file()), the
 * other files (configuration, saved games) are real files in
 * jj_plat_save_dir().
 *
 * io/file.cpp includes this under GNW: its stdio calls become jjf_*.
 */
#ifndef JJ_FILE_H
#define JJ_FILE_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JJFILE {
    const uint8_t *mem;  /* pack file (read only), NULL: real file */
    uint32_t size;
    uint32_t pos;
    FILE *fp;
} JJFILE;

JJFILE *jjf_open(const char *path, const char *mode);
int     jjf_close(JJFILE *f);
long    jjf_tell(JJFILE *f);
int     jjf_seek(JJFILE *f, long offset, int whence);
size_t  jjf_read(void *buf, size_t size, size_t n, JJFILE *f);
size_t  jjf_write(const void *buf, size_t size, size_t n, JJFILE *f);
int     jjf_getc(JJFILE *f);
int     jjf_putc(int c, JJFILE *f);
int     jjf_access(const char *path, int mode);
/* pack files only: the data at the current position (NULL otherwise) */
const uint8_t *jjf_mapped(JJFILE *f);

#ifdef __cplusplus
}
#endif

#ifdef JJ_FILE_RENAME
#define fopen  jjf_open
#define fclose jjf_close
#define ftell  jjf_tell
#define fseek  jjf_seek
#define fread  jjf_read
#define fwrite jjf_write
#define fgetc  jjf_getc
#define fputc  jjf_putc
#define access jjf_access
#endif

#endif

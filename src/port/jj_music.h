/* Pre-rendered music pieces (jj_music.c) */
#ifndef JJ_MUSIC_H
#define JJ_MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct jj_mus jj_mus_t;

/* A "GWMU" piece in memory (read in place), NULL if invalid. */
jj_mus_t *jj_mus_open(const unsigned char *data, int len);
void jj_mus_close(jj_mus_t *f);
int jj_mus_rewind(jj_mus_t *f);
/* Adds the next `frames` frames at `out_rate` to `out` (interleaved,
 * `channels` channels) when `audible`. The piece loops forever (from its
 * loop start): returns `frames`. */
int jj_mus_mix(jj_mus_t *f, short *out, int frames, int channels, int audible, int out_rate);

#ifdef __cplusplus
}
#endif

#endif

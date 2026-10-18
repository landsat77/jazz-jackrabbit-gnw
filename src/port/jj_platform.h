/*
 * Interface between OpenJazz (src/oj + the mini SDL of src/port/sdl) and the
 * platform it runs on:
 *  - src/port/jj_main.c : the Game & Watch homebrew (retro-go)
 *  - harness/jj_host.c  : a headless Linux harness used to test the port
 */
#ifndef JJ_PLATFORM_H
#define JJ_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Screen of the port (OpenJazz draws at any size: the whole LCD) */
#define JJ_WIDTH  320
#define JJ_HEIGHT 240

/* Buttons of the console, as seen by the core (jj_core_set_buttons()) */
enum {
    JJ_BTN_LEFT   = 1 << 0,
    JJ_BTN_RIGHT  = 1 << 1,
    JJ_BTN_UP     = 1 << 2,
    JJ_BTN_DOWN   = 1 << 3,
    JJ_BTN_A      = 1 << 4,
    JJ_BTN_B      = 1 << 5,
    JJ_BTN_GAME   = 1 << 6,   /* START on Zelda units */
    JJ_BTN_TIME   = 1 << 7,   /* SELECT on Zelda units */
};

/* ---- implemented by the platform ---- */

/* Monotonic time in milliseconds. */
uint32_t jj_plat_time_ms(void);

/* Called while the game waits (in_delay = 1, SDL_Delay) and when it reads
 * its events (in_delay = 0, SDL_PollEvent). The platform feeds its audio
 * output with jj_core_audio_render(), reads its buttons and gives them to
 * jj_core_set_buttons(). */
void jj_plat_service(int in_delay);

/* Show the canvas (w x h palette indexes, `pitch` bytes per line) with
 * this palette (256 x R, G, B, unused; `pal_version` changes with it). */
void jj_plat_present(const uint8_t *pixels, int w, int h, int pitch,
                     const uint8_t *palette, uint32_t pal_version);

/* Output sample rate (mono, signed 16 bit). */
int jj_plat_audio_rate(void);

/* Look up a file of the asset pack (upper case names: "LEVEL0.000", ...).
 * The returned memory is read-only and stays valid while the game runs. */
bool jj_plat_find_file(const char *name, const uint8_t **data, uint32_t *size);

/* Directory (without trailing '/') where the game may write its files
 * (configuration, saved games). */
const char *jj_plat_save_dir(void);

/* Memory outside the heap for the canvas (a whole screen of bytes), NULL:
 * use the heap. */
void *jj_plat_canvas_memory(size_t size);

/* Unrecoverable error. Never returns. */
void jj_plat_fatal(const char *msg) __attribute__((noreturn));

/* The game asked to quit (main menu). Never returns. */
void jj_plat_quit(void) __attribute__((noreturn));

/* ---- implemented by the core (jj_core.cpp, jj_sdl.cpp) ---- */

typedef struct {
    bool music;
    bool sound;
    /* start this level directly (-1: normal start with the intro) */
    int start_world;
    int start_level;
    /* harness only: write every picture pre-decoded to this file and quit */
    const char *bake_out;
} jj_core_options_t;

/* Run the game. Never returns. */
void jj_core_run(const jj_core_options_t *opts) __attribute__((noreturn));

/* Buttons currently held (JJ_BTN_*). */
void jj_core_set_buttons(uint32_t buttons);

/* Savestates (jj_state.cpp): the level, its checkpoint and the player.
 * jj_core_load_state() plays it as soon as the game gets control. */
bool jj_core_can_save(void);
bool jj_core_save_state(const char *path);
bool jj_core_load_state(const char *path);

/* Options of the pause menu */
void jj_core_set_sound(bool music, bool sound);

/* Render `frames` mono samples (sound effects and music). */
void jj_core_audio_render(int16_t *out, int frames);

/* ---- implemented by jj_alloc.c ---- */
void   jj_heap_init(void *base, size_t size);
void  *jj_malloc(size_t size);
void  *jj_calloc(size_t n, size_t size);
void  *jj_realloc(void *ptr, size_t size);
void   jj_free(void *ptr);
size_t jj_heap_used(void);
size_t jj_heap_peak(void);
size_t jj_heap_size(void);
void   jj_heap_ignore_range(const void *lo, const void *hi);

#ifdef __cplusplus
}
#endif

#endif

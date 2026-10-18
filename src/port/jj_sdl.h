/* Port side of the mini SDL (jj_sdl.cpp) */
#ifndef JJ_SDL_H
#define JJ_SDL_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

SDL_Surface *jj_sdl_screen(void);
void jj_sdl_set_physical_palette(const SDL_Color *colors, int first, int n);
/* button bit (JJ_BTN_x = 1 << bit) -> one or two keys */
void jj_sdl_map_button(int bit, SDLKey k1, SDLKey k2);
void jj_sdl_set_buttons(uint32_t buttons);
void jj_sdl_request_quit(void);
/* pictures made from these pixels / with these pixels now: in place from
 * the pack if they were baked (jj_bake.c) */
SDL_Surface *jj_sdl_create_baked(const uint8_t *pixels, int w, int h);
void jj_sdl_bake_surface(SDL_Surface *s);
/* a surface on pre-decoded pixels of the pack (read only) */
SDL_Surface *jj_sdl_surface_from_baked(const uint8_t *pixels, int w, int h);
/* harness, option -B: the pictures are being recorded */
bool jj_sdl_bake_recording(void);
/* mono output of the game's audio callback */
void jj_sdl_audio_render(int16_t *out, int frames);

#ifdef __cplusplus
}
#endif

#endif

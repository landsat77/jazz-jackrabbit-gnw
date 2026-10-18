/*
 * Core of the port: starts OpenJazz (oj_gnw_main() in src/oj/main.cpp) and
 * gives it the buttons and the audio of the platform.
 */
#include <SDL.h>
#include <stdio.h>
#include "jj_platform.h"
#include "jj_sdl.h"
#include "jj_sound.h"

int oj_gnw_main(int startWorld, int startLevel);
int oj_gnw_bake(const char *out);

static const jj_core_options_t *g_opts;

extern "C" void jj_core_set_buttons(uint32_t buttons)
{
	jj_sdl_set_buttons(buttons);
}

extern "C" void jj_core_set_sound(bool music, bool sound)
{
	jj_sound_enable(music, sound);
}

extern "C" void jj_core_audio_render(int16_t *out, int frames)
{
	jj_sound_render(out, frames);
}

extern "C" void jj_core_run(const jj_core_options_t *opts)
{
	static jj_core_options_t o;
	o = *opts;
	g_opts = &o;

	jj_sound_enable(o.music, o.sound);

	/* buttons -> keys (src/oj/platforms/gnw.h) */
	jj_sdl_map_button(0, SDLK_LEFT, SDLK_UNKNOWN);
	jj_sdl_map_button(1, SDLK_RIGHT, SDLK_UNKNOWN);
	jj_sdl_map_button(2, SDLK_UP, SDLK_UNKNOWN);
	jj_sdl_map_button(3, SDLK_DOWN, SDLK_UNKNOWN);
	jj_sdl_map_button(4, SDLK_LCTRL, SDLK_UNKNOWN);  /* A: jump, enter, yes */
	jj_sdl_map_button(5, SDLK_LALT, SDLK_UNKNOWN);   /* B: fire, no */
	jj_sdl_map_button(6, SDLK_ESCAPE, SDLK_UNKNOWN); /* GAME: menu / back */
	jj_sdl_map_button(7, SDLK_LSHIFT, SDLK_UNKNOWN); /* TIME: change weapon */

#ifndef JJ_DEVICE
	if (o.bake_out) {
		int b = oj_gnw_bake(o.bake_out);
		printf("jj: bake %s\n", b == 0 ? "done" : "FAILED");
		jj_plat_quit();
	}
#endif
	int r = oj_gnw_main(o.start_world, o.start_level);
	printf("jj: game ended (%d)\n", r);
	jj_plat_quit();
}

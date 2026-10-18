/**
 *
 * @file gnw.h
 *
 * Part of the OpenJazz project
 *
 * @par Licence:
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * @par Description:
 * Game & Watch (Retro-Go SD homebrew), with the mini SDL 1.2 of src/port/sdl.
 *
 */

#ifndef _GNW_H
#define _GNW_H

#ifdef GNW

// Audio config
#define MUSIC_SETTINGS 0 // Low
#define SOUND_FREQ 22050
#define SOUND_SAMPLES 256

// Video config: the whole LCD
#define DEFAULT_SCREEN_WIDTH 320
#define DEFAULT_SCREEN_HEIGHT 240
#define FULLSCREEN_ONLY
#define NO_RESIZE
#define FULLSCREEN_FLAGS 0
#define WINDOWED_FLAGS 0

// Controller config: the buttons of the console are keys (jj_sdl.cpp)
#define NO_CONTROLLER_CFG

// Keyboard config:
//   A = jump / swim / enter / yes, B = fire / no,
//   GAME = escape (menu), TIME = change weapon
#define DEFAULT_KEY_JUMP   (SDLK_LCTRL)
#define DEFAULT_KEY_SWIM   (SDLK_LCTRL)
#define DEFAULT_KEY_FIRE   (SDLK_LALT)
#define DEFAULT_KEY_CHANGE (SDLK_LSHIFT)
#define DEFAULT_KEY_ENTER  (SDLK_LCTRL)
#define DEFAULT_KEY_ESCAPE (SDLK_ESCAPE)
#define DEFAULT_KEY_STATS  (SDLK_TAB)
#define DEFAULT_KEY_PAUSE  (SDLK_p)
#define DEFAULT_KEY_YES    (SDLK_LCTRL)
#define DEFAULT_KEY_NO     (SDLK_LALT)

#endif

#endif

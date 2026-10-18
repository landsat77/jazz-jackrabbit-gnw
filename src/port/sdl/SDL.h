/*
 * Minimal SDL 1.2 for OpenJazz on the Game & Watch (and its Linux harness).
 *
 * Only what OpenJazz uses with SDL 1.2: 8 bit surfaces (palette indexes),
 * blits with a colour key and a per surface palette, keyboard events, a
 * timer, and an audio callback. The platform side is in jj_platform.h.
 *
 * Palettes: OpenJazz keeps every surface's "logical palette" a grey ramp
 * (colour i = i, i, i) and gives a surface another grey palette to remap
 * its indexes (sprite flashing, font colours). A blit therefore writes
 * remap[src] = palette[src].r; the screen's physical palette (the real
 * colours) is only used by the platform to show the canvas.
 */
#ifndef JJ_MINI_SDL_H
#define JJ_MINI_SDL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;

#define OJ_SDL1 1

#define SDL_INIT_TIMER    0x01
#define SDL_INIT_AUDIO    0x10
#define SDL_INIT_VIDEO    0x20
#define SDL_INIT_JOYSTICK 0x200

#define SDL_SWSURFACE  0x00000000
#define SDL_HWSURFACE  0x00000001
#define SDL_SRCCOLORKEY 0x00001000
#define SDL_HWPALETTE  0x20000000
#define SDL_DOUBLEBUF  0x40000000
#define SDL_FULLSCREEN 0x80000000
#define SDL_RESIZABLE  0x00000010
#define SDL_LOGPAL 0x01
#define SDL_PHYSPAL 0x02
#define SDL_ENABLE 1
#define SDL_DISABLE 0
#define SDL_MUSTLOCK(s) 0
#define SDL_LockSurface(s) 0
#define SDL_UnlockSurface(s) ((void)0)

typedef struct SDL_Color {
	Uint8 r, g, b, unused;
} SDL_Color;

typedef struct SDL_Rect {
	int x, y; /* Sint16 / Uint16 in SDL 1.2: int avoids narrowing errors */
	int w, h;
} SDL_Rect;

typedef struct SDL_Palette {
	int ncolors;
	SDL_Color *colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
	SDL_Palette *palette;
	Uint8 BitsPerPixel, BytesPerPixel;
	Uint32 colorkey;
} SDL_PixelFormat;

typedef struct SDL_Surface {
	Uint32 flags;
	SDL_PixelFormat *format;
	int w, h;
	Uint16 pitch;
	void *pixels;
	SDL_Rect clip_rect;
	int refcount;
	/* mini SDL */
	SDL_PixelFormat fmt;
	Uint8 *remap;       /* index remap (logical palette), NULL: identity */
	Uint8 own_pixels;
	Uint8 baked;        /* pixels read only in the pack: copied on write */
} SDL_Surface;

/* ---- surfaces ---- */
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
	Uint32 r, Uint32 g, Uint32 b, Uint32 a);
/* pixels stay owned by the caller (may be read-only: flash) */
SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int w, int h, int depth, int pitch,
	Uint32 r, Uint32 g, Uint32 b, Uint32 a);
void SDL_FreeSurface(SDL_Surface *s);
int  SDL_SetColorKey(SDL_Surface *s, Uint32 flag, Uint32 key);
int  SDL_SetPalette(SDL_Surface *s, int flags, const SDL_Color *colors, int first, int n);
int  SDL_SetColors(SDL_Surface *s, const SDL_Color *colors, int first, int n);
int  SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
#define SDL_BlitSurface SDL_UpperBlit
int  SDL_FillRect(SDL_Surface *dst, SDL_Rect *rect, Uint32 color);
int  SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *rect);
Uint32 SDL_MapRGB(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
int  SDL_SaveBMP(SDL_Surface *s, const char *file);

/* ---- video ---- */
SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, Uint32 flags);
int  SDL_Flip(SDL_Surface *screen);
SDL_Rect **SDL_ListModes(const SDL_PixelFormat *fmt, Uint32 flags);
void SDL_WM_SetCaption(const char *title, const char *icon);
int  SDL_ShowCursor(int toggle);

/* ---- misc ---- */
int  SDL_Init(Uint32 flags);
void SDL_Quit(void);
const char *SDL_GetError(void);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);

typedef struct SDL_version { Uint8 major, minor, patch; } SDL_version;
#define SDL_VERSION(x) do { (x)->major = 1; (x)->minor = 2; (x)->patch = 15; } while (0)
const SDL_version *SDL_Linked_Version(void);

/* ---- events ---- */
typedef enum {
	SDLK_UNKNOWN = 0,
	SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_RETURN = 13, SDLK_ESCAPE = 27,
	SDLK_SPACE = 32,
	SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
	SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
	SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
	SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
	SDLK_DELETE = 127,
	SDLK_UP = 273, SDLK_DOWN, SDLK_RIGHT, SDLK_LEFT,
	SDLK_F1 = 282, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9,
	SDLK_RSHIFT = 303, SDLK_LSHIFT, SDLK_RCTRL, SDLK_LCTRL, SDLK_RALT, SDLK_LALT,
	SDLK_LAST
} SDLKey;

#define KMOD_NONE 0
#define KMOD_LALT 0x0100
#define KMOD_RALT 0x0200
#define KMOD_ALT (KMOD_LALT | KMOD_RALT)
typedef int SDLMod;

typedef struct SDL_keysym {
	Uint8 scancode;
	SDLKey sym;
	SDLMod mod;
	Uint16 unicode;
} SDL_keysym;

enum {
	SDL_NOEVENT = 0, SDL_ACTIVEEVENT, SDL_KEYDOWN, SDL_KEYUP, SDL_MOUSEMOTION,
	SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_JOYAXISMOTION, SDL_JOYBALLMOTION,
	SDL_JOYHATMOTION, SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP, SDL_QUIT,
	SDL_SYSWMEVENT, SDL_EVENT_RESERVEDA, SDL_EVENT_RESERVEDB, SDL_VIDEORESIZE,
	SDL_VIDEOEXPOSE
};

#define SDL_HAT_UP 0x01
#define SDL_HAT_RIGHT 0x02
#define SDL_HAT_DOWN 0x04
#define SDL_HAT_LEFT 0x08
#define SDL_BUTTON(x) (1 << ((x) - 1))
#define SDL_BUTTON_LEFT 1

typedef struct { Uint8 type, state; SDL_keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint8 type, which, button, state; } SDL_JoyButtonEvent;
typedef struct { Uint8 type, which, axis; Sint16 value; } SDL_JoyAxisEvent;
typedef struct { Uint8 type, which, hat, value; } SDL_JoyHatEvent;
typedef struct { Uint8 type, which, state; Uint16 x, y; Sint16 xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { Uint8 type, which, button, state; Uint16 x, y; } SDL_MouseButtonEvent;
typedef struct { Uint8 type; int w, h; } SDL_ResizeEvent;

typedef union SDL_Event {
	Uint8 type;
	SDL_KeyboardEvent key;
	SDL_JoyButtonEvent jbutton;
	SDL_JoyAxisEvent jaxis;
	SDL_JoyHatEvent jhat;
	SDL_MouseMotionEvent motion;
	SDL_MouseButtonEvent button;
	SDL_ResizeEvent resize;
} SDL_Event;

int  SDL_PollEvent(SDL_Event *event);
char *SDL_GetKeyName(SDLKey key);

/* ---- joystick: none ---- */
typedef struct _SDL_Joystick SDL_Joystick;
#define SDL_NumJoysticks() 0
#define SDL_JoystickOpen(i) ((SDL_Joystick *)0)
#define SDL_JoystickClose(j) ((void)0)

/* ---- audio ---- */
#define AUDIO_U8     0x0008
#define AUDIO_S8     0x8008
#define AUDIO_U16LSB 0x0010
#define AUDIO_S16LSB 0x8010
#define AUDIO_U16MSB 0x1010
#define AUDIO_S16MSB 0x9010
#define AUDIO_S16SYS AUDIO_S16LSB
#define SDL_MIX_MAXVOLUME 128

typedef struct SDL_AudioSpec {
	int freq;
	Uint16 format;
	Uint8 channels;
	Uint8 silence;
	Uint16 samples;
	Uint16 padding;
	Uint32 size;
	void (*callback)(void *userdata, Uint8 *stream, int len);
	void *userdata;
} SDL_AudioSpec;

int  SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_PauseAudio(int pause_on);
void SDL_CloseAudio(void);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume);

#ifdef __cplusplus
}
#endif

#endif

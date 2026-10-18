/*
 * Minimal SDL 1.2 (src/port/sdl/SDL.h) on top of jj_platform.h.
 */
#include <SDL.h>
#include <string.h>
#include <stdio.h>
#include "jj_platform.h"
#include "jj_sdl.h"
#include "jj_bake.h"

/* ------------------------------------------------------------------ */
/* Surfaces                                                           */
/* ------------------------------------------------------------------ */
static SDL_Surface *new_surface(int w, int h)
{
	SDL_Surface *s = (SDL_Surface *)jj_calloc(1, sizeof(SDL_Surface));
	if (!s) jj_plat_fatal("out of memory (surface)");
	s->w = w;
	s->h = h;
	s->pitch = (Uint16)w;
	s->format = &s->fmt;
	s->fmt.BitsPerPixel = 8;
	s->fmt.BytesPerPixel = 1;
	s->clip_rect.x = 0;
	s->clip_rect.y = 0;
	s->clip_rect.w = w;
	s->clip_rect.h = h;
	s->refcount = 1;
	return s;
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth,
	Uint32, Uint32, Uint32, Uint32)
{
	(void)flags;
	(void)depth;
	SDL_Surface *s = new_surface(w, h);
	if (w > 0 && h > 0) {
		s->pixels = jj_calloc(1, (size_t)w * h);
#ifdef JJ_NEW_TRACE
		if ((size_t)w * h >= 2048)
			fprintf(stderr, "SURF %dx%d %p used %zu\n", w, h, __builtin_return_address(0), jj_heap_used());
#endif
		if (!s->pixels) {
			char msg[64];
			snprintf(msg, sizeof(msg), "out of memory (surface %dx%d)", w, h);
			jj_plat_fatal(msg);
		}
	}
	s->own_pixels = 1;
	return s;
}

SDL_Surface *SDL_CreateRGBSurfaceFrom(void *pixels, int w, int h, int depth, int pitch,
	Uint32, Uint32, Uint32, Uint32)
{
	(void)depth;
	SDL_Surface *s = new_surface(w, h);
	s->pixels = pixels;
	s->pitch = (Uint16)pitch;
	s->own_pixels = 0;
	return s;
}

void SDL_FreeSurface(SDL_Surface *s)
{
	if (!s || s == jj_sdl_screen())
		return;
	if (s->own_pixels)
		jj_free(s->pixels);
	jj_free(s->remap);
	jj_free(s);
}

int SDL_SetColorKey(SDL_Surface *s, Uint32 flag, Uint32 key)
{
	if (flag & SDL_SRCCOLORKEY) {
		s->flags |= SDL_SRCCOLORKEY;
		s->fmt.colorkey = key;
	} else {
		s->flags &= ~SDL_SRCCOLORKEY;
	}
	return 0;
}

/* The logical palette of a surface is a grey ramp unless it was changed:
 * the remap table gives the index each colour stands for. */
static void set_logical_palette(SDL_Surface *s, const SDL_Color *colors, int first, int n)
{
	if (first < 0) first = 0;
	if (first + n > 256) n = 256 - first;
	if (n <= 0)
		return;
	bool identity = true;
	for (int i = 0; i < n; i++) {
		if (colors[i].r != first + i) {
			identity = false;
			break;
		}
	}
	if (identity && !s->remap)
		return;
	if (!s->remap) {
		s->remap = (Uint8 *)jj_malloc(256);
		if (!s->remap) jj_plat_fatal("out of memory (palette)");
		for (int i = 0; i < 256; i++)
			s->remap[i] = (Uint8)i;
	}
	for (int i = 0; i < n; i++)
		s->remap[first + i] = colors[i].r;
	/* back to the grey ramp: no table */
	for (int i = 0; i < 256; i++) {
		if (s->remap[i] != i)
			return;
	}
	jj_free(s->remap);
	s->remap = nullptr;
}

int SDL_SetPalette(SDL_Surface *s, int flags, const SDL_Color *colors, int first, int n)
{
	if (s == jj_sdl_screen()) {
		if (flags & SDL_PHYSPAL)
			jj_sdl_set_physical_palette(colors, first, n);
		return 1; /* the screen's logical palette stays the grey ramp */
	}
	if (flags & SDL_LOGPAL)
		set_logical_palette(s, colors, first, n);
	return 1;
}

int SDL_SetColors(SDL_Surface *s, const SDL_Color *colors, int first, int n)
{
	return SDL_SetPalette(s, SDL_LOGPAL | SDL_PHYSPAL, colors, first, n);
}

/* a baked surface (read only, in the pack) gets a RAM copy before it is
 * drawn into (the panel of the levels: ammo, ...) */
static void make_writable(SDL_Surface *s)
{
	if (!s->baked)
		return;
	Uint8 *p = (Uint8 *)jj_malloc((size_t)s->w * s->h);
	if (!p) jj_plat_fatal("out of memory (surface copy)");
	for (int y = 0; y < s->h; y++)
		memcpy(p + y * s->w, (const Uint8 *)s->pixels + y * s->pitch, s->w);
	s->pixels = p;
	s->pitch = (Uint16)s->w;
	s->own_pixels = 1;
	s->baked = 0;
}

static bool clip(SDL_Rect *r, const SDL_Rect *c)
{
	int x0 = r->x > c->x ? r->x : c->x;
	int y0 = r->y > c->y ? r->y : c->y;
	int x1 = (r->x + r->w) < (c->x + c->w) ? (r->x + r->w) : (c->x + c->w);
	int y1 = (r->y + r->h) < (c->y + c->h) ? (r->y + r->h) : (c->y + c->h);
	if (x1 <= x0 || y1 <= y0) {
		r->w = r->h = 0;
		return false;
	}
	r->x = x0;
	r->y = y0;
	r->w = x1 - x0;
	r->h = y1 - y0;
	return true;
}

/* SDL 1.2 semantics: srcrect gives the size, dstrect only the position
 * (and gets the rectangle really drawn). */
int SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
	int sx, sy, w, h;
	if (srcrect) {
		sx = srcrect->x;
		sy = srcrect->y;
		w = srcrect->w;
		h = srcrect->h;
	} else {
		sx = sy = 0;
		w = src->w;
		h = src->h;
	}
	int dx = dstrect ? dstrect->x : 0;
	int dy = dstrect ? dstrect->y : 0;

	/* clip to the source */
	if (sx < 0) { dx -= sx; w += sx; sx = 0; }
	if (sy < 0) { dy -= sy; h += sy; sy = 0; }
	if (sx + w > src->w) w = src->w - sx;
	if (sy + h > src->h) h = src->h - sy;
	/* clip to the destination */
	const SDL_Rect &c = dst->clip_rect;
	if (dx < c.x) { int d = c.x - dx; sx += d; w -= d; dx = c.x; }
	if (dy < c.y) { int d = c.y - dy; sy += d; h -= d; dy = c.y; }
	if (dx + w > c.x + c.w) w = c.x + c.w - dx;
	if (dy + h > c.y + c.h) h = c.y + c.h - dy;
	if (dstrect) {
		dstrect->x = dx;
		dstrect->y = dy;
		dstrect->w = w > 0 ? w : 0;
		dstrect->h = h > 0 ? h : 0;
	}
	if (w <= 0 || h <= 0)
		return 0;
	make_writable(dst);

	const Uint8 *s = (const Uint8 *)src->pixels + sy * src->pitch + sx;
	Uint8 *d = (Uint8 *)dst->pixels + dy * dst->pitch + dx;
	const Uint8 *remap = src->remap;
	if (src->flags & SDL_SRCCOLORKEY) {
		const Uint8 key = (Uint8)src->fmt.colorkey;
		for (int y = 0; y < h; y++, s += src->pitch, d += dst->pitch) {
			if (remap) {
				for (int x = 0; x < w; x++)
					if (s[x] != key) d[x] = remap[s[x]];
			} else {
				for (int x = 0; x < w; x++)
					if (s[x] != key) d[x] = s[x];
			}
		}
	} else {
		for (int y = 0; y < h; y++, s += src->pitch, d += dst->pitch) {
			if (remap) {
				for (int x = 0; x < w; x++)
					d[x] = remap[s[x]];
			} else {
				memcpy(d, s, w);
			}
		}
	}
	return 0;
}

int SDL_FillRect(SDL_Surface *dst, SDL_Rect *rect, Uint32 color)
{
	SDL_Rect r;
	if (rect) {
		r = *rect;
	} else {
		r.x = r.y = 0;
		r.w = dst->w;
		r.h = dst->h;
	}
	if (!clip(&r, &dst->clip_rect))
		return 0;
	make_writable(dst);
	Uint8 *d = (Uint8 *)dst->pixels + r.y * dst->pitch + r.x;
	for (int y = 0; y < r.h; y++, d += dst->pitch)
		memset(d, (int)(color & 0xFF), r.w);
	return 0;
}

int SDL_SetClipRect(SDL_Surface *s, const SDL_Rect *rect)
{
	SDL_Rect full = { 0, 0, s->w, s->h };
	if (!rect) {
		s->clip_rect = full;
		return 1;
	}
	SDL_Rect r = *rect;
	if (!clip(&r, &full)) {
		s->clip_rect = r;
		return 0;
	}
	s->clip_rect = r;
	return 1;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *, Uint8 r, Uint8, Uint8)
{
	return r; /* grey ramp logical palette */
}

int SDL_SaveBMP(SDL_Surface *, const char *)
{
	return -1;
}

/* ------------------------------------------------------------------ */
/* Video                                                              */
/* ------------------------------------------------------------------ */
static SDL_Surface *g_screen;
static Uint8 g_phys[256 * 4];
static uint32_t g_phys_version = 1;

SDL_Surface *jj_sdl_screen(void)
{
	return g_screen;
}

void jj_sdl_set_physical_palette(const SDL_Color *colors, int first, int n)
{
	if (first < 0) first = 0;
	if (first + n > 256) n = 256 - first;
	for (int i = 0; i < n; i++) {
		g_phys[(first + i) * 4 + 0] = colors[i].r;
		g_phys[(first + i) * 4 + 1] = colors[i].g;
		g_phys[(first + i) * 4 + 2] = colors[i].b;
		g_phys[(first + i) * 4 + 3] = 0;
	}
	g_phys_version++;
}

SDL_Surface *SDL_SetVideoMode(int w, int h, int, Uint32)
{
	if (!g_screen) {
		void *mem = jj_plat_canvas_memory((size_t)w * h);
		if (mem) {
			g_screen = SDL_CreateRGBSurfaceFrom(mem, w, h, 8, w, 0, 0, 0, 0);
			memset(mem, 0, (size_t)w * h);
		} else {
			g_screen = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
		}
	}
	return g_screen;
}

int SDL_Flip(SDL_Surface *screen)
{
	jj_plat_present((const uint8_t *)screen->pixels, screen->w, screen->h, screen->pitch,
	                g_phys, g_phys_version);
	return 0;
}

static SDL_Rect g_mode = { 0, 0, JJ_WIDTH, JJ_HEIGHT };
static SDL_Rect *g_modes[2] = { &g_mode, nullptr };

SDL_Rect **SDL_ListModes(const SDL_PixelFormat *, Uint32)
{
	return g_modes;
}

void SDL_WM_SetCaption(const char *, const char *) {}
int SDL_ShowCursor(int) { return 0; }

/* ------------------------------------------------------------------ */
/* Misc                                                               */
/* ------------------------------------------------------------------ */
int SDL_Init(Uint32) { return 0; }
void SDL_Quit(void) {}
const char *SDL_GetError(void) { return "mini SDL"; }

Uint32 SDL_GetTicks(void)
{
	return jj_plat_time_ms();
}

void SDL_Delay(Uint32 ms)
{
	uint32_t start = jj_plat_time_ms();
	jj_plat_service(0);
	while (jj_plat_time_ms() - start < ms)
		jj_plat_service(1);
}

const SDL_version *SDL_Linked_Version(void)
{
	static const SDL_version v = { 1, 2, 15 };
	return &v;
}

/* ------------------------------------------------------------------ */
/* Events: the buttons of the console as keys                         */
/* ------------------------------------------------------------------ */
static uint32_t g_buttons;      /* held now (jj_core_set_buttons) */
static uint32_t g_buttons_sent; /* as told to the game */
static SDLKey g_keymap[8][2];   /* per button bit: up to 2 keys */
static bool g_quit;

void jj_sdl_map_button(int bit, SDLKey k1, SDLKey k2)
{
	g_keymap[bit][0] = k1;
	g_keymap[bit][1] = k2;
}

void jj_sdl_set_buttons(uint32_t buttons)
{
	g_buttons = buttons;
}

void jj_sdl_request_quit(void)
{
	g_quit = true;
}

int SDL_PollEvent(SDL_Event *event)
{
	static int pending_key = -1; /* second key of a button */
	static bool pending_down;
	static int pending_bit;

	if (pending_key < 0)
		jj_plat_service(0);

	if (pending_key >= 0) {
		memset(event, 0, sizeof(*event));
		event->type = pending_down ? SDL_KEYDOWN : SDL_KEYUP;
		event->key.keysym.sym = (SDLKey)pending_key;
		pending_key = -1;
		(void)pending_bit;
		return 1;
	}
	if (g_quit) {
		g_quit = false;
		memset(event, 0, sizeof(*event));
		event->type = SDL_QUIT;
		return 1;
	}
	uint32_t changed = g_buttons ^ g_buttons_sent;
	if (!changed)
		return 0;
	int bit = 0;
	while (!(changed & (1u << bit)))
		bit++;
	bool down = (g_buttons >> bit) & 1;
	g_buttons_sent ^= 1u << bit;
	memset(event, 0, sizeof(*event));
	event->type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event->key.keysym.sym = g_keymap[bit][0];
	if (g_keymap[bit][1] != SDLK_UNKNOWN) {
		pending_key = g_keymap[bit][1];
		pending_down = down;
		pending_bit = bit;
	}
	return 1;
}

static const char *jj_key_name_impl(SDLKey key);

char *SDL_GetKeyName(SDLKey key)
{
	return (char *)jj_key_name_impl(key);
}

static const char *jj_key_name_impl(SDLKey key)
{
	switch (key) {
	case SDLK_UP: return "up";
	case SDLK_DOWN: return "down";
	case SDLK_LEFT: return "left";
	case SDLK_RIGHT: return "right";
	case SDLK_LCTRL: return "A";
	case SDLK_LALT: return "B";
	case SDLK_ESCAPE: return "GAME";
	case SDLK_LSHIFT: return "TIME";
	case SDLK_RETURN: return "return";
	case SDLK_SPACE: return "space";
	default: return "-";
	}
}

/* ------------------------------------------------------------------ */
/* Audio: the game's callback (stereo, 16 bit) mixed down to mono     */
/* ------------------------------------------------------------------ */
static SDL_AudioSpec g_audio;
static bool g_audio_open, g_audio_paused = true;

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
	g_audio = *desired;
	g_audio.freq = jj_plat_audio_rate();
	g_audio.format = AUDIO_S16SYS;
	g_audio.channels = 2;
	if (obtained)
		*obtained = g_audio;
	g_audio_open = true;
	return 0;
}

void SDL_PauseAudio(int pause_on) { g_audio_paused = pause_on != 0; }
void SDL_CloseAudio(void) { g_audio_open = false; }
void SDL_LockAudio(void) {}
void SDL_UnlockAudio(void) {}

void SDL_MixAudio(Uint8 *dst, const Uint8 *src, Uint32 len, int volume)
{
	int16_t *d = (int16_t *)dst;
	const int16_t *s = (const int16_t *)src;
	for (Uint32 i = 0; i < len / 2; i++) {
		int v = d[i] + (s[i] * volume) / SDL_MIX_MAXVOLUME;
		d[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
	}
}

void jj_sdl_audio_render(int16_t *out, int frames)
{
	if (!g_audio_open || g_audio_paused || !g_audio.callback) {
		memset(out, 0, frames * sizeof(int16_t));
		return;
	}
	static int16_t buf[256 * 2];
	while (frames > 0) {
		int n = frames > 256 ? 256 : frames;
		g_audio.callback(g_audio.userdata, (Uint8 *)buf, n * 4);
		for (int i = 0; i < n; i++)
			out[i] = (int16_t)((buf[i * 2] + buf[i * 2 + 1]) >> 1);
		out += n;
		frames -= n;
	}
}

/* ------------------------------------------------------------------ */
/* Pre-decoded pictures (jj_bake.c)                                   */
/* ------------------------------------------------------------------ */
void jj_sdl_bake_surface(SDL_Surface *s)
{
	if (!s || !s->pixels || !s->own_pixels)
		return;
#ifndef JJ_DEVICE
	if (jj_bake_recording()) {
		jj_bake_record((const uint8_t *)s->pixels, s->w, s->h, s->pitch);
		return;
	}
#endif
	const uint8_t *baked = jj_bake_find((const uint8_t *)s->pixels, s->w, s->h, s->pitch);
	if (!baked)
		return;
	jj_free(s->pixels);
	s->pixels = (void *)baked;
	s->pitch = (Uint16)s->w;
	s->own_pixels = 0;
	s->baked = 1;
}

SDL_Surface *jj_sdl_create_baked(const uint8_t *pixels, int w, int h)
{
#ifndef JJ_DEVICE
	if (jj_bake_recording()) {
		jj_bake_record(pixels, w, h, w);
		return nullptr;
	}
#endif
	const uint8_t *baked = jj_bake_find(pixels, w, h, w);
	if (!baked) {
		if (w * h >= 4096)
			printf("jj: picture %dx%d not pre-decoded: in RAM\n", w, h);
		return nullptr;
	}
	return jj_sdl_surface_from_baked(baked, w, h);
}

SDL_Surface *jj_sdl_surface_from_baked(const uint8_t *pixels, int w, int h)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceFrom((void *)pixels, w, h, 8, w, 0, 0, 0, 0);
	s->baked = 1;
	return s;
}

bool jj_sdl_bake_recording(void)
{
#ifndef JJ_DEVICE
	return jj_bake_recording();
#else
	return false;
#endif
}

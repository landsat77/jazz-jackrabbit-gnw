/*
 * Jazz Jackrabbit for the Game & Watch (Retro-Go SD), as a GWHB homebrew.
 *
 * The game is OpenJazz (src/oj) on a minimal SDL 1.2 (src/port/sdl,
 * jj_sdl.cpp), run single threaded by jj_core.cpp. This file is the
 * retro-go side: it maps the asset pack (the .jj file next to this .bin, see
 * tools/jj_pack.py) from the SD card into the external flash, gives the free
 * RAM to the game's heap, and implements the platform hooks (display,
 * audio, buttons, menu).
 *
 * Everything that reaches the firmware goes through the ABI bridge
 * (sdk/src/gw_core_bridge.*): this binary is not linked with the firmware.
 */
#include <odroid_system.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "gw_lcd.h"
#include "gw_buttons.h"
#include "gw_audio.h"
#include "gw_malloc.h"
#include "common.h"
#include "rom_manager.h"
#include "appid.h"
#include "rg_storage.h"
#include "odroid_overlay.h"
#include "odroid_settings.h"

#include "jj_platform.h"
#include "jj_pack.h"

#include "gw_core_i18n.h"
#include "gw_core_bridge.h" /* last: rewrites ACTIVE_FILE, common_emu_state, dma_counter */

#define JJ_SAMPLE_RATE         22050
/* Audio: the firmware's interrupt fills each DMA half buffer (16.7 ms)
 * from a ring the game fills ahead (JJ_RING_AHEAD samples, ~93 ms): the
 * sound does not depend on when the game gets to it. */
#define JJ_AUDIO_BUFFER_LENGTH 368
#define JJ_RING_SIZE  4096   /* power of 2 */
#define JJ_RING_AHEAD 2048
#define JJ_TICK_MS             16       /* buttons / menu / overlay rate */

static odroid_gamepad_state_t joystick;

/* The game draws into its canvas (320 x 240 palette indexes, in DTCM);
 * jj_plat_present() converts it to the LCD. */
static const uint8_t *canvas;
static int canvas_pitch;
static const uint8_t *canvas_palette;
static uint32_t canvas_pal_version, pal_version_done = 0xFFFFFFFF;
static uint16_t pal565[256];
static bool have_frame;
static uint32_t last_present_ms;

static uint32_t last_dma_counter;
static uint32_t last_tick_ms;
static uint32_t time_offset;  /* time spent in the menus: not game time */
static int in_service;

/* statistics (log every 5 s): frames, idle time (DWT cycle counter) */
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static uint32_t stat_frames, stat_t0, stat_audio_missed, stat_max_frame_ms;
static void feed_audio(void);
static FILE *stat_log;
static int stat_log_lines;
static void ring_stop(void);
static uint64_t stat_idle_cycles;

static char save_dir[160];
static char options_path[200];

/* Options (saved per pack) */
typedef struct {
    uint32_t magic;
    uint8_t music;
    uint8_t sound;
    uint8_t swap_ab;
    uint8_t reserved;
} jj_options_t;
#define JJ_OPTIONS_MAGIC 0x4A4A4F31 /* "JJO1" */
static jj_options_t options = { JJ_OPTIONS_MAGIC, 1, 1, 0, 0 };

static void options_load(void)
{
    FILE *f = fopen(options_path, "rb");
    if (!f)
        return;
    jj_options_t o;
    if (fread(&o, sizeof(o), 1, f) == 1 && o.magic == JJ_OPTIONS_MAGIC)
        options = o;
    fclose(f);
}

static void options_save(void)
{
    FILE *f = fopen(options_path, "wb");
    if (!f)
        return;
    fwrite(&options, sizeof(options), 1, f);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Platform: time, files, memory                                      */
/* ------------------------------------------------------------------ */
uint32_t jj_plat_time_ms(void)
{
    return get_elapsed_time() - time_offset;
}

int jj_plat_audio_rate(void)
{
    return JJ_SAMPLE_RATE;
}

bool jj_plat_find_file(const char *name, const uint8_t **data, uint32_t *size)
{
    return jj_pack_find(name, data, size);
}

const char *jj_plat_save_dir(void)
{
    return save_dir;
}

void *jj_plat_canvas_memory(size_t size)
{
    /* the canvas is drawn and read all the time: DTCM */
    void *p = dtc_malloc(size);
    printf("jj: canvas %u bytes at %p\n", (unsigned)size, p);
    return p;
}

static void show_error(const char *msg, const char *detail);

void jj_plat_fatal(const char *msg)
{
    ring_stop();
    audio_stop_playing();
    show_error("Jazz Jackrabbit error:", msg);
    while (1)
        ;
}

void jj_plat_quit(void)
{
    printf("jj: game quit\n");
    ring_stop();
    audio_stop_playing();
    odroid_system_switch_app(0);
    while (1)
        ;
}

/* ------------------------------------------------------------------ */
/* Display                                                            */
/* ------------------------------------------------------------------ */
static void convert_canvas(uint16_t *fb)
{
    if (canvas_pal_version != pal_version_done) {
        for (int i = 0; i < 256; i++) {
            const uint8_t *c = canvas_palette + i * 4;
            pal565[i] = (uint16_t)(((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[2] >> 3));
        }
        pal_version_done = canvas_pal_version;
    }
    for (int y = 0; y < GW_LCD_HEIGHT; y++) {
        const uint8_t *s = canvas + y * canvas_pitch;
        uint16_t *d = fb + y * GW_LCD_WIDTH;
        for (int x = 0; x < GW_LCD_WIDTH; x += 4) {
            d[x] = pal565[s[x]];
            d[x + 1] = pal565[s[x + 1]];
            d[x + 2] = pal565[s[x + 2]];
            d[x + 3] = pal565[s[x + 3]];
        }
    }
}

void jj_plat_present(const uint8_t *pixels, int w, int h, int pitch,
                     const uint8_t *palette, uint32_t pal_version)
{
    (void)w;
    (void)h;
    canvas = pixels;
    canvas_pitch = pitch;
    canvas_palette = palette;
    canvas_pal_version = pal_version;
    feed_audio();
    lcd_sleep_while_swap_pending();
    uint16_t *fb = lcd_get_active_buffer();
    convert_canvas(fb);
    common_ingame_overlay();
    lcd_swap();
    uint32_t now = get_elapsed_time();
    if (have_frame && now - last_present_ms > stat_max_frame_ms && now - last_present_ms < 1000)
        stat_max_frame_ms = now - last_present_ms;
    have_frame = true;
    last_present_ms = now;
    stat_frames++;
    if (last_present_ms - stat_t0 >= 5000) {
        uint32_t ms = last_present_ms - stat_t0;
        /* 280 MHz */
        uint32_t idle_ms = (uint32_t)(stat_idle_cycles / 280000u);
        char line[160];
        snprintf(line, sizeof(line), "jj: %lu.%lu fps, longest frame %lu ms, idle %lu%%, heap %u / peak %u, audio underrun %lu samples\n",
               (unsigned long)(stat_frames * 1000 / ms), (unsigned long)((stat_frames * 10000 / ms) % 10),
               (unsigned long)stat_max_frame_ms,
               (unsigned long)(idle_ms * 100 / ms), (unsigned)jj_heap_used(), (unsigned)jj_heap_peak(),
               (unsigned long)stat_audio_missed);
        printf("%s", line);
        /* the first minute in a file of the save folder (diagnostics) */
        if (stat_log && stat_log_lines < 12) {
            fputs(line, stat_log);
            if (++stat_log_lines == 12) {
                fclose(stat_log);
                stat_log = NULL;
            }
        }
        stat_audio_missed = 0;
        stat_max_frame_ms = 0;
        stat_frames = 0;
        stat_idle_cycles = 0;
        stat_t0 = last_present_ms;
    }
}

/* menu background: the game's picture */
static void repaint(void)
{
    lcd_sleep_while_swap_pending();
    if (have_frame)
        convert_canvas(lcd_get_active_buffer());
    common_ingame_overlay();
}

static void *screenshot(void)
{
    lcd_wait_for_vblank();
    if (have_frame) {
        convert_canvas(lcd_get_active_buffer());
        return lcd_get_active_buffer();
    }
    return lcd_get_inactive_buffer();
}

/* ------------------------------------------------------------------ */
/* Audio                                                              */
/* ------------------------------------------------------------------ */
static int16_t ring[JJ_RING_SIZE];
static volatile uint16_t ring_head, ring_tail; /* written / read (interrupt) */
static uint32_t ring_written;                  /* samples written in all */
static bool ring_on;

static void ring_start(void)
{
    ring_head = ring_tail = 0;
    ring_written = 0;
    pcm_attach(ring, JJ_RING_SIZE, &ring_head, &ring_tail);
    pcm_audio_setpos(0);
    pcm_audio_set(common_emu_sound_get_volume(), 1);
    pcm_audio_enable(1);
    ring_on = true;
}

static void ring_stop(void)
{
    if (ring_on)
        pcm_audio_enable(0);
    ring_on = false;
}

/* tops the ring up to JJ_RING_AHEAD samples ahead of the interrupt */
static void feed_audio(void)
{
    if (!ring_on)
        return;
    /* the interrupt played silence when the ring was empty */
    uint32_t played = pcm_audio_pos();
    if ((int32_t)(played - ring_written) > 0) {
        stat_audio_missed += played - ring_written;
        ring_written = played;
    }
    bool muted = common_emu_sound_loop_is_muted();
    pcm_audio_set(common_emu_sound_get_volume(), muted ? 0 : 1);
    uint16_t tail = ring_tail;
    int level = (ring_head - tail) & (JJ_RING_SIZE - 1);
    int want = JJ_RING_AHEAD - level;
    while (want > 0) {
        int head = ring_head;
        int n = want;
        if (n > JJ_RING_SIZE - head)
            n = JJ_RING_SIZE - head; /* up to the end of the ring */
        if (n > 512)
            n = 512;
        /* always render: keeps the music and the sounds moving */
        jj_core_audio_render(ring + head, n);
        ring_head = (uint16_t)((head + n) & (JJ_RING_SIZE - 1));
        ring_written += n;
        want -= n;
    }
}

/* ------------------------------------------------------------------ */
/* Pause menu                                                         */
/* ------------------------------------------------------------------ */
static char music_value[12], sound_value[12], swap_value[12];

static const gw_i18n_entry_t i18n_yes[] = {
    { "en", "Yes" }, { "fr", "Oui" }, { "es", "S\xc3\xad" }, { "de", "Ja" }, { "it", "S\xc3\xac" }, { "pt", "Sim" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no[] = {
    { "en", "No" }, { "fr", "Non" }, { "es", "No" }, { "de", "Nein" }, { "it", "No" }, { "pt", "N\xc3\xa3o" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_music[] = {
    { "en", "Music" }, { "fr", "Musique" }, { "es", "M\xc3\xbasica" }, { "de", "Musik" }, { "it", "Musica" }, { "pt", "M\xc3\xbasica" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_sound[] = {
    { "en", "Sound" }, { "fr", "Son" }, { "es", "Sonido" }, { "de", "Ton" }, { "it", "Suono" }, { "pt", "Som" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_swap_ab[] = {
    { "en", "Swap A/B" }, { "fr", "Inverser A/B" }, { "es", "Invertir A/B" }, { "de", "A/B tauschen" }, { "it", "Inverti A/B" }, { "pt", "Inverter A/B" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no_pack[] = {
    { "en", "Game data not found:" }, { "fr", "Donn\xc3\xa9" "es du jeu introuvables :" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_bad_pack[] = {
    { "en", "Not a valid .jj file:" }, { "fr", "Fichier .jj invalide :" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no_flash[] = {
    { "en", "Pack too big for the flash:" }, { "fr", "Pack trop gros pour la flash :" }, GW_I18N_END
};

static void bool_label(char *dst, bool v)
{
    strncpy(dst, gw_i18n(v ? i18n_yes : i18n_no), 11);
    dst[11] = '\0';
}

static bool music_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT || event == ODROID_DIALOG_ENTER) {
        options.music = !options.music;
        jj_core_set_sound(options.music, options.sound);
        options_save();
    }
    bool_label(option->value, options.music);
    return false;
}

static bool sound_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT || event == ODROID_DIALOG_ENTER) {
        options.sound = !options.sound;
        jj_core_set_sound(options.music, options.sound);
        options_save();
    }
    bool_label(option->value, options.sound);
    return false;
}

static bool swap_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT || event == ODROID_DIALOG_ENTER) {
        options.swap_ab = !options.swap_ab;
        options_save();
    }
    bool_label(option->value, options.swap_ab);
    return false;
}

static odroid_dialog_choice_t game_options[4];

static void setup_game_options(void)
{
    game_options[0] = (odroid_dialog_choice_t){300, gw_i18n(i18n_music), music_value, 1, &music_cb};
    game_options[1] = (odroid_dialog_choice_t){301, gw_i18n(i18n_sound), sound_value, 1, &sound_cb};
    game_options[2] = (odroid_dialog_choice_t){302, gw_i18n(i18n_swap_ab), swap_value, 1, &swap_cb};
    game_options[3] = (odroid_dialog_choice_t)ODROID_DIALOG_CHOICE_LAST;
}

/* ------------------------------------------------------------------ */
/* Buttons                                                            */
/* ------------------------------------------------------------------ */
/* The core turns them into the keys of the PC version (jj_core.cpp):
 *   A = jump / swim / enter, B = fire / no, GAME = menu (Esc),
 *   TIME = change weapon. */
static uint32_t map_buttons(const odroid_gamepad_state_t *j)
{
    uint32_t b = 0;
    if (j->values[ODROID_INPUT_LEFT])  b |= JJ_BTN_LEFT;
    if (j->values[ODROID_INPUT_RIGHT]) b |= JJ_BTN_RIGHT;
    if (j->values[ODROID_INPUT_UP])    b |= JJ_BTN_UP;
    if (j->values[ODROID_INPUT_DOWN])  b |= JJ_BTN_DOWN;
    if (j->values[ODROID_INPUT_A])     b |= options.swap_ab ? JJ_BTN_B : JJ_BTN_A;
    if (j->values[ODROID_INPUT_B])     b |= options.swap_ab ? JJ_BTN_A : JJ_BTN_B;
    if (j->values[ODROID_INPUT_START] || j->values[ODROID_INPUT_X])  b |= JJ_BTN_GAME;
    if (j->values[ODROID_INPUT_SELECT] || j->values[ODROID_INPUT_Y]) b |= JJ_BTN_TIME;
    return b;
}

/* ------------------------------------------------------------------ */
/* Service                                                            */
/* ------------------------------------------------------------------ */
static void run_input_loop(void)
{
    uint32_t t0 = get_elapsed_time();

    bool_label(music_value, options.music);
    bool_label(sound_value, options.sound);
    bool_label(swap_value, options.swap_ab);

    common_emu_input_loop(&joystick, game_options, &repaint);

    uint32_t spent = get_elapsed_time() - t0;
    if (spent > 100) {
        /* the menu was open: its time is not game time, and a clean picture */
        time_offset += spent;
        lcd_sleep_while_swap_pending();
        lcd_clear_buffers();
        if (have_frame) {
            convert_canvas(lcd_get_active_buffer());
            lcd_swap();
        }
    }

    last_tick_ms = get_elapsed_time();
    last_dma_counter = dma_counter;
}

void jj_plat_service(int in_delay)
{
    if (in_service)
        return;
    in_service = 1;

    wdog_refresh();
    feed_audio();

    uint32_t now = get_elapsed_time();
    if (now - last_tick_ms >= JJ_TICK_MS) {
        last_tick_ms = now;
        odroid_input_read_gamepad(&joystick);
        lcd_sleep_while_swap_pending();
        run_input_loop();
        jj_core_set_buttons(map_buttons(&joystick));
        /* volume / brightness overlays while the game does not draw */
        if (common_emu_state.overlay != 0 && have_frame && now - last_present_ms > 100 && !lcd_is_swap_pending()) {
            repaint();
            lcd_swap();
        }
    }

    in_service = 0;
    if (in_delay) {
        uint32_t c0 = DWT_CYCCNT;
        __WFI(); /* SysTick (1 kHz) or the audio DMA wake us up */
        stat_idle_cycles += (uint32_t)(DWT_CYCCNT - c0);
    }
}

/* ------------------------------------------------------------------ */
/* Savestates (jj_state.cpp): the level, its checkpoint, the player   */
/* ------------------------------------------------------------------ */
static bool jj_load_state(const char *path)
{
    /* played as soon as the game gets control (also before it starts) */
    return jj_core_load_state(path);
}

static bool jj_save_state(const char *path)
{
    return jj_core_save_state(path);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */
static void show_error(const char *msg, const char *detail)
{
    printf("jj: %s %s\n", msg, detail);
    odroid_dialog_choice_t choices[] = {
        {0, msg, "", -1, NULL},
        {0, detail, "", -1, NULL},
        ODROID_DIALOG_CHOICE_SEPARATOR,
        {1, "OK", "", 1, NULL},
        ODROID_DIALOG_CHOICE_LAST,
    };
    lcd_clear_buffers();
    odroid_overlay_dialog("Jazz Jackrabbit", choices, 3, NULL, 0);
    odroid_system_switch_app(0);
}

/* ".../Jazz.bin" -> ".../Jazz.jj" */
static void pack_path_for(const char *bin_path, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s", bin_path);
    char *slash = strrchr(out, '/');
    char *dot = strrchr(out, '.');
    if (dot && (!slash || dot > slash))
        *dot = '\0';
    size_t len = strlen(out);
    snprintf(out + len, out_size - len, ".jj");
}

void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
    gw_core_bridge_init();
    /* cycle counter for the statistics */
    *(volatile uint32_t *)0xE000EDFC |= 1u << 24; /* DEMCR.TRCENA */
    *(volatile uint32_t *)0xE0001000 |= 1u;       /* DWT_CTRL.CYCCNTENA */
    printf("Jazz Jackrabbit start\n");

    odroid_system_init(APPID_HOMEBREW, JJ_SAMPLE_RATE);
    odroid_system_emu_init(&jj_load_state, &jj_save_state, &screenshot, NULL, NULL, NULL, NULL);

    /* Same name as this .bin */
    static char pack_path[256];
    pack_path_for(ACTIVE_FILE->path, pack_path, sizeof(pack_path));
    const char *pack_name = strrchr(pack_path, '/');
    pack_name = pack_name ? pack_name + 1 : pack_path;

    /* Folder for the game's own files (configuration, saved games) and the
     * options of the port */
    {
        char name[64];
        snprintf(name, sizeof(name), "%s", pack_name);
        char *dot = strrchr(name, '.');
        if (dot)
            *dot = '\0';
        rg_storage_mkdir(ODROID_BASE_PATH_SAVES "/jazz");
        snprintf(save_dir, sizeof(save_dir), ODROID_BASE_PATH_SAVES "/jazz/%s", name);
        rg_storage_mkdir(save_dir);
        snprintf(options_path, sizeof(options_path), "%s/gnw_options.bin", save_dir);
        options_load();
    }

    /* Map the asset pack into the external flash */
    FILE *probe = fopen(pack_path, "rb");
    if (!probe) {
        show_error(gw_i18n(i18n_no_pack), pack_name);
        return;
    }
    fclose(probe);
    uint32_t pack_size = 0;
    uint8_t *pack = odroid_overlay_cache_file_in_flash(pack_path, &pack_size, false);
    if (!pack) {
        show_error(gw_i18n(i18n_no_flash), pack_name);
        return;
    }
    int r = jj_pack_open(pack, pack_size);
    if (r != JJ_PACK_OK) {
        show_error(gw_i18n(i18n_bad_pack), pack_name);
        return;
    }

    /* Everything left in RAM_EMU is the game's heap */
    size_t heap_size = ram_get_free_size();
    void *heap = ram_malloc(heap_size);
    jj_heap_init(heap, heap_size);
    printf("jj: pack %lu bytes, heap %u bytes, DTCM %u bytes\n", (unsigned long)pack_size,
           (unsigned)heap_size, (unsigned)dtc_get_free_size());

    if (load_state)
        odroid_system_emu_load_state(save_slot); /* needs the pack: after it */

    if (start_paused) {
        common_emu_state.pause_after_frames = 2;
        odroid_audio_mute(true);
    } else {
        common_emu_state.pause_after_frames = 0;
    }
    common_emu_state.frame_time_10us = (uint16_t)(100000 / 60 + 0.5f);

    lcd_clear_buffers();
    audio_start_playing(JJ_AUDIO_BUFFER_LENGTH);
    last_dma_counter = dma_counter;
    ring_start();
    {
        char path[220];
        snprintf(path, sizeof(path), "%s/log.txt", save_dir);
        stat_log = fopen(path, "w");
    }
    last_tick_ms = get_elapsed_time();

    setup_game_options();

    jj_core_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.start_world = -1;
    opts.start_level = -1;
    opts.music = options.music;
    opts.sound = options.sound;
    jj_core_run(&opts); /* never returns */
}

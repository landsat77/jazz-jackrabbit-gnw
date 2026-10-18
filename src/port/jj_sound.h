/* Port side of the sound (jj_sound.cpp) */
#ifndef JJ_SOUND_H
#define JJ_SOUND_H

#include <stdint.h>

/* mono output: music + sound effects */
void jj_sound_render(int16_t *out, int frames);
void jj_sound_enable(bool music, bool sound);

#endif

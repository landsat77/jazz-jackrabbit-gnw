/*
 * Sound of OpenJazz on the Game & Watch (replaces src/oj/io/sound.cpp).
 *
 *  - Sound effects: the 8 bit clips of SOUNDS.000 / the levels are played in
 *    place from the pack (no resampled copies in RAM), mixed at the output
 *    rate with a fixed point step.
 *  - Music: the PSM modules are far too heavy for the console (xmp loads
 *    the whole module and its samples in RAM): tools/jj_pack.py renders them
 *    on the PC (harness, option -R, xmp) to "<NAME>.MUS" pieces that the
 *    console plays back in place, in a loop (jj_music.c).
 *    The faster music of the "fast feet" bonus is not reproduced.
 */
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "io/file.h"
#include "io/sound.h"
#include "util.h"
#include "io/log.h"

#include "jj_platform.h"
#include "jj_music.h"
#include "jj_sound.h"

namespace {

	struct RawSound {
		const unsigned char *data;
		int length;
		char name[13];
	};

	struct Sound {
		const signed char *data;
		int length;
		uint32_t step;     // 16.16 source samples per output sample
		int32_t position;  // 16.16 position, < 0: not playing
	};

	RawSound *rawSounds = nullptr;
	int nRawSounds = 0;
	Sound sounds[SE::MAX] = {};
	bool soundsLoaded = false;
	int musicVolume = MAX_VOLUME >> 1; // 50%
	int soundVolume = MAX_VOLUME >> 2; // 25%
	char currentMusic[16];
	MusicTempo musicTempo = MusicTempo::NORMAL;
	bool musicPaused = false;
	jj_mus_t *mus = nullptr;
	bool musicEnabled = true;
	bool soundEnabled = true;

}

void jj_sound_enable(bool music, bool sound)
{
	musicEnabled = music;
	soundEnabled = sound;
	if (!sound) {
		for (int i = 0; i < SE::MAX; i++)
			sounds[i].position = -1;
	}
}

void jj_sound_render(int16_t *out, int frames)
{
	memset(out, 0, frames * sizeof(int16_t));
	const int rate = jj_plat_audio_rate();

	if (mus && !musicPaused) {
		int done = 0;
		bool audible = musicEnabled && musicVolume > 0;
		while (done < frames) {
			int n = jj_mus_mix(mus, out + done, frames - done, 1, audible, rate);
			done += n;
			if (done < frames && !jj_mus_rewind(mus))
				break;
		}
		if (audible) {
			// music rendered at xmp volume 100; the game's default volume
			// (50) is half of it: twice as loud on the small speaker
			const int gain = musicVolume * 2 * 256 / MAX_VOLUME;
			for (int i = 0; i < frames; i++) {
				int v = (out[i] * gain) >> 8;
				out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
			}
		}
	}

	if (!soundsLoaded || !soundEnabled)
		return;

	// 8 bit clips: x 256, then the volume (SDL: volume * 128 / 100 of 128),
	// twice as loud like the music
	const int vol = soundVolume * 2 * 256 / MAX_VOLUME;
	for (int i = SE::NONE + 1; i < SE::MAX; i++) {
		Sound &s = sounds[i];
		if (!s.data || s.position < 0)
			continue;
		int32_t pos = s.position;
		const int32_t end = s.length << 16;
		for (int k = 0; k < frames; k++) {
			if (pos >= end) {
				pos = -1;
				break;
			}
			int v = out[k] + ((s.data[pos >> 16] * vol));
			out[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
			pos += s.step;
		}
		s.position = pos;
	}
	(void)rate;
}

void openAudio () {
	soundsLoaded = loadSounds("SOUNDS.000");
}


void closeAudio () {
	stopMusic();
	freeSounds();
	delete[] rawSounds;
	rawSounds = nullptr;
	nRawSounds = 0;
}


void playMusic (const char * fileName, bool restart) {
	char name[16];
	int i = 0;
	for (; fileName[i] && fileName[i] != '.' && i < 11; i++)
		name[i] = toupper(static_cast<unsigned char>(fileName[i]));
	name[i] = '\0';

	if (mus && strcmp(name, currentMusic) == 0 && !restart)
		return;

	stopMusic();

	char musName[20];
	snprintf(musName, sizeof(musName), "%s.MUS", name);
	const uint8_t *data;
	uint32_t size;
	if (!jj_plat_find_file(musName, &data, &size)) {
		LOG_WARN("No pre-rendered music %s", musName);
		return;
	}
	mus = jj_mus_open(data, static_cast<int>(size));
	if (mus)
		strcpy(currentMusic, name);
	musicPaused = false;
}


void pauseMusic (bool pause) {
	musicPaused = pause;
}


void stopMusic () {
	jj_mus_t *m = mus;
	mus = nullptr;
	if (m) jj_mus_close(m);
	currentMusic[0] = '\0';
}


int getMusicVolume () {
	return musicVolume;
}


void setMusicVolume (int volume) {
	musicVolume = CLAMP(volume, 0, MAX_VOLUME);
}


MusicTempo getMusicTempo () {
	return musicTempo;
}


void setMusicTempo (MusicTempo tempo) {
	musicTempo = tempo;
}


bool loadSounds (const char *fileName) {
	FilePtr file;

	try {
		file = std::make_unique<File>(fileName, PATH_TYPE_GAME);
	} catch (int e) {
		return false;
	}

	// Checking sound file header
	char *identifier1 = file->loadString(3);
	char identifier2 = file->loadChar();
	if (strncmp(identifier1, "sfx", 2) != 0 || identifier2 != 0x1A) {
		LOG_ERROR("Sound data not valid!");
		delete[] identifier1;
		return false;
	}
	delete[] identifier1;

	// Locate the header data
	file->seek(file->getSize() - 4, true);
	int headerOffset = file->loadInt();

	nRawSounds = (file->getSize() - headerOffset) / 18;
	if (nRawSounds > SE::MAX) nRawSounds = SE::MAX;

	delete[] rawSounds;
	rawSounds = new RawSound[nRawSounds];

	for (int i = 0; i < nRawSounds; i++) {
		file->seek(headerOffset + (i * 18), true);
		char *name = file->loadString(12);
		strncpy(rawSounds[i].name, name, 12);
		rawSounds[i].name[12] = '\0';
		delete[] name;
		int offset = file->loadInt();
		rawSounds[i].length = file->loadShort();
		file->seek(offset, true);
		// in place, in the pack
		rawSounds[i].data = file->mapped();
		if (!rawSounds[i].data) rawSounds[i].length = 0;
	}

	resampleSounds();

	return true;
}


void resampleSound (int index, const char* name, int rate) {
	int se = index + 1;

	if (!isValidSoundIndex(static_cast<SE::Type>(se)) || se == SE::NONE) {
		LOG_ERROR("Cannot resample Sound Index %d", se);
		return;
	}

	sounds[se].data = nullptr;
	sounds[se].position = -1;

	if (!strlen(name) || rate <= 0)
		return;

	for (int i = 0; i < nRawSounds; i++) {
		if (strcmp(name, rawSounds[i].name) != 0)
			continue;
		sounds[se].data = reinterpret_cast<const signed char *>(rawSounds[i].data);
		sounds[se].length = rawSounds[i].length;
		sounds[se].step = static_cast<uint32_t>((static_cast<uint64_t>(rate) << 16) / jj_plat_audio_rate());
		return;
	}
	LOG_WARN("Cannot find sound %s!", name);
}


void resampleSounds() {
	for (int i = 0; i < nRawSounds && i + 1 < SE::MAX; i++) {
		resampleSound(i, rawSounds[i].name, 11025);
	}
}


void freeSounds() {
	for (int i = SE::NONE; i < SE::MAX; i++) {
		sounds[i].data = nullptr;
		sounds[i].position = -1;
	}
}


void playSound(SE::Type index) {
	if (!soundsLoaded || index == SE::NONE || !soundEnabled) return;
	if (!isValidSoundIndex(index)) return;
	if (!sounds[index].data) return;
	sounds[index].position = 0;
}


bool isSoundPlaying (SE::Type index) {
	if (!soundsLoaded || !isValidSoundIndex(index)) return false;
	return (sounds[index].position > 0);
}


int getSoundVolume () {
	return soundVolume;
}


void setSoundVolume (int volume) {
	soundVolume = CLAMP(volume, 0, MAX_VOLUME);
}

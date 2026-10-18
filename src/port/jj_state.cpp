/*
 * Savestates of Jazz Jackrabbit on the Game & Watch.
 *
 * OpenJazz has no way to save a level being played (its own saved games
 * only record the level), so a savestate records what the game needs to
 * restart where the player could restart after losing a life: the level,
 * its last checkpoint, the difficulty and the player's score, lives, ammo,
 * fire rate and birds. Loading one plays that level from that checkpoint
 * (the enemies and items of the level come back, like after losing a life).
 *
 * File (little endian): "GWJJ", u32 version, char level[32], i32 difficulty,
 * i32 checkX, i32 checkY, i32 ammo[5], ammoType, score, lives, fireSpeed,
 * flockSize.
 */
#include <stdio.h>
#include <string.h>

#include "game/game.h"
#include "player/player.h"
#include "jj1/level/jj1level.h"
#include "io/log.h"

#include "jj_platform.h"
#include "jj_state.h"

#define STATE_VERSION 1

struct GnwStateData {
	char magic[4];
	int32_t version;
	char level[32];
	int32_t difficulty;
	int32_t checkX, checkY;
	int32_t ammo[5];
	int32_t ammoType, score, lives, fireSpeed, flockSize;
};

static Game *g_game;
static GnwStateData g_pending;
static bool g_has_pending;

void gnw_game_created(Game *game)
{
	g_game = game;
}

void gnw_game_destroyed(Game *game)
{
	if (g_game == game)
		g_game = nullptr;
}

struct GnwState {

	static bool save(GnwStateData &d) {
		Game *g = g_game;
		// only in a JJ1 level, single player
		if (!g || !level || !localPlayer || !g->levelFile || g->levelType != LT_JJ1)
			return false;
		if (g->mode && g->mode->getMode() != M_SINGLE)
			return false;
		memset(&d, 0, sizeof(d));
		memcpy(d.magic, "GWJJ", 4);
		d.version = STATE_VERSION;
		snprintf(d.level, sizeof(d.level), "%s", g->levelFile);
		d.difficulty = static_cast<int32_t>(g->difficulty);
		d.checkX = g->checkX;
		d.checkY = g->checkY;
		Player *p = localPlayer;
		for (int i = 0; i < 5; i++)
			d.ammo[i] = p->ammo[i];
		d.ammoType = p->ammoType;
		d.score = p->score;
		d.lives = p->lives;
		d.fireSpeed = p->fireSpeed;
		d.flockSize = p->flockSize;
		return true;
	}

	static int play(const GnwStateData &d) {
		LocalGame *g;
		try {
			g = new LocalGame(d.level, static_cast<difficultyType>(d.difficulty));
		} catch (int e) {
			return e;
		}
		Player *p = localPlayer;
		for (int i = 0; i < 5; i++)
			p->ammo[i] = d.ammo[i];
		p->ammoType = d.ammoType;
		p->score = d.score;
		p->lives = d.lives > 0 ? d.lives : 1;
		p->fireSpeed = d.fireSpeed;
		p->flockSize = d.flockSize;
		g->checkX = static_cast<short int>(d.checkX);
		g->checkY = static_cast<short int>(d.checkY);
		g->gnwCheckpointStart = true;
		int ret = g->play();
		delete g;
		return ret;
	}

};

extern "C" bool jj_core_save_state(const char *path)
{
	GnwStateData d;
	if (!GnwState::save(d)) {
		LOG_WARN("No savestate outside the levels");
		return false;
	}
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	bool ok = fwrite(&d, sizeof(d), 1, f) == 1;
	fclose(f);
	return ok;
}

extern "C" bool jj_core_load_state(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return false;
	GnwStateData d;
	bool ok = fread(&d, sizeof(d), 1, f) == 1;
	fclose(f);
	if (!ok || memcmp(d.magic, "GWJJ", 4) != 0 || d.version != STATE_VERSION)
		return false;
	d.level[sizeof(d.level) - 1] = '\0';
	g_pending = d;
	g_has_pending = true;
	return true;
}

extern "C" bool jj_core_can_save(void)
{
	GnwStateData d;
	return GnwState::save(d);
}

bool gnw_resume_pending()
{
	return g_has_pending;
}

int gnw_resume_play()
{
	g_has_pending = false;
	GnwStateData d = g_pending;
	return GnwState::play(d);
}

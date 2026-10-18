# OpenJazz sources of the port (relative to src/oj): JJ1 only (no JJ2,
# no other platforms), and the libraries of src/ext.
OJ_FILES = \
	game/clientgame.cpp game/game.cpp game/gamemode.cpp game/localgame.cpp \
	game/servergame.cpp \
	io/controls.cpp io/file.cpp io/log.cpp io/network.cpp \
	io/gfx/anim.cpp io/gfx/font.cpp io/gfx/paletteeffects.cpp \
	io/gfx/sprite.cpp io/gfx/video.cpp \
	level/level.cpp level/movable.cpp \
	main.cpp debug.cpp setup.cpp util.cpp version.cpp \
	menu/debugmenu.cpp menu/filemenu.cpp menu/gamemenu.cpp menu/mainmenu.cpp \
	menu/menu.cpp menu/plasma.cpp menu/setupmenu.cpp \
	platforms/platform_interface.cpp \
	player/player.cpp \
	jj1/jj1episodeutils.cpp \
	jj1/bonuslevel/jj1bonuslevel.cpp jj1/bonuslevel/jj1bonuslevelplayer.cpp \
	jj1/level/jj1bird.cpp jj1/level/jj1levelplayer.cpp \
	jj1/level/jj1levelplayerframe.cpp jj1/level/event/jj1bridge.cpp \
	jj1/level/event/jj1event.cpp jj1/level/event/jj1guardians.cpp \
	jj1/level/event/jj1standardevent.cpp jj1/level/jj1bullet.cpp \
	jj1/level/jj1demolevel.cpp jj1/level/jj1level.cpp \
	jj1/level/jj1levelframe.cpp jj1/level/jj1levelload.cpp \
	jj1/planet/jj1planet.cpp jj1/save/jj1save.cpp \
	jj1/scene/jj1scene.cpp jj1/scene/jj1sceneload.cpp


OJ_DEFS = -DNDEBUG -DPORTABLE -DOJ_SDL1=1 -DOJ_SDL2=0 -DOJ_SDL3=0

# relative to src/ext
XMP_FILES = \
	xmp/src/control.c xmp/src/dataio.c xmp/src/effects.c xmp/src/filetype.c \
	xmp/src/filter.c xmp/src/flow.c xmp/src/format.c xmp/src/hio.c \
	xmp/src/lfo.c xmp/src/load.c xmp/src/load_helpers.c xmp/src/memio.c \
	xmp/src/misc.c xmp/src/mix_all.c xmp/src/mixer.c xmp/src/period.c \
	xmp/src/player.c xmp/src/read_event.c xmp/src/rng.c xmp/src/scan.c \
	xmp/src/smix.c xmp/src/virtual.c xmp/src/win32.c \
	xmp/src/loaders/common.c xmp/src/loaders/iff.c \
	xmp/src/loaders/masi_load.c xmp/src/loaders/sample.c

XMP_DEFS = -DLIBXMP_CORE_PLAYER -DLIBXMP_CORE_DISABLE_IT -DLIBXMP_STATIC -DHAVE_POWF

EXT_FILES = miniz/miniz.c stb/stb_rect_pack.c

EXT_DEFS = -DMINIZ_NO_ARCHIVE_APIS

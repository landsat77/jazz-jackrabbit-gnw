# Jazz Jackrabbit (OpenJazz) — headless Linux harness
#
#   make            → build/jj_host
#   make SAN=1      → with AddressSanitizer + UBSan
#
#   ./build/jj_host Jazz.jj -t 20000 -e 1000 -o /tmp/jj

BUILD_DIR = build
TARGET = $(BUILD_DIR)/jj_host

OJ = ../src/oj
EXT = ../src/ext
PORT = ../src/port

include $(OJ)/sources.mk

OJ_SOURCES = $(addprefix $(OJ)/,$(OJ_FILES)) $(PORT)/jj_core.cpp $(PORT)/jj_sdl.cpp $(PORT)/jj_cxx.cpp $(PORT)/jj_sound.cpp $(PORT)/jj_render.cpp $(PORT)/jj_state.cpp
EXT_SOURCES = $(addprefix $(EXT)/,$(XMP_FILES) $(EXT_FILES))
PORT_SOURCES = \
	$(PORT)/jj_alloc.c \
	$(PORT)/jj_pack.c \
	$(PORT)/jj_file.c \
	$(PORT)/jj_bake.c \
	$(PORT)/jj_music.c \
	jj_host.c

LIVE ?= 0
ifeq ($(LIVE),1)
OPT = -O1 -g -fno-pie -fno-omit-frame-pointer -DJJ_LIVE_TRACE
LDFLAGS += -no-pie -rdynamic
PORT_SOURCES += jj_live.c
endif

SAN ?= 0
OPT ?= -O2 -g
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer -DJJ_HOST_SYSTEM_MALLOC
LDFLAGS += -fsanitize=address,undefined
endif

INCLUDES = -I$(PORT) -I$(PORT)/sdl -I$(OJ) -I$(EXT)/xmp -I$(EXT)/miniz -I$(EXT)/stb
WARN = -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
CFLAGS = $(OPT) -MMD -std=gnu11 -DGNW $(WARN) $(INCLUDES)
GLUE = -include $(PORT)/gnw_jj_glue.h
CXXFLAGS = $(OPT) -MMD -std=gnu++14 -DGNW $(OJ_DEFS) $(WARN) -Wno-class-memaccess \
	-fno-rtti $(INCLUDES) $(GLUE)
EXTFLAGS = $(OPT) -MMD -std=gnu11 -DGNW $(XMP_DEFS) $(EXT_DEFS) -w $(INCLUDES)

OJ_OBJECTS = $(addprefix $(BUILD_DIR)/oj_,$(subst /,_,$(OJ_FILES:.cpp=.o))) \
	$(BUILD_DIR)/p_jj_core.o $(BUILD_DIR)/p_jj_sdl.o $(BUILD_DIR)/p_jj_cxx.o $(BUILD_DIR)/p_jj_sound.o $(BUILD_DIR)/p_jj_render.o $(BUILD_DIR)/p_jj_state.o
EXT_OBJECTS = $(addprefix $(BUILD_DIR)/x_,$(subst /,_,$(XMP_FILES:.c=.o) $(EXT_FILES:.c=.o)))
PORT_OBJECTS = $(addprefix $(BUILD_DIR)/c_,$(notdir $(PORT_SOURCES:.c=.o)))

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $@

define oj_rule
$(BUILD_DIR)/oj_$(subst /,_,$(1:.cpp=.o)): $(OJ)/$(1) | $(BUILD_DIR)
	$$(CXX) -c $$(CXXFLAGS) $$< -o $$@
endef
$(foreach f,$(OJ_FILES),$(eval $(call oj_rule,$(f))))

define ext_rule
$(BUILD_DIR)/x_$(subst /,_,$(1:.c=.o)): $(EXT)/$(1) | $(BUILD_DIR)
	$$(CC) -c $$(EXTFLAGS) $$< -o $$@
endef
$(foreach f,$(XMP_FILES) $(EXT_FILES),$(eval $(call ext_rule,$(f))))

$(BUILD_DIR)/p_%.o: $(PORT)/%.cpp | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/c_%.o: $(PORT)/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/c_jj_host.o: jj_host.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/c_jj_live.o: jj_live.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OJ_OBJECTS) $(EXT_OBJECTS) $(PORT_OBJECTS)
	$(CXX) $^ $(LDFLAGS) -lm -o $@

-include $(wildcard $(BUILD_DIR)/*.d)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

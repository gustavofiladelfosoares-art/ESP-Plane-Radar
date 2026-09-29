# Desktop preview of the firmware UI (macOS/Linux, needs SDL2 headers only to
# satisfy LovyanGFX's desktop platform; no window is opened).
#
#   make -C sim            # build sim/build/sim
#   make -C sim preview    # render frames + sim/out/preview.html

ROOT := $(abspath ..)
LGFX_DIR ?= $(firstword $(wildcard $(ROOT)/.pio/libdeps/supermini/LovyanGFX) $(LGFX_FALLBACK))
LGFX_SRC := $(LGFX_DIR)/src
BUILD := build
PY ?= $(ROOT)/.venv/bin/python

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

CXXFLAGS := -std=c++17 -O2 -MMD -MP -DLGFX_SDL -DPR_ROOT='"$(ROOT)"' \
  -Iinclude -I$(ROOT)/include -I$(LGFX_SRC) $(SDL_CFLAGS) -Wno-deprecated-declarations
CFLAGS := -O1 -I$(LGFX_SRC)

# Firmware sources that are platform-independent (drawing + radar math).
APP_SRC := \
  $(ROOT)/src/ui/fonts.cpp \
  $(ROOT)/src/ui/draw_util.cpp \
  $(ROOT)/src/ui/pages.cpp \
  $(ROOT)/src/ui/page_weather.cpp \
  $(ROOT)/src/ui/page_clock.cpp \
  $(ROOT)/src/ui/page_nearest.cpp \
  $(ROOT)/src/ui/page_airsun.cpp \
  $(ROOT)/src/ui/page_about.cpp \
  $(ROOT)/src/ui/radar_display.cpp \
  $(ROOT)/src/ui/radar_map.cpp \
  $(ROOT)/src/ui/radar_range.cpp \
  $(ROOT)/src/ui/runway_overlay.cpp \
  $(ROOT)/src/hardware/display_font.cpp \
  $(ROOT)/src/services/radar_location.cpp \
  $(ROOT)/src/data/large_airports_data.cpp

LGFX_CPP := $(wildcard $(LGFX_SRC)/lgfx/v1/*.cpp)
LGFX_C := $(shell find $(LGFX_SRC)/lgfx/utility $(LGFX_SRC)/lgfx/Fonts $(LGFX_SRC)/lgfx/v1 -name '*.c')

APP_OBJ := $(patsubst $(ROOT)/%.cpp,$(BUILD)/app/%.o,$(APP_SRC)) $(BUILD)/sim_main.o
LGFX_OBJ := $(patsubst $(LGFX_SRC)/%.cpp,$(BUILD)/lgfx/%.o,$(LGFX_CPP)) \
            $(patsubst $(LGFX_SRC)/%.c,$(BUILD)/lgfx/%.o,$(LGFX_C))

$(BUILD)/sim: $(APP_OBJ) $(BUILD)/liblgfx.a
	$(CXX) $^ -o $@ $(SDL_LIBS)

$(BUILD)/liblgfx.a: $(LGFX_OBJ)
	ar rcs $@ $^

$(BUILD)/app/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/sim_main.o: sim_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/lgfx/%.o: $(LGFX_SRC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/lgfx/%.o: $(LGFX_SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rebuild objects when headers change.
-include $(APP_OBJ:.o=.d)

preview: $(BUILD)/sim
	cd $(ROOT) && sim/$(BUILD)/sim sim/out && $(PY) sim/make_preview.py sim/out

clean:
	rm -rf $(BUILD) out

.PHONY: preview clean

#include "hardware/display_font.h"

#include "hardware/display.h"
#include "ui/fonts.h"

namespace {

bool s_vlw_loaded = false;

}  // namespace

bool displayFontInit() {
  s_vlw_loaded = ui::fonts::use(tft, ui::fonts::Id::Ui);
  return s_vlw_loaded;
}

bool displayFontIsSmooth() { return s_vlw_loaded; }

bool displayFontEnsureLoaded(lgfx::LGFXBase& gfx) {
  if (!s_vlw_loaded) {
    return false;
  }
  // Pages switch between several VLW fonts; make sure the radar's is active.
  return ui::fonts::use(static_cast<lgfx::LovyanGFX&>(gfx), ui::fonts::Id::Ui);
}

void displayFontSetSmoothSize(lgfx::LGFXBase& gfx, float size) {
  gfx.setTextSize(size);
}

void displayFontSetBitmap(lgfx::LGFXBase& gfx, const lgfx::GFXfont* font) {
  gfx.setFont(font);
  gfx.setTextSize(1);
}

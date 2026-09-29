#include "ui/fonts.h"

#include "ui/color.h"

namespace ui {

bool g_swap_rb = false;

namespace fonts {

namespace {

struct Slot {
  const lgfx::LovyanGFX* gfx = nullptr;
  Id id = Id::Count;
};

// Only the panel and the frame sprite ever draw text; a few slots is plenty.
Slot s_slots[4];

Slot* slotFor(const lgfx::LovyanGFX& gfx) {
  for (Slot& s : s_slots) {
    if (s.gfx == &gfx) {
      return &s;
    }
  }
  for (Slot& s : s_slots) {
    if (s.gfx == nullptr) {
      s.gfx = &gfx;
      return &s;
    }
  }
  s_slots[0] = Slot{&gfx, Id::Count};
  return &s_slots[0];
}

bool vlwActive(const lgfx::LovyanGFX& gfx) {
  const lgfx::IFont* f = gfx.getFont();
  return f != nullptr && f->getType() == lgfx::IFont::font_type_t::ft_vlw;
}

}  // namespace

bool use(lgfx::LovyanGFX& gfx, Id id) {
  Slot* slot = slotFor(gfx);
  if (slot->id == id && vlwActive(gfx)) {
    gfx.setTextSize(1);
    return true;
  }
  const uint8_t* bytes = data(id);
  if (bytes == nullptr) {
    return false;
  }
  const bool ok = gfx.loadFont(bytes, lgfx::IFont::font_type_t::ft_vlw);
  slot->id = ok ? id : Id::Count;
  gfx.setTextSize(1);
  return ok;
}

void invalidate(lgfx::LovyanGFX& gfx) { slotFor(gfx)->id = Id::Count; }

}  // namespace fonts
}  // namespace ui

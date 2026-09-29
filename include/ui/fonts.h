#pragma once

#include <cstdint>

#include <LovyanGFX.hpp>

namespace ui::fonts {

/** Smooth (VLW) fonts. Ui is the original radar font; the rest cover Latin-1. */
enum class Id : uint8_t {
  Ui,        // Noto Sans Bold 15 (ASCII + °), used by the radar labels
  S14,       // Noto Sans Bold 14, Latin-1
  S17,       // Noto Sans Bold 17, Latin-1
  S22,       // Noto Sans Bold 22, Latin-1
  S28,       // Noto Sans Bold 28, Latin-1
  Digits54,  // Noto Sans Bold 54, digits and "-+:.,%°C" only
  Count,
};

/** Make `id` the active font on gfx (no-op when it already is) at size 1. */
bool use(lgfx::LovyanGFX& gfx, Id id);

/** Forget cached state for gfx (call after drawing with a bitmap font). */
void invalidate(lgfx::LovyanGFX& gfx);

/** Raw VLW bytes for id — provided per platform (firmware embeds, simulator loads files). */
const uint8_t* data(Id id);

}  // namespace ui::fonts

// Firmware font source: VLW files embedded by board_build.embed_files.

#include "ui/fonts.h"

#define EMBEDDED(name)                                          \
  extern const uint8_t _binary_##name##_start[] asm("_binary_" #name "_start")

extern "C" {
EMBEDDED(data_ui_font_vlw);
EMBEDDED(data_fonts_noto_bold_14_vlw);
EMBEDDED(data_fonts_noto_bold_17_vlw);
EMBEDDED(data_fonts_noto_bold_22_vlw);
EMBEDDED(data_fonts_noto_bold_28_vlw);
EMBEDDED(data_fonts_noto_bold_54_digits_vlw);
}

namespace ui::fonts {

const uint8_t* data(Id id) {
  switch (id) {
    case Id::Ui:
      return _binary_data_ui_font_vlw_start;
    case Id::S14:
      return _binary_data_fonts_noto_bold_14_vlw_start;
    case Id::S17:
      return _binary_data_fonts_noto_bold_17_vlw_start;
    case Id::S22:
      return _binary_data_fonts_noto_bold_22_vlw_start;
    case Id::S28:
      return _binary_data_fonts_noto_bold_28_vlw_start;
    case Id::Digits54:
      return _binary_data_fonts_noto_bold_54_digits_vlw_start;
    default:
      return nullptr;
  }
}

}  // namespace ui::fonts

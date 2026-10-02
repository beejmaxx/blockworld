#include "ui_font.hpp"
#include "ui_font_native.h"
#include <stdexcept>

namespace bw {
const FontAtlas& readableFont() {
  static const FontAtlas atlas=[] {
    FontAtlas result;
    std::array<BwFontGlyph,128> glyphs{};
    if(!bw_rasterize_font(result.pixels.data(),FontAtlas::width,FontAtlas::height,FontAtlas::em,glyphs.data()))
      throw std::runtime_error("Cannot rasterize the macOS interface font");
    for(std::size_t i=0;i<glyphs.size();++i) {
      const auto& source=glyphs[i]; auto& target=result.glyphs[i];
      target.uvMin={source.uv_min[0],source.uv_min[1]}; target.uvMax={source.uv_max[0],source.uv_max[1]};
      target.offset={source.offset[0],source.offset[1]}; target.size={source.size[0],source.size[1]}; target.advance=source.advance;
    }
    return result;
  }();
  return atlas;
}
float readableWidth(std::string_view text,float size) {
  float width=0; const auto& font=readableFont();
  for(unsigned char c : text) width+=font.glyphs[c<128 ? c : '?'].advance;
  return width*size/FontAtlas::em;
}
} // namespace bw

#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string_view>

namespace bw {
struct FontGlyph {
  glm::vec2 uvMin{},uvMax{},offset{},size{};
  float advance=0;
};
struct FontAtlas {
  static constexpr int width=1024,height=512;
  static constexpr float em=36;
  std::array<std::uint8_t,width*height> pixels{};
  std::array<FontGlyph,128> glyphs{};
};
const FontAtlas& readableFont();
float readableWidth(std::string_view text,float size);
} // namespace bw

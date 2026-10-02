#include "ui_font.hpp"
#include <CoreText/CoreText.h>
#include <algorithm>
#include <stdexcept>

namespace bw {
const FontAtlas& readableFont() {
  static const FontAtlas atlas=[] {
    FontAtlas result;
    auto font=CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,FontAtlas::em,nullptr);
    if(!font) throw std::runtime_error("Cannot load the macOS interface font");
    float ascent=float(CTFontGetAscent(font));
    auto gray=CGColorSpaceCreateDeviceGray();
    for(int code=32;code<127;++code) {
      UniChar character=UniChar(code); CGGlyph glyph{};
      CTFontGetGlyphsForCharacters(font,&character,&glyph,1);
      CGSize advance{}; CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,&glyph,&advance,1);
      auto& entry=result.glyphs[code]; entry.advance=float(advance.width);
      auto path=CTFontCreatePathForGlyph(font,glyph,nullptr);
      if(!path) continue;
      std::array<std::uint8_t,64*64> pixels{};
      auto context=CGBitmapContextCreate(pixels.data(),64,64,8,64,gray,kCGImageAlphaNone);
      if(!context) { CGPathRelease(path); CGColorSpaceRelease(gray); CFRelease(font); throw std::runtime_error("Cannot rasterize interface font"); }
      CGContextSetGrayFillColor(context,1,1);
      // Core Graphics paths use a bottom-left origin; bitmap rows are top-down.
      // A baseline at y=16 in this 64px context appears at row 48 in the atlas.
      CGContextTranslateCTM(context,8,16);
      CGContextAddPath(context,path); CGContextFillPath(context);
      CGContextRelease(context); CGPathRelease(path);
      int left=64,top=64,right=0,bottom=0;
      for(int y=0;y<64;++y) for(int x=0;x<64;++x) if(pixels[y*64+x]) {
        left=std::min(left,x); right=std::max(right,x+1); top=std::min(top,y); bottom=std::max(bottom,y+1);
      }
      if(left>=right) continue;
      // Transparent padding keeps the linear-filtered glyph edges clean.
      left=std::max(0,left-1); top=std::max(0,top-1); right=std::min(64,right+1); bottom=std::min(64,bottom+1);
      int gx=(code-32)%16*64,gy=(code-32)/16*64;
      for(int y=0;y<64;++y) for(int x=0;x<64;++x) result.pixels[(gy+y)*FontAtlas::width+gx+x]=pixels[y*64+x];
      entry.offset={float(left-8),float(top-48)+ascent}; entry.size={float(right-left),float(bottom-top)};
      entry.uvMin=glm::vec2(gx+left,gy+top)/glm::vec2(FontAtlas::width,FontAtlas::height);
      entry.uvMax=glm::vec2(gx+right,gy+bottom)/glm::vec2(FontAtlas::width,FontAtlas::height);
    }
    CGColorSpaceRelease(gray); CFRelease(font); return result;
  }();
  return atlas;
}
float readableWidth(std::string_view text,float size) {
  float width=0; const auto& font=readableFont();
  for(unsigned char c : text) width+=font.glyphs[c<128 ? c : '?'].advance;
  return width*size/FontAtlas::em;
}
} // namespace bw

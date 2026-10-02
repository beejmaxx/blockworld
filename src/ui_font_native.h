#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct BwFontGlyph {
  float uv_min[2],uv_max[2],offset[2],size[2],advance;
};

// C bridge: Apple's graphics SDK headers contain enum operations invalid in C++26.
int bw_rasterize_font(unsigned char* pixels,int width,int height,float em,struct BwFontGlyph* glyphs);

#ifdef __cplusplus
}
#endif

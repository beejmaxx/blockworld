#include "ui_font_native.h"
#include <CoreText/CoreText.h>
#include <string.h>

int bw_rasterize_font(unsigned char* atlas,int width,int height,float em,struct BwFontGlyph* glyphs) {
  if(width!=1024 || height!=512) return 0;
  CTFontRef font=CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,em,NULL);
  if(!font) return 0;
  float ascent=(float)CTFontGetAscent(font);
  CGColorSpaceRef gray=CGColorSpaceCreateDeviceGray();
  if(!gray) { CFRelease(font); return 0; }
  for(int code=32;code<127;++code) {
    UniChar character=(UniChar)code; CGGlyph glyph=0; CGSize advance={0,0};
    CTFontGetGlyphsForCharacters(font,&character,&glyph,1);
    CTFontGetAdvancesForGlyphs(font,kCTFontOrientationHorizontal,&glyph,&advance,1);
    struct BwFontGlyph* entry=&glyphs[code]; entry->advance=(float)advance.width;
    CGPathRef path=CTFontCreatePathForGlyph(font,glyph,NULL);
    if(!path) continue;
    unsigned char pixels[64*64]={0};
    CGContextRef context=CGBitmapContextCreate(pixels,64,64,8,64,gray,kCGImageAlphaNone);
    if(!context) { CGPathRelease(path); CGColorSpaceRelease(gray); CFRelease(font); return 0; }
    CGContextSetGrayFillColor(context,1,1);
    CGContextTranslateCTM(context,8,16);
    CGContextAddPath(context,path); CGContextFillPath(context);
    CGContextRelease(context); CGPathRelease(path);
    int left=64,top=64,right=0,bottom=0;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x) if(pixels[y*64+x]) {
      if(x<left) left=x; if(x+1>right) right=x+1;
      if(y<top) top=y; if(y+1>bottom) bottom=y+1;
    }
    if(left>=right) continue;
    if(left>0) --left; if(top>0) --top;
    if(right<64) ++right; if(bottom<64) ++bottom;
    int gx=(code-32)%16*64,gy=(code-32)/16*64;
    for(int y=0;y<64;++y) memcpy(atlas+(gy+y)*width+gx,pixels+y*64,64);
    entry->offset[0]=(float)(left-8); entry->offset[1]=(float)(top-48)+ascent;
    entry->size[0]=(float)(right-left); entry->size[1]=(float)(bottom-top);
    entry->uv_min[0]=(float)(gx+left)/width; entry->uv_min[1]=(float)(gy+top)/height;
    entry->uv_max[0]=(float)(gx+right)/width; entry->uv_max[1]=(float)(gy+bottom)/height;
  }
  CGColorSpaceRelease(gray); CFRelease(font); return 1;
}

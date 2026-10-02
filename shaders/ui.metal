#include <metal_stdlib>
using namespace metal;
struct Input { float2 position [[attribute(0)]]; float4 color [[attribute(1)]]; float2 uv [[attribute(2)]]; };
struct Output { float4 position [[position]]; float4 color; float2 uv; };
vertex Output uiVertex(Input in [[stage_in]],constant float4& screen [[buffer(0)]]) {
  Output out; out.position=float4(in.position.x/screen.x*2-1,1-in.position.y/screen.y*2,0,1); out.color=in.color; out.uv=in.uv; return out;
}
fragment float4 uiFragment(Output in [[stage_in]],texture2d<float> font [[texture(0)]],sampler filtering [[sampler(0)]]) {
  float4 color=in.color;
  if(in.uv.x>=0) color.a*=font.sample(filtering,in.uv).r;
  return color;
}

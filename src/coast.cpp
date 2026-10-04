#include "coast.hpp"
#include "city.hpp"
#include "farm.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
float smooth(float a,float b,float x) {
  float t=std::clamp((x-a)/(b-a),0.f,1.f); return t*t*(3-2*t);
}
float ellipse(float x,float z,float cx,float cz,float rx,float rz) {
  return (std::sqrt((x-cx)*(x-cx)/(rx*rx)+(z-cz)*(z-cz)/(rz*rz))-1)*std::min(rx,rz);
}
struct Shore { float mainland,island,distance; };
Shore shore(float x,float z) {
  // Three connected coves, a northern lagoon and a winding tidal channel.
  float warp=6.5f*std::sin(x*.047f+z*.013f)+3.5f*std::sin(z*.087f-x*.03f)
    +1.5f*std::sin((x+z)*.17f);
  float d=std::min({ellipse(x,z,173,239,109,74),ellipse(x,z,277,209,59,73),ellipse(x,z,109,180,48,47)});
  float lagoon=ellipse(x,z,239,112,43,31);
  float channelX=251+10*std::sin((z-130)*.044f);
  float channel=std::max(std::abs(x-channelX)-8.f,std::max(130-z,z-201));
  d=std::min({d,lagoon,channel})+warp;
  // An irregular island and a rocky headland break up the water's outline.
  float island=ellipse(x,z,182,236,21,16)+1.3f*std::sin(x*.23f+z*.18f);
  float headland=ellipse(x,z,309,280,33,34)+std::sin(z*.1f)*2;
  return {d,island,std::max({d,-island,-headland})};
}
float hill(float x,float z,float cx,float cz,float rx,float rz,float height) {
  float u=(x-cx)/rx,v=(z-cz)/rz;
  return height*std::exp(-2.2f*(u*u+v*v));
}
bool overlaps(Cell a,int size,Cell b,int other) {
  return a.x<b.x+other && a.x+size>b.x && a.z<b.z+other && a.z+size>b.z;
}
}
bool coastContains(Cell o,float x,float z,int margin) {
  return x>=o.x-margin && x<o.x+coastSize+margin && z>=o.z-margin && z<o.z+coastSize+margin;
}
CoastColumn coastColumn(const Terrain& terrain,Cell o,int wx,int wz) {
  float x=float(wx-o.x),z=float(wz-o.z);
  auto s=shore(x,z);
  float d=s.distance;
  float relief=hill(x,z,116,93,74,67,24)+hill(x,z,337,118,39,66,29)+hill(x,z,49,263,55,76,27);
  float grain=4.5f*std::sin(x*.07f+z*.01f)*std::cos(z*.061f)+1.2f*std::sin(x*.17f)*std::cos(z*.14f);
  float land=17+std::clamp(d,-25.f,16.f)*.48f+std::max(0.f,d-16)*.09f;
  land+=(relief+grain)*smooth(7,33,d);
  // Island has a low grassy crest above a ring of sand, not a square platform.
  if(s.island<0 && s.mainland<0) land=17+std::min(-s.island*.45f,7.f);
  // The coastal loop follows the mainland contour. Grade only its corridor;
  // beaches step down seaward and forested slopes continue behind it.
  float roadWeight=smooth(5,9,s.mainland)*(1-smooth(20,45,s.mainland));
  land=std::lerp(land,22.f,roadWeight);
  float boundary=std::min({x,z,float(coastSize-1)-x,float(coastSize-1)-z});
  land=std::lerp(float(terrain.height(wx,wz)),land,smooth(8,24,boundary));
  int ground=std::clamp(int(std::floor(land)),3,worldHeight-9);
  bool road=s.mainland>=10 && s.mainland<=18 && boundary>24;
  bool pavement=s.mainland>=8 && s.mainland<=20 && boundary>24 && !road;
  if(road || pavement) ground=22;
  bool water=ground<coastSeaLevel-1;
  return {ground,water,road,pavement,(!water && d<7) || (water && ground>coastSeaLevel-6)};
}
void generateCoast(Chunk& chunk,const Terrain& terrain,Cell o) {
  int bx=chunk.pos.x*chunkSize,bz=chunk.pos.z*chunkSize;
  if(!overlaps(o,coastSize,{bx,0,bz},chunkSize)) return;
  auto put=[&](int wx,int y,int wz,Block b) {
    if(wx>=bx && wx<bx+chunkSize && wz>=bz && wz<bz+chunkSize && y>0 && y<worldHeight
        && coastContains(o,float(wx),float(wz))) chunk.set(wx-bx,y,wz-bz,b);
  };
  for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x) {
    int wx=bx+x,wz=bz+z;
    if(!coastContains(o,float(wx),float(wz))) continue;
    auto c=coastColumn(terrain,o,wx,wz);
    for(int y=1;y<worldHeight;++y) {
      Block b=Block::Air;
      if(y<=c.ground) {
        b=y<c.ground-3 ? Block::Stone : c.beach || c.water ? Block::Sand : Block::Dirt;
        if(y==c.ground) b=c.road ? Block::Asphalt : c.pavement ? Block::Limestone : c.water || c.beach ? Block::Sand : Block::Grass;
        if(y==c.ground && !c.road && !c.pavement && !c.beach && c.ground>=43) b=Block::Stone;
      } else if(y<coastSeaLevel) b=Block::Water;
      put(wx,y,wz,b);
    }
    // Occasional half-height stone shore steps let swimmers walk out. Most
    // of the shoreline remains sand, with shallow shelves visible in water.
    if(c.beach && !c.water && c.ground==17 && ((wx-o.x)/12)%3==0) put(wx,17,wz,Block::StoneSlab);
  }
  // Rounded, overlapping canopies at stable anchors; crowns cross chunk seams.
  for(int gz=floorDiv(bz-5,9);gz<=floorDiv(bz+chunkSize+4,9);++gz)
    for(int gx=floorDiv(bx-5,9);gx<=floorDiv(bx+chunkSize+4,9);++gx) {
      auto spatial=PositionHash{}(ChunkPos{gx*19,gz*23});
      auto hash=std::uint32_t(spatial^(spatial>>32)^terrain.seed());
      int x=gx*9+int(hash%5)+2,z=gz*9+int((hash>>8)%5)+2;
      if(!coastContains(o,float(x),float(z)) || hash%100>73) continue;
      auto c=coastColumn(terrain,o,x,z);
      auto s=shore(float(x-o.x),float(z-o.z));
      if(c.water || c.beach || c.road || c.pavement || c.ground>44 || (s.mainland>4 && s.mainland<26)) continue;
      if(std::abs(coastColumn(terrain,o,x+3,z+3).ground-c.ground)>4) continue;
      int h=4+int((hash>>12)%3),radius=2+int((hash>>15)%2);
      for(int y=-2;y<=2;++y) for(int dz=-radius;dz<=radius;++dz) for(int dx=-radius;dx<=radius;++dx) {
        float sphere=(dx*dx+dz*dz)/float(radius*radius)+y*y/6.f;
        if(sphere>1.35f || (std::abs(dx)==radius && std::abs(dz)==radius)) continue;
        put(x+dx,c.ground+h+y,z+dz,Block::Leaves);
      }
      for(int y=1;y<=h;++y) put(x,c.ground+y,z,Block::Wood);
    }
  // A stepped swimming pier on the northern bay. The
  // terrain is the focus; no apartment template is stamped over the coast.
  int px=o.x+180,pz=o.z+154;
  for(int z=pz;z<=pz+28;++z) for(int x=px-3;x<=px+3;++x) {
    auto c=coastColumn(terrain,o,x,z);
    int step=z>pz+24 ? 8+z-pz-24 : std::clamp(z-pz-4,0,8),top=22-step/2;
    put(x,top,z,step%2 ? Block::StoneSlab : Block::Planks);
    for(int y=std::max(top+1,coastSeaLevel);y<top+4;++y) put(x,y,z,Block::Air);
    if((x==px-3 || x==px+3) && z%5==0) for(int y=c.ground+1;y<top;++y) put(x,y,z,Block::Wood);
  }
}
bool initializeCoast(World& w,const Player& p) {
  if(w.coastOrigin) return true;
  constexpr std::array sites{Cell{512,coastSeaLevel,-192},Cell{-896,coastSeaLevel,-192},Cell{512,coastSeaLevel,256},Cell{-896,coastSeaLevel,256}};
  for(auto o : sites) {
    if(coastContains(o,p.pose.position.x,p.pose.position.z) || w.editedIn({o.x,1,o.z},{o.x+coastSize-1,worldHeight-1,o.z+coastSize-1})) continue;
    if(w.cityOrigin && overlaps(o,coastSize,*w.cityOrigin,citySize)) continue;
    if(w.castleOrigin && overlaps(o,coastSize,*w.castleOrigin,50)) continue;
    auto contains=[&](glm::vec3 at){return coastContains(o,at.x,at.z);};
    if(w.farm.car.owned && contains(w.farm.car.position)) continue;
    if(w.farm.initialized && contains(w.farm.home)) continue;
    if(std::ranges::any_of(w.farm.chickens,[&](const auto& a){return contains(a.position);})
       || std::ranges::any_of(w.farm.livestock,[&](const auto& a){return contains(a.position) || contains(a.home);})) continue;
    w.coastOrigin=o;
    std::vector<ChunkPos> loaded;
    for(const auto& [c,chunk] : w.chunks) if(overlaps(o,coastSize,{c.x*chunkSize,0,c.z*chunkSize},chunkSize)) loaded.push_back(c);
    for(auto c : loaded) w.insert(w.terrain.generate(c));
    return true;
  }
  return false;
}
bool visitCoast(World& w,Player& p) {
  if(!initializeCoast(w,p)) return false;
  auto o=*w.coastOrigin;
  // Find clear pavement near the northern bay. A wide graded road is a usable
  // starting point for walking and car delivery, rather than a flying camera.
  for(int z=140;z<=163;++z) for(int x=174;x<=190;++x) {
    auto c=coastColumn(w.terrain,o,o.x+x,o.z+z);
    auto s=shore(float(x),float(z));
    if(!c.road || std::abs(s.mainland-14)>1) continue;
    w.ensure(chunkAt(o.x+x,o.z+z),3);
    glm::vec3 at{o.x+x+.5f,float(c.ground+1),o.z+z+.5f};
    Box box{at+glm::vec3(-.3f,0,-.3f),at+glm::vec3(.3f,1.8f,.3f)};
    if(p.collides(w,at) || !opaque(w.get({o.x+x,c.ground,o.z+z})) || chickensOverlap(w,box) || ranchOverlap(w,box)) continue;
    float dx=shore(float(x+1),float(z)).mainland-shore(float(x-1),float(z)).mainland;
    float dz=shore(float(x),float(z+1)).mainland-shore(float(x),float(z-1)).mainland;
    p.pose.position=at; p.pose.yaw=std::atan2(-dz,-dx); p.pose.pitch=-.08f;
    p.stopFlying(); p.grounded=false; p.sneaking=false; return true;
  }
  return false;
}
bool atCoast(const World& w,const Player& p) { return w.coastOrigin && coastContains(*w.coastOrigin,p.pose.position.x,p.pose.position.z); }
} // namespace bw

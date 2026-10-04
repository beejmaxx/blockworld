#include "countryside.hpp"
#include "road.hpp"
#include "coast.hpp"
#include "city.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace bw {
namespace {
struct Field {int x,z,width,depth;CropKind crop;};
constexpr std::array fields{
  Field{8,50,36,16,CropKind::Wheat},Field{68,50,36,16,CropKind::Carrot},
  Field{8,74,36,16,CropKind::Strawberry},Field{68,74,36,16,CropKind::Pumpkin}};
class Builder {
public:
  Chunk& chunk;Cell o;
  void fill(int x0,int y0,int z0,int x1,int y1,int z1,Block b) {
    int bx=chunk.pos.x*chunkSize-o.x,bz=chunk.pos.z*chunkSize-o.z;
    x0=std::max(x0,bx);x1=std::min(x1,bx+chunkSize-1);z0=std::max(z0,bz);z1=std::min(z1,bz+chunkSize-1);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)for(int y=std::max(y0,1-o.y);y<=std::min(y1,worldHeight-1-o.y);++y)
      chunk.set(x-bx,y+o.y,z-bz,b);
  }
  void put(int x,int y,int z,Block b){fill(x,y,z,x,y,z,b);}
  void fence(int x,int z,int w,int d) {
    fill(x,0,z,x+w,0,z,Block::Fence);fill(x,0,z+d,x+w,0,z+d,Block::Fence);
    fill(x,0,z,x,0,z+d,Block::Fence);fill(x+w,0,z,x+w,0,z+d,Block::Fence);
    put(x+w/2,0,z,Block::GateZ);
  }
  void barn(int x,int z,int width,int depth) {
    fill(x,-1,z,x+width-1,-1,z+depth-1,Block::Planks);
    for(int y=0;y<=4;++y) {
      fill(x,y,z,x+width-1,y,z,Block::Terracotta);fill(x,y,z+depth-1,x+width-1,y,z+depth-1,Block::Terracotta);
      fill(x,y,z,x,y,z+depth-1,Block::Terracotta);fill(x+width-1,y,z,x+width-1,y,z+depth-1,Block::Terracotta);
    }
    for(int side:{x,x+width-1}) {
      fill(side,0,z,side,4,z,Block::Concrete);fill(side,0,z+depth-1,side,4,z+depth-1,Block::Concrete);
      for(int window=z+5;window<z+depth-3;window+=5)fill(side,2,window,side,3,window+2,Block::Glass);
    }
    fill(x,4,z,x+width-1,4,z,Block::Concrete);
    fill(x+width/2-2,0,z,x+width/2+2,3,z,Block::Air); // Wide open barn doors.
    for(int xx=-1;xx<=width;++xx) {
      int roof=5+std::max(0,std::min(xx,width-1-xx))/2;
      fill(x+xx,roof,z-1,x+xx,roof,z+depth,Block::Charcoal);
      if(xx>=0 && xx<width)for(int end:{z,z+depth-1})fill(x+xx,5,end,x+xx,roof-1,end,Block::Terracotta);
    }
    fill(x+2,0,z+depth-6,x+5,1,z+depth-3,Block::GoldTile);
    fill(x+width-5,0,z+depth-6,x+width-3,2,z+depth-3,Block::GoldTile);
    put(x+width/2,3,z+depth/2,Block::Lamp);
  }
};
bool overlaps(int x,int z,int w,int d,Cell o,int width,int depth) {
  return x<o.x+width && x+w>o.x && z<o.z+depth && z+d>o.z;
}
}
void generateCountryside(Chunk& chunk,Cell o) {
  int bx=chunk.pos.x*chunkSize,bz=chunk.pos.z*chunkSize;
  if(!overlaps(bx,bz,chunkSize,chunkSize,o+Cell{-8,0,-24},countrysideWidth+16,countrysideDepth+32))return;
  Builder b{chunk,o};
  for(int z=0;z<chunkSize;++z)for(int x=0;x<chunkSize;++x) {
    int xx=bx+x-o.x,zz=bz+z-o.z;
    bool site=xx>=0 && xx<countrysideWidth && zz>=0 && zz<countrysideDepth;
    bool approach=xx>=48 && xx<=64 && zz>=-24 && zz<0;
    float edge=std::hypot(float(std::max({0,-xx,xx-countrysideWidth+1})),float(std::max({0,-zz,zz-countrysideDepth+1})));
    if(!site && !approach && edge>=8)continue;
    int surface=o.y-1;
    if(!site && !approach) {
      int natural=surface;
      for(int y=worldHeight-1;y>0;--y) {
        auto block=chunk.get(x,y,z);
        if(opaque(block) && block!=Block::Wood && block!=Block::Leaves){natural=y;break;}
      }
      surface=int(std::round(std::lerp(float(surface),float(natural),edge/8)));
    }
    bool lane=(site || approach) && ((xx>=51 && xx<=61) || (site && ((zz>=44 && zz<=47) || (zz>=68 && zz<=71))));
    for(int y=1;y<worldHeight;++y)chunk.set(x,y,z,y<surface-2 ? Block::Stone : y<surface ? Block::Dirt : y==surface ? (lane ? Block::Sand : Block::Grass) : Block::Air);
  }
  // Buildings frame a yard and leave the central lane wide enough for the GT2.
  b.barn(8,5,19,21);b.barn(79,5,23,20);
  for(int x=30;x<=36;++x)for(int z=10;z<=16;++z) {
    float radius=std::hypot(x-33.f,z-13.f);if(radius>3.7f)continue;
    b.fill(x,0,z,x,11,z,radius>2.4f ? Block::Concrete : Block::Air);
    b.put(x,12,z,Block::Charcoal);
    if(radius<2.5f)b.put(x,13,z,Block::Charcoal);
  }
  b.fence(6,30,38,12);b.fence(68,30,38,12);
  // A glass nursery beside the granary. Its aisle is open at both ends.
  b.fill(39,-1,5,48,-1,25,Block::Planks);
  for(int y=0;y<=3;++y) {
    b.fill(39,y,5,39,y,25,Block::Glass);b.fill(48,y,5,48,y,25,Block::Glass);
    b.fill(39,y,5,48,y,5,Block::Glass);b.fill(39,y,25,48,y,25,Block::Glass);
  }
  b.fill(39,4,5,48,4,25,Block::Glass);b.fill(43,0,5,44,2,5,Block::Air);b.fill(43,0,25,44,2,25,Block::Air);
  for(int x:{39,48})for(int z:{5,15,25})b.fill(x,0,z,x,4,z,Block::Wood);
  for(int x:{40,41,46,47})b.fill(x,-1,7,x,-1,23,Block::Farmland);
  for(auto f:fields) {
    b.fill(f.x-1,-1,f.z-1,f.x+f.width,-1,f.z+f.depth,Block::Planks);
    b.fill(f.x,-1,f.z,f.x+f.width-1,-1,f.z+f.depth-1,Block::Farmland);
    // Visible irrigation channels and walkways divide long crop rows.
    for(int x=f.x+7;x<f.x+f.width;x+=9)b.fill(x,-1,f.z,x,-1,f.z+f.depth-1,Block::Water);
  }
  // Shaded produce stand, benches and a bed in the barn for a night's rest.
  for(int x:{48,64})b.fill(x,0,31,x,3,31,Block::Wood);
  for(int x=47;x<=65;++x)b.fill(x,4,29,x,4,33,(x/2)%2 ? Block::Concrete : Block::RedTile);
  b.fill(48,0,32,50,0,32,Block::Table);b.fill(62,0,32,64,0,32,Block::Table);
  for(int x:{48,64})b.put(x,1,32,Block::Planter);
  b.put(10,0,8,Block::BedZ);b.put(10,0,7,Block::BedZHead);b.put(12,0,7,Block::Table);b.put(12,1,7,Block::Lamp);
  for(int z:{-4,2,48,72,92})for(int x:{49,63}) {b.fill(x,0,z,x,2,z,Block::Wood);b.put(x,3,z,Block::Lamp);}
}
bool initializeCountryside(World& w,const Player& player) {
  if(w.countrysideOrigin)return true;
  if(!initializeRoad(w,player))return false;
  float length=sampleRoad(w.road,w.road.front().x,w.road.front().z).length;
  for(float fraction:{.72f,.60f,.48f,.36f}) {
    auto anchor=w.road.front();float distance=0;
    for(std::size_t i=1;i<w.road.size();++i) {
      float segment=glm::length(glm::vec2(w.road[i].x-anchor.x,w.road[i].z-anchor.z));
      if(distance+segment>=length*fraction) {anchor=glm::mix(anchor,w.road[i],(length*fraction-distance)/segment);break;}
      distance+=segment;anchor=w.road[i];
    }
    Cell o{int(std::round(anchor.x))-56,int(std::round(anchor.y)),int(std::round(anchor.z))+24};
    auto contains=[&](glm::vec3 p){return p.x>=o.x-8 && p.x<=o.x+countrysideWidth+8 && p.z>=o.z-24 && p.z<=o.z+countrysideDepth+8;};
    if(contains(player.pose.position) || contains(w.farm.home) || (w.farm.car.owned && contains(w.farm.car.position)))continue;
    if(w.editedIn(o+Cell{-8,1-o.y,-24},o+Cell{countrysideWidth+8,worldHeight-1-o.y,countrysideDepth+8}))continue;
    bool blocked=false;
    // Leave the existing highway intact, including bends past the entrance.
    for(int z=0;z<=countrysideDepth;z+=8)for(int x=0;x<=countrysideWidth;x+=8)
      blocked|=sampleRoad(w.road,float(o.x+x),float(o.z+z)).distance<roadClearance+4;
    for(auto [site,width,depth]:{std::tuple{w.coastOrigin,coastSize,coastSize},std::tuple{w.cityOrigin,citySize,citySize},std::tuple{w.castleOrigin,50,50}})
      if(site)blocked|=overlaps(o.x-8,o.z-24,countrysideWidth+16,countrysideDepth+32,*site,width,depth);
    for(auto a:w.farm.chickens)blocked|=contains(a.position);
    for(auto a:w.farm.livestock)blocked|=contains(a.position) || contains(a.home);
    if(blocked)continue;
    w.countrysideOrigin=o;
    // Regenerate only loaded parcel chunks, then load enough to plant real,
    // harvestable crops once. Future streaming never replants harvested rows.
    std::vector<ChunkPos> loaded;for(const auto& [pos,chunk]:w.chunks)
      if(overlaps(pos.x*chunkSize,pos.z*chunkSize,chunkSize,chunkSize,o+Cell{-8,0,-24},countrysideWidth+16,countrysideDepth+32))loaded.push_back(pos);
    for(auto pos:loaded)w.insert(w.terrain.generate(pos));
    for(int z=-1;z<=countrysideDepth;z+=16)for(int x=0;x<=countrysideWidth;x+=16)w.ensure(chunkAt(o.x+x,o.z+z),0);
    auto plant=[&](int x,int z,CropKind kind,float age) {
      if(w.farm.crops.size()>=cropLimit)return;
      Cell c=o+Cell{x,0,z};w.ensure(chunkAt(c.x,c.z),0);
      if(w.get(c+Cell{0,-1,0})==Block::Farmland)w.set(c,cropStage(kind,cropGrowSeconds(kind)*age));
    };
    for(auto f:fields) {
      int stride=f.crop==CropKind::Pumpkin ? 3 : 2;
      for(int z=f.z;z<f.z+f.depth;z+=stride)for(int x=f.x;x<f.x+f.width;x+=stride)
        plant(x,z,f.crop,(z-f.z)%6==0 ? .4f : 1.f);
    }
    for(int x:{40,41,46,47})for(int z=7;z<=23;z+=2)plant(x,z,CropKind::Strawberry,.6f);
    for(auto [kind,x,z]:{std::tuple{LivestockKind::Cow,14,35},std::tuple{LivestockKind::Cow,24,38},std::tuple{LivestockKind::Horse,78,35},std::tuple{LivestockKind::Sheep,94,38}}) {
      if(w.farm.livestock.size()>=livestockLimit)break;
      Livestock a;a.kind=kind;a.position=a.home={o.x+x+.5f,float(o.y),o.z+z+.5f};w.farm.livestock.push_back(a);
    }
    return true;
  }
  return false;
}
bool countrysideContains(Cell o,float x,float z,float margin) {
  return x>=o.x-margin && x<o.x+countrysideWidth+margin && z>=o.z-24-margin && z<o.z+countrysideDepth+margin;
}
bool atCountryside(const World& w,const Player& p) {
  return w.countrysideOrigin && countrysideContains(*w.countrysideOrigin,p.pose.position.x,p.pose.position.z);
}
} // namespace bw

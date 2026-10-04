#include "harbor.hpp"
#include "farm.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bw {
namespace {
// Buildings face the water. The southern skyline steps down toward the west;
// the lagoon and eastern shore have smaller, separate neighbourhoods.
constexpr std::array buildings{
  HarborBuilding{205,335,40,38,8,0,2,Block::Concrete,Block::Limestone,"Bay terraces"},
  HarborBuilding{164,347,34,30,7,1,2,Block::Terracotta,Block::Concrete,"Terracotta court"},
  HarborBuilding{122,342,30,32,5,2,2,Block::Sage,Block::Limestone,"Garden apartments"},
  HarborBuilding{75,323,28,32,8,3,2,Block::Concrete,Block::Charcoal,"Marina house"},
  HarborBuilding{270,336,26,28,14,4,2,Block::BlueGlass,Block::Concrete,"Pearl tower"},
  HarborBuilding{314,318,30,32,10,3,2,Block::Concrete,Block::BlueTile,"Cobalt court"},
  HarborBuilding{360,233,30,18,7,2,1,Block::Sage,Block::Concrete,"East garden"},
  HarborBuilding{360,175,30,18,9,1,1,Block::Terracotta,Block::Limestone,"Cove house"},
  HarborBuilding{17,168,28,16,4,1,3,Block::Terracotta,Block::Concrete,"West quay"},
  HarborBuilding{250,31,30,24,6,2,0,Block::Limestone,Block::Sage,"Lagoon residence"}
};
constexpr std::uint32_t allLots=(1u<<buildings.size())-1;
struct Rect { int x0,z0,x1,z1; };
Rect parcel(const HarborBuilding& b) {
  int w=b.turn%2 ? b.depth : b.width,d=b.turn%2 ? b.width : b.depth;
  return {b.x-2,b.z-2,b.x+w+1,b.z+d+1};
}
bool contains(Rect r,int x,int z) { return x>=r.x0 && x<=r.x1 && z>=r.z0 && z<=r.z1; }
// Both block geometry and walking waypoints use the same rotation.
Cell cell(const HarborBuilding& b,int x,int y,int z) {
  if(b.turn==1) return {b.x+b.depth-1-z,y,b.z+x};
  if(b.turn==2) return {b.x+b.width-1-x,y,b.z+b.depth-1-z};
  if(b.turn==3) return {b.x+z,y,b.z+b.width-1-x};
  return {b.x+x,y,b.z+z};
}
class Builder {
public:
  Chunk& chunk; Cell coast; const HarborBuilding& b;
  void worldFill(int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
    int bx=chunk.pos.x*chunkSize-coast.x,bz=chunk.pos.z*chunkSize-coast.z;
    x0=std::max(x0,bx); x1=std::min(x1,bx+chunkSize-1);
    z0=std::max(z0,bz); z1=std::min(z1,bz+chunkSize-1);
    y0=std::max(y0,1); y1=std::min(y1,worldHeight-1);
    for(int z=z0;z<=z1;++z) for(int x=x0;x<=x1;++x) for(int y=y0;y<=y1;++y)
      chunk.set(x-bx,y,z-bz,block);
  }
  void fill(int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
    if(isBed(block)) block=bedVariant(b.turn%2,bedHead(block)^(b.turn>=2));
    auto a=cell(b,x0,y0+harborGround,z0),c=cell(b,x1,y1+harborGround,z1);
    worldFill(std::min(a.x,c.x),a.y,std::min(a.z,c.z),std::max(a.x,c.x),c.y,std::max(a.z,c.z),block);
  }
  void put(int x,int y,int z,Block block) { fill(x,y,z,x,y,z,block); }
  void step(int x0,int x1,int z0,int z1,int halfHeight,Block trim) {
    int y=halfHeight/2;
    fill(x0,halfHeight%2 ? y : y-1,z0,x1,halfHeight%2 ? y : y-1,z1,halfHeight%2 ? Block::StoneSlab : trim);
  }
  void stairs(int floors) {
    for(int floor=0;floor<floors;++floor) {
      int y=floor*harborFloorHeight;
      for(int n=0;n<5;++n) {
        step(2,4,11-n,11-n,y*2+n+1,b.trim);
        step(6,8,7+n,7+n,y*2+n+6,b.trim);
      }
      step(2,8,4,6,y*2+5,b.trim);
      fill(5,y,7,5,y+4,11,b.trim);
    }
  }
  void windowWall(int loX,int loZ,int hiX,int hiZ,int y,int h,Block wall) {
    fill(loX,y,loZ,hiX,y+h,hiZ,wall);
    if(loZ==hiZ) {
      for(int x=loX+2;x<hiX-1;x+=5) fill(x,y+1,loZ,std::min(x+2,hiX-1),y+h-1,loZ,Block::Glass);
    } else {
      for(int z=loZ+2;z<hiZ-1;z+=5) fill(loX,y+1,z,loX,y+h-1,std::min(z+2,hiZ-1),Block::Glass);
    }
  }
  void rail(int x0,int z0,int x1,int z1,int y) {
    fill(x0,y,z0,x1,y,z1,Block::Glass);
  }
  void planter(int x,int y,int z,int length=3) {
    fill(x,y,z,x+length-1,y,z,Block::Planter);
  }
  void pergola(int x0,int z0,int x1,int z1,int y) {
    for(int x : {x0,x1}) for(int z : {z0,z1}) fill(x,y,z,x,y+4,z,Block::Concrete);
    fill(x0-1,y+5,z0-1,x1+1,y+5,z1+1,Block::Glass);
    fill(x0-1,y+5,z0-1,x1+1,y+5,z0-1,b.trim); fill(x0-1,y+5,z1+1,x1+1,y+5,z1+1,b.trim);
    for(int x=x0-1;x<=x1+1;x+=3) fill(x,y+5,z0-1,x,y+5,z1+1,b.trim);
    put(x0+1,y+4,z0+1,Block::Lamp);
  }
  void apartment(int floor) {
    int y=floor*harborFloorHeight,w=b.width,d=b.depth;
    // Kitchen, lounge and bedroom sit beyond the clear stair landing.
    fill(w-8,y,2,w-3,y,2,Block::Concrete);
    put(w-8,y+1,2,Block::Charcoal); put(w-5,y+1,2,Block::BlueTile); put(w-3,y+1,2,Block::Planter);
    fill(w-8,y,6,w-5,y,6,Block::Sofa); fill(w-7,y,4,w-6,y,4,Block::Table);
    put(w-9,y,4,Block::Chair);
    if(d>20) {
      fill(w-10,y,10,w-2,y+3,10,b.trim); fill(w-7,y,10,w-6,y+2,10,Block::Air);
      put(w-5,y,d-5,Block::BedZ); put(w-5,y,d-6,Block::BedZHead);
      put(w-3,y,d-5,Block::Table); put(w-3,y+1,d-5,Block::Lamp);
      put(w-9,y,d-4,Block::Planter);
    }
    if(floor==0) {
      fill(17,y,d-5,w-4,y,d-5,Block::Table);
      for(int x=18;x<w-4;x+=3) { put(x,y,d-7,Block::Chair); put(x,y,d-3,Block::Planter); }
    }
    put(10,y+3,4,Block::Lamp);
  }
  void penthouse(int roof) {
    int w=b.width,d=b.depth;
    // The first terrace wraps a set-back, glazed living pavilion. Its roof is
    // a second terrace with a smaller bedroom pavilion and a walk-up sun deck.
    fill(0,roof-1,0,w-1,roof-1,d-1,Block::Limestone);
    fill(2,roof-1,5,8,roof-1,11,Block::Air);
    rail(0,0,w-1,0,roof); rail(0,d-1,w-1,d-1,roof);
    rail(0,0,0,d-1,roof); rail(w-1,0,w-1,d-1,roof);
    rail(1,4,1,11,roof); rail(9,4,9,11,roof); rail(2,4,8,4,roof);
    int right=w-4,back=d-13;
    if(b.style==0) {
      fill(11,roof,2,right,roof+4,back,Block::Glass);
      fill(12,roof,3,right-1,roof+4,back-1,Block::Air);
      for(int x : {11,right}) for(int z : {2,back}) fill(x,roof,z,x,roof+4,z,Block::Concrete);
      fill(10,roof+5,1,right+1,roof+5,back+1,Block::Concrete);
      fill(14,roof+5,5,right-3,roof+5,back-3,Block::Glass);
      // Wide door opening gives a continuous route from the stairs to the lounge.
      fill(11,roof,12,11,roof+3,14,Block::Air);
      fill(12,roof,back,15,roof+3,back,Block::Air);
      fill(15,roof,8,20,roof,8,Block::Sofa); fill(16,roof,5,18,roof,5,Block::Table);
      fill(right-8,roof,4,right-2,roof,4,Block::Concrete);
      put(right-7,roof+1,4,Block::Charcoal); put(right-3,roof+1,4,Block::BlueTile);
      fill(right-7,roof,9,right-4,roof,10,Block::Table);
      for(int x : {right-7,right-4}) { put(x,roof,8,Block::Chair); put(x,roof,12,Block::Chair); }
      put(13,roof,4,Block::Planter); put(right-2,roof,back-2,Block::Planter);
      put(19,roof+4,4,Block::Lamp); put(right-3,roof+4,back-2,Block::Lamp);
      // Second floor: full glass front, a real bed and a private balcony.
      fill(14,roof-1,4,21,roof-1,9,Block::Sage);
      int upper=roof+6;
      fill(14,upper-1,3,right-3,upper-1,13,Block::Planks);
      fill(14,upper,3,right-3,upper+3,13,Block::Glass);
      fill(15,upper,4,right-4,upper+3,12,Block::Air);
      fill(13,upper+4,2,right-2,upper+4,14,Block::Concrete);
      fill(17,upper+4,5,right-6,upper+4,10,Block::Glass);
      fill(14,upper,11,14,upper+2,12,Block::Air);
      put(right-6,upper,7,Block::BedZ); put(right-6,upper,6,Block::BedZHead);
      put(right-4,upper,7,Block::Table); put(right-4,upper+1,7,Block::Lamp);
      fill(17,upper,11,20,upper,11,Block::Sofa);
      rail(10,1,right+1,1,upper); rail(right+1,1,right+1,back+1,upper);
      rail(10,back+1,right+1,back+1,upper);
      // Broad exterior half-steps climb beside the pavilion, with no door maze.
      fill(9,roof,back-9,10,roof+9,back+2,Block::Air);
      for(int n=0;n<12;++n) step(9,10,back+2-n,back+2-n,roof*2+n+1,b.trim);
      fill(9,upper-1,back-11,13,upper-1,back-9,Block::Concrete);
      // Pool faces the bay, inset into a deck wide enough to walk around.
      fill(19,roof-2,d-10,w-5,roof-2,d-4,Block::BlueTile);
      fill(19,roof-1,d-10,w-5,roof-1,d-4,Block::Water);
      fill(19,roof-1,d-4,w-5,roof-1,d-4,Block::StoneSlab);
      pergola(3,d-11,13,d-4,roof);
      fill(5,roof,d-8,10,roof,d-8,Block::Sofa); fill(6,roof,d-6,9,roof,d-6,Block::Table);
      planter(2,roof,d-2,12); planter(w-2,roof,5,1); planter(2,roof,1,6);
    } else {
      int x0=11,z0=2,z1=std::max(7,d-9);
      fill(x0,roof,z0,w-3,roof+3,z1,Block::Glass);
      fill(x0+1,roof,z0+1,w-4,roof+3,z1-1,Block::Air);
      fill(x0-1,roof+4,z0-1,w-2,roof+4,z1+1,b.trim);
      fill(x0+3,roof+4,z0+3,w-5,roof+4,z1-2,Block::Glass);
      fill(x0,roof,7,x0,roof+2,9,Block::Air);
      put(w-6,roof,6,Block::BedZ); put(w-6,roof,5,Block::BedZHead);
      put(w-4,roof,5,Block::Planter); put(w-4,roof+2,7,Block::Lamp);
      if(d>22) {
        fill(12,roof-2,d-7,w-4,roof-2,d-3,Block::BlueTile);
        fill(12,roof-1,d-7,w-4,roof-1,d-3,Block::Water);
        fill(12,roof-1,d-3,w-4,roof-1,d-3,Block::StoneSlab);
        pergola(2,d-8,8,d-3,roof);
        fill(3,roof,d-5,7,roof,d-5,Block::Sofa);
      }
      planter(2,roof,1,6); put(w-3,roof,d-2,Block::Planter);
    }
  }
  void build() {
    int w=b.width,d=b.depth,roof=b.floors*harborFloorHeight;
    // Clear just this building's footprint. The bay and the hills between
    // parcels keep their terrain and vegetation.
    fill(-2,1-harborGround,-2,w+1,-2,d+1,Block::Stone);
    fill(-2,-1,-2,w+1,-1,d+1,Block::Limestone);
    fill(-2,0,-2,w+1,worldHeight-harborGround-1,d+1,Block::Air);
    for(int level=0;level<b.floors;++level) {
      int y=level*harborFloorHeight;
      fill(0,y-1,0,w-1,y-1,d-1,level==0 ? Block::Limestone : Block::Planks);
      windowWall(0,0,w-1,0,y,4,b.wall); windowWall(0,d-1,w-1,d-1,y,4,b.wall);
      windowWall(0,0,0,d-1,y,4,b.wall); windowWall(w-1,0,w-1,d-1,y,4,b.wall);
      for(int x : {0,w-1}) fill(x,y,0,x,y+4,d-1,b.trim);
      // Side glazing is recessed between paired vertical piers.
      for(int z=3;z<d-3;z+=5) {
        fill(0,y+1,z,0,y+3,z+2,Block::Glass); fill(w-1,y+1,z,w-1,y+3,z+2,Block::Glass);
      }
      if(b.style==4) {
        for(int z=0;z<d;z+=5) { fill(-1,y,z,-1,y+4,z,b.trim); fill(w,y,z,w,y+4,z,b.trim); }
        for(int x=1;x<w;x+=6) { fill(x,y,-1,x,y+4,-1,b.trim); fill(x,y,d,x,y+4,d,b.trim); }
      } else {
        fill(-1,y-1,-1,w,y-1,-1,b.trim); fill(-1,y-1,d,w,y-1,d,b.trim);
        fill(-1,y-1,0,-1,y-1,d-1,b.trim); fill(w,y-1,0,w,y-1,d-1,b.trim);
        if(level>0 && (b.style!=3 || level%2==0)) {
          // Individual recessed balconies alternate with full-height window bays.
          for(int x=2;x<w-4;x+=8) {
            fill(x,y-1,d,x+5,y-1,d+1,b.trim); rail(x,d+1,x+5,d+1,y);
            put(x,y,d,Block::Glass); put(x+5,y,d,Block::Glass);
            fill(x+2,y,d-1,x+3,y+2,d-1,Block::Air);
            put(x+1,y,d,Block::Planter);
          }
        }
      }
      if(level>0) fill(2,y-1,5,8,y-1,11,Block::Air);
      fill(9,y,4,9,y+3,11,b.trim);
      apartment(level);
    }
    // Street-level lobby and cafe have tall clear windows, canopies and lamps.
    fill(11,0,d-1,14,3,d-1,Block::Air);
    fill(10,4,d,15,4,d+2,Block::Glass);
    put(10,2,d-1,Block::Lamp); put(15,2,d-1,Block::Lamp);
    penthouse(roof); stairs(b.floors);
    if(b.style==4) {
      // A faceted crown gives the tallest tower a distinct skyline silhouette.
      for(int y=roof+5;y<=roof+18;++y) {
        int r=std::max(1,10-(y-roof-5)*9/13);
        fill(w/2-r,y,d/2-r,w/2+r,y,d/2+r,Block::BlueGlass);
        for(int x : {w/2-r,w/2+r}) { put(x,y,d/2-r,Block::Concrete); put(x,y,d/2+r,Block::Concrete); }
      }
      put(w/2,roof+19,d/2,Block::Lamp);
    }
    for(int x : {-1,w}) { put(x,0,d+1,Block::Planter); put(x,1,d+1,Block::Lamp); }
  }
};
struct Approach { glm::ivec2 from,to; };
Approach approach(const HarborBuilding& b) {
  Terrain t(7262026,true); Cell o{512,coastSeaLevel,-192};
  auto door=harborPosition({},b,{12.5f,0,float(b.depth)+2.5f});
  glm::ivec2 from{int(door.x),int(door.z)},best=from; float nearest=std::numeric_limits<float>::max();
  for(int z=from.y-48;z<=from.y+48;++z) for(int x=from.x-48;x<=from.x+48;++x) {
    auto c=coastColumn(t,o,o.x+x,o.z+z); if(!c.road) continue;
    float distance=float((x-from.x)*(x-from.x)+(z-from.y)*(z-from.y));
    if(distance<nearest) { nearest=distance; best={x,z}; }
  }
  return {from,best};
}
const std::array<Approach,buildings.size()>& approaches() {
  static const auto result=[] { std::array<Approach,buildings.size()> a; for(std::size_t i=0;i<a.size();++i) a[i]=approach(buildings[i]); return a; }();
  return result;
}
float pathDistance(glm::vec2 p,Approach a) {
  glm::vec2 from(a.from),d=glm::vec2(a.to-a.from);
  float t=std::clamp(glm::dot(p-from,d)/std::max(1.f,glm::dot(d,d)),0.f,1.f);
  return glm::length(p-from-t*d);
}
}
std::span<const HarborBuilding> harborBuildings() { return buildings; }
glm::vec3 harborPosition(Cell o,const HarborBuilding& b,glm::vec3 p) {
  float x=p.x,z=p.z;
  if(b.turn==1) { x=b.depth-p.z; z=p.x; }
  if(b.turn==2) { x=b.width-p.x; z=b.depth-p.z; }
  if(b.turn==3) { x=p.z; z=b.width-p.x; }
  return {o.x+b.x+x,harborGround+p.y,o.z+b.z+z};
}
bool harborGraded(Cell o,std::uint32_t lots,int wx,int wz) {
  int x=wx-o.x,z=wz-o.z;
  for(std::size_t i=0;i<buildings.size();++i) if(lots&(1u<<i))
    if(contains(parcel(buildings[i]),x,z) || pathDistance({x+.5f,z+.5f},approaches()[i])<=3) return true;
  return false;
}
void generateHarbor(Chunk& chunk,Cell o,std::uint32_t lots) {
  int bx=chunk.pos.x*chunkSize-o.x,bz=chunk.pos.z*chunkSize-o.z;
  if(bx+chunkSize<0 || bz+chunkSize<0 || bx>=coastSize || bz>=coastSize) return;
  // Short paved approaches connect doors to the existing curved road. They do
  // not erase the coast or turn the whole region into a rectangular platform.
  for(std::size_t i=0;i<buildings.size();++i) if(lots&(1u<<i)) {
    Builder builder{chunk,o,buildings[i]}; auto a=approaches()[i];
    for(int z=bz;z<bz+chunkSize;++z) for(int x=bx;x<bx+chunkSize;++x) {
      float distance=pathDistance({x+.5f,z+.5f},a);
      if(distance>3) continue;
      builder.worldFill(x,1,z,x,21,z,Block::Stone);
      builder.worldFill(x,22,z,x,22,z,distance>2.1 ? Block::Terracotta : Block::Limestone);
      builder.worldFill(x,23,z,x,worldHeight-1,z,Block::Air);
    }
  }
  for(std::size_t i=0;i<buildings.size();++i) if(lots&(1u<<i)) {
    auto r=parcel(buildings[i]);
    if(r.x0<bx+chunkSize && r.x1>=bx && r.z0<bz+chunkSize && r.z1>=bz) Builder{chunk,o,buildings[i]}.build();
  }
}
bool initializeHarbor(World& w,const Player& p) {
  if(w.harborLots) return *w.harborLots!=0;
  if(!initializeCoast(w,p)) return false;
  auto o=*w.coastOrigin; std::uint32_t mask=0;
  for(std::size_t i=0;i<buildings.size();++i) {
    auto r=parcel(buildings[i]); auto a=approaches()[i];
    r={std::min({r.x0,a.from.x-3,a.to.x-3}),std::min({r.z0,a.from.y-3,a.to.y-3}),
       std::max({r.x1,a.from.x+3,a.to.x+3}),std::max({r.z1,a.from.y+3,a.to.y+3})};
    auto occupied=[&](glm::vec3 v){return contains(r,int(std::floor(v.x))-o.x,int(std::floor(v.z))-o.z);};
    if(w.editedIn({o.x+r.x0,1,o.z+r.z0},{o.x+r.x1,worldHeight-1,o.z+r.z1}) || occupied(p.pose.position)) continue;
    if(w.farm.car.owned && occupied(w.farm.car.position)) continue;
    if(std::ranges::any_of(w.farm.chickens,[&](auto& v){return occupied(v.position);})
      || std::ranges::any_of(w.farm.livestock,[&](auto& v){return occupied(v.position)||occupied(v.home);})) continue;
    mask|=1u<<i;
  }
  w.harborLots=mask&allLots;
  std::vector<ChunkPos> loaded;
  for(auto& [c,chunk]:w.chunks) if(coastContains(o,float(c.x*chunkSize),float(c.z*chunkSize))) loaded.push_back(c);
  for(auto c:loaded) w.insert(w.terrain.generate(c));
  return mask!=0;
}
bool visitHarbor(World& w,Player& p,bool penthouse) {
  if(!initializeHarbor(w,p)) return false;
  auto o=*w.coastOrigin;
  for(std::size_t i=0;i<buildings.size();++i) if(*w.harborLots&(1u<<i)) {
    const auto& b=buildings[i]; auto a=approaches()[i];
    auto at=penthouse ? harborPosition(o,b,{b.style==0 ? 15.5f : 7.5f,float(b.floors*harborFloorHeight),b.style==0 ? 35.5f : 13.5f})
                     : glm::vec3(o.x+a.to.x+.5f,float(harborGround),o.z+a.to.y+.5f);
    w.ensure(chunkAt(int(at.x),int(at.z)),3);
    Box box{at+glm::vec3(-.3f,0,-.3f),at+glm::vec3(.3f,1.8f,.3f)};
    if(p.collides(w,at) || chickensOverlap(w,box) || ranchOverlap(w,box)) continue;
    auto target=penthouse ? harborPosition(o,b,{b.style==0 ? 35.f : 25.f,float(b.floors*harborFloorHeight)+3,float(b.depth)-7})
                         : harborPosition(o,b,{b.width*.5f,12,float(b.depth)});
    auto direction=glm::normalize(target-at-glm::vec3(0,1.6f,0));
    p.pose.position=at; p.stopFlying(); p.pose.yaw=std::atan2(direction.x,-direction.z); p.pose.pitch=std::asin(direction.y);
    p.grounded=false; p.sneaking=false; return true;
  }
  return false;
}
} // namespace bw

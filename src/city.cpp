#include "city.hpp"
#include "farm.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr std::array buildings{
  CityBuilding{6,4,23,20,6,Block::Brick,Block::Limestone},
  CityBuilding{44,4,23,20,8,Block::Concrete,Block::Charcoal},
  CityBuilding{73,4,21,20,5,Block::Sage,Block::Limestone},
  CityBuilding{100,4,23,20,7,Block::Limestone,Block::Concrete},
  CityBuilding{6,42,23,24,7,Block::Terracotta,Block::Concrete},
  CityBuilding{100,42,23,24,8,Block::Concrete,Block::Sage},
  CityBuilding{6,74,23,16,3,Block::Limestone,Block::Brick},
  CityBuilding{100,74,23,16,5,Block::Brick,Block::Concrete},
  CityBuilding{6,101,23,23,5,Block::Concrete,Block::Terracotta},
  CityBuilding{44,101,23,23,7,Block::Terracotta,Block::Limestone},
  CityBuilding{73,101,21,23,4,Block::Sage,Block::Concrete},
  CityBuilding{100,101,23,23,6,Block::Limestone,Block::Charcoal}
};
// Every fill clips before iterating. A streamed chunk only constructs its own
// 16x16 columns, regardless of how many buildings are in the district.
class Builder {
public:
  Chunk& chunk; Cell origin;
  void fill(int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
    int bx=chunk.pos.x*chunkSize-origin.x,bz=chunk.pos.z*chunkSize-origin.z;
    x0=std::max(x0,bx); x1=std::min(x1,bx+chunkSize-1);
    z0=std::max(z0,bz); z1=std::min(z1,bz+chunkSize-1);
    y0=std::max(y0,1-origin.y); y1=std::min(y1,worldHeight-1-origin.y);
    for(int z=z0;z<=z1;++z) for(int x=x0;x<=x1;++x) for(int y=y0;y<=y1;++y)
      chunk.set(x-bx,y+origin.y,z-bz,block);
  }
  void put(int x,int y,int z,Block block) { fill(x,y,z,x,y,z,block); }
  void tree(int x,int z) {
    fill(x-2,-1,z-2,x+2,-1,z+2,Block::Grass);
    fill(x-2,3,z-2,x+2,5,z+2,Block::Leaves);
    fill(x-1,6,z-1,x+1,6,z+1,Block::Leaves);
    fill(x,0,z,x,4,z,Block::Wood);
  }
  void lamp(int x,int z) {
    fill(x,0,z,x,2,z,Block::Charcoal); put(x,3,z,Block::Lamp);
    put(x,4,z,Block::StoneSlab);
  }
  void bench(int x,int z) {
    fill(x,0,z,x+2,0,z,Block::StoneSlab);
    fill(x,0,z+1,x+2,0,z+1,Block::Wood);
  }
  void building(const CityBuilding& b) {
    const int x=b.x,z=b.z,w=b.width,d=b.depth,roof=b.floors*cityFloorHeight;
    auto f=[&](int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
      fill(x+x0,y0,z+z0,x+x1,y1,z+z1,block);
    };
    auto p=[&](int px,int py,int pz,Block block) { f(px,py,pz,px,py,pz,block); };
    f(-1,-1,-1,w,-1,d+2,Block::Concrete);
    f(0,0,0,w-1,roof-1,0,b.wall); f(0,0,d-1,w-1,roof-1,d-1,b.wall);
    f(0,0,0,0,roof-1,d-1,b.wall); f(w-1,0,0,w-1,roof-1,d-1,b.wall);
    for(int corner : {0,w-1}) f(corner,0,0,corner,roof-1,d-1,b.trim);
    for(int floor=0;floor<=b.floors;++floor) {
      int y=floor*cityFloorHeight;
      f(0,y-1,0,w-1,y-1,d-1,floor==b.floors ? Block::Concrete : Block::Planks);
      // Continuous horizontal cornices and corner columns articulate each tower.
      f(-1,y-1,-1,w,y-1,0,b.trim); f(-1,y-1,d-1,w,y-1,d,b.trim);
      f(-1,y-1,0,0,y-1,d-1,b.trim); f(w-1,y-1,0,w,y-1,d-1,b.trim);
      if(floor>0) f(2,y-1,5,8,y-1,10,Block::Air);
      if(floor==b.floors) break;
      for(int wx=2;wx<w-2;wx+=4) {
        f(wx,y+1,0,std::min(wx+2,w-2),y+2,0,Block::Glass);
        f(wx,y+1,d-1,std::min(wx+2,w-2),y+2,d-1,Block::Glass);
      }
      for(int wz=2;wz<d-2;wz+=4) {
        f(0,y+1,wz,0,y+2,std::min(wz+2,d-2),Block::Glass);
        f(w-1,y+1,wz,w-1,y+2,std::min(wz+2,d-2),Block::Glass);
      }
      // The landing stays open across the south end of the stairwell.
      f(9,y,4,9,y+2,10,b.trim);
      p(9,y+2,11,Block::Lamp);
      if(floor>0) {
        f(2,y-1,d, w-3,y-1,d+1,b.trim);
        f(2,y,d+1,w-3,y,d+1,Block::Glass);
        p(2,y,d,Block::Glass); p(w-3,y,d,Block::Glass);
        f(11,y,d-1,12,y+2,d-1,Block::Air);
        p(11,y,d-1,Block::DoorZ); p(11,y+1,d-1,Block::DoorZTop);
      } else {
        f(11,0,d-1,13,2,d-1,Block::Air);
        f(10,3,d,14,3,d+2,Block::Glass);
        p(10,2,d-1,Block::Lamp); p(14,2,d-1,Block::Lamp);
      }
      // Apartments: seating by the windows, a kitchen and a working bed.
      f(w-5,y,3,w-3,y,5,Block::StoneSlab);
      f(w-2,y,3,w-2,y+1,5,b.wall);
      f(12,y,2,14,y,2,Block::Concrete); p(12,y+1,2,Block::Lamp);
      p(13,y,5,Block::Planks); p(14,y,5,Block::Planks);
      if(floor>0) {
        p(w-4,y,d-5,Block::BedZ); p(w-4,y,d-6,Block::BedZHead);
      }
    }
    // Paired half-block flights: north, across the landing, south, repeat.
    // All floors, including the roof, are reachable without jumping or flying.
    for(int floor=0;floor<b.floors;++floor) {
      int y=floor*cityFloorHeight;
      for(int step=0;step<4;++step) {
        int whole=(step+1)/2;
        if(step%2==0) {
          f(2,y+whole,10-step,4,y+whole,10-step,Block::StoneSlab);
          f(6,y+2+whole,7+step,8,y+2+whole,7+step,Block::StoneSlab);
        } else {
          f(2,y+whole-1,10-step,4,y+whole-1,10-step,b.trim);
          f(6,y+2+whole-1,7+step,8,y+2+whole-1,7+step,b.trim);
        }
      }
      f(2,y+1,4,8,y+1,6,b.trim);
      f(5,y,7,5,y+3,10,b.trim); // Divider stops accidental falls between flights.
    }
    // Glass rooftop rails, a penthouse, a shallow pool, and planted terraces.
    f(0,roof,0,w-1,roof,0,Block::Glass); f(0,roof,d-1,w-1,roof,d-1,Block::Glass);
    f(0,roof,0,0,roof,d-1,Block::Glass); f(w-1,roof,0,w-1,roof,d-1,Block::Glass);
    f(1,roof,4,1,roof,11,Block::Glass); f(9,roof,4,9,roof,10,Block::Glass);
    f(2,roof,4,8,roof,4,Block::Glass);
    f(11,roof-2,2,w-3,roof-2,6,Block::Concrete);
    f(11,roof-1,2,w-3,roof-1,6,Block::Water);
    f(11,roof-1,6,w-3,roof-1,6,Block::StoneSlab); // Walk-out pool steps.
    f(10,roof,9,w-2,roof+2,9,b.trim); f(10,roof,d-2,w-2,roof+2,d-2,b.trim);
    f(10,roof,9,10,roof+2,d-2,b.trim); f(w-2,roof,9,w-2,roof+2,d-2,b.trim);
    f(12,roof+1,9,w-3,roof+2,9,Block::Glass);
    f(w-2,roof+1,10,w-2,roof+2,d-3,Block::Glass);
    f(9,roof+3,8,w-1,roof+3,d-1,Block::Concrete);
    f(10,roof,11,10,roof+2,12,Block::Air);
    p(10,roof,12,Block::DoorX); p(10,roof+1,12,Block::DoorXTop);
    p(w-4,roof,d-4,Block::BedZ); p(w-4,roof,d-5,Block::BedZHead);
    f(12,roof,10,14,roof,10,Block::StoneSlab);
    f(11,roof,d-4,13,roof,d-3,Block::Concrete);
    p(11,roof+1,d-3,Block::Charcoal);
    if(d>=20) {
      f(14,roof,14,15,roof,15,Block::Planks);
      p(13,roof,15,Block::StoneSlab);
      p(w-3,roof,11,Block::Terracotta); p(w-3,roof+1,11,Block::Leaves);
    }
    p(12,roof+2,10,Block::Lamp); p(w-3,roof+2,d-3,Block::Lamp);
    f(2,roof,1,7,roof,2,Block::Terracotta); f(2,roof+1,1,7,roof+1,2,Block::Leaves);
    f(2,roof,d-3,6,roof,d-2,Block::Grass); f(2,roof+1,d-3,6,roof+1,d-2,Block::Leaves);
    p(8,roof,d-3,Block::Lamp);
  }
};
bool contains(Cell o,glm::vec3 p) {
  return p.x>=o.x && p.x<o.x+citySize && p.z>=o.z && p.z<o.z+citySize;
}
}
std::span<const CityBuilding> cityBuildings() { return buildings; }
void generateCity(Chunk& chunk,Cell origin) {
  int bx=chunk.pos.x*chunkSize,bz=chunk.pos.z*chunkSize;
  if(bx+chunkSize<=origin.x || bx>=origin.x+citySize || bz+chunkSize<=origin.z || bz>=origin.z+citySize) return;
  Builder b{chunk,origin};
  b.fill(0,1-origin.y,0,127,-3,127,Block::Stone);
  b.fill(0,-2,0,127,-1,127,Block::Grass);
  b.fill(0,0,0,127,worldHeight-1-origin.y,127,Block::Air);
  // Clear, eight-block streets form a driving loop around the lake.
  for(int z : {25,90}) {
    b.fill(0,-1,z-2,127,-1,z+9,Block::Concrete);
    b.fill(0,-1,z,127,-1,z+7,Block::Asphalt);
  }
  b.fill(31,-1,0,42,-1,127,Block::Concrete); b.fill(33,-1,0,40,-1,127,Block::Asphalt);
  b.fill(85,-1,25,96,-1,97,Block::Concrete); b.fill(87,-1,25,94,-1,97,Block::Asphalt);
  for(int n=2;n<126;n+=6) {
    for(int z : {28,93}) if(n<29 || (n>42 && n<84) || n>96) b.fill(n,-1,z,n+2,-1,z,Block::Limestone);
    if(n<22 || (n>35 && n<87) || n>100) b.fill(36,-1,n,36,-1,n+2,Block::Limestone);
    if(n>35 && n<87) b.fill(90,-1,n,90,-1,n+2,Block::Limestone);
  }
  for(int x : {33,87}) for(int z : {25,90}) for(int stripe=0;stripe<8;stripe+=2) {
    b.fill(x+stripe,-1,z+9,x+stripe,-1,z+11,Block::Concrete);
    b.fill(x-4,-1,z+stripe,x-2,-1,z+stripe,Block::Concrete);
  }
  // Brick waterfront promenade and a shallow, wadeable lake with step-out edges.
  b.fill(43,-1,35,84,-1,86,Block::Brick);
  b.fill(46,-3,39,81,-3,82,Block::Sand);
  b.fill(46,-2,39,81,-1,82,Block::Water);
  b.fill(46,-2,39,81,-2,39,Block::StoneSlab); b.fill(46,-2,82,81,-2,82,Block::StoneSlab);
  b.fill(46,-1,39,81,-1,39,Block::StoneSlab); b.fill(46,-1,82,81,-1,82,Block::StoneSlab);
  b.fill(46,-2,40,46,-2,81,Block::Concrete); b.fill(81,-2,40,81,-2,81,Block::Concrete);
  b.fill(46,-1,40,46,-1,81,Block::StoneSlab); b.fill(81,-1,40,81,-1,81,Block::StoneSlab);
  // An island garden and a bridge make the waterfront a place to explore.
  b.fill(59,-2,55,69,-1,66,Block::Grass);
  b.fill(43,-1,59,84,-1,62,Block::Planks);
  for(int x=47;x<81;++x) if(x<59 || x>69) { b.put(x,0,58,Block::Fence); b.put(x,0,63,Block::Fence); }
  for(int x : {60,68}) for(int z : {56,65}) b.fill(x,0,z,x,3,z,Block::Wood);
  for(int x=59;x<=69;x+=2) b.fill(x,4,55,x,4,66,Block::Planks);
  b.bench(62,56); b.bench(62,64); b.lamp(67,64);
  for(int z : {38,50,74,85}) for(int x : {44,83}) b.lamp(x,z);
  for(int z : {43,70,84}) { b.tree(3,z); b.tree(125,z); }
  for(int x : {49,77}) { b.tree(x,36); b.tree(x,85); }
  for(int z : {47,70}) { b.bench(43,z); b.bench(82,z); }
  for(int z : {16,40,70,106,123}) b.lamp(31,z);
  for(int x : {15,54,76,111}) { b.lamp(x,33); b.lamp(x,88); }
  for(const auto& building : buildings) b.building(building);
}
bool initializeCity(World& world,const Player& player) {
  if(world.cityOrigin) return true;
  constexpr std::array sites{Cell{160,16,-96},Cell{-320,16,-96},Cell{160,16,160},Cell{-320,16,160}};
  for(auto o : sites) {
    if(contains(o,player.pose.position) || world.editedIn(o+Cell{0,1-o.y,0},o+Cell{127,worldHeight-1-o.y,127})) continue;
    if(world.farm.car.owned && contains(o,world.farm.car.position)) continue;
    if(std::ranges::any_of(world.farm.chickens,[&](const auto& a){return contains(o,a.position);})
        || std::ranges::any_of(world.farm.livestock,[&](const auto& a){return contains(o,a.position) || contains(o,a.home);})) continue;
    world.cityOrigin=o;
    // Chunks already visited also receive the city; insert reapplies user edits.
    std::vector<ChunkPos> loaded;
    for(const auto& [p,chunk] : world.chunks)
      if(p.x*chunkSize>=o.x && p.x*chunkSize<o.x+citySize && p.z*chunkSize>=o.z && p.z*chunkSize<o.z+citySize) loaded.push_back(p);
    for(auto p : loaded) world.insert(world.terrain.generate(p));
    return true;
  }
  return false;
}
bool visitCity(World& world,Player& player) {
  if(!initializeCity(world,player)) return false;
  auto o=*world.cityOrigin;
  world.ensure(chunkAt(o.x+64,o.z+64),5);
  // Arrive facing along a boulevard, so C delivers a car with a clear road ahead.
  for(int z : {86,85,87,84,88,83}) for(int x : {38,37,39,36,35}) {
    glm::vec3 at{o.x+x+.5f,float(o.y),o.z+z+.5f};
    if(!opaque(world.get(o+Cell{x,-1,z})) || player.collides(world,at)
        || chickensOverlap(world,{at+glm::vec3(-.3f,0,-.3f),at+glm::vec3(.3f,1.8f,.3f)})
        || ranchOverlap(world,{at+glm::vec3(-.3f,0,-.3f),at+glm::vec3(.3f,1.8f,.3f)})) continue;
    player.pose.position=at; player.pose.yaw=0; player.pose.pitch=.1f;
    player.stopFlying(); player.grounded=false; player.sneaking=false; return true;
  }
  return false;
}
bool atCity(const World& world,const Player& player) { return world.cityOrigin && contains(*world.cityOrigin,player.pose.position); }
} // namespace bw

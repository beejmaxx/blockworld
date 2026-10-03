#include "castle.hpp"
#include "farm.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
bool inParcel(glm::vec3 p,Cell o) {
  return p.x>=o.x-5 && p.x<=o.x+35 && p.z>=o.z-5 && p.z<=o.z+43;
}
bool untouched(const World& world,const Player& player,Cell o) {
  if(inParcel(player.pose.position,o)) return false;
  for(const auto& c : world.farm.chickens) if(inParcel(c.position,o)) return false;
  for(const auto& a : world.farm.livestock) if(inParcel(a.position,o)) return false;
  if(world.farm.car.owned && inParcel(world.farm.car.position,o)) return false;
  for(int x=o.x-4;x<=o.x+34;++x) for(int z=o.z-4;z<=o.z+42;++z)
    for(int y=1;y<worldHeight;++y) if(world.edited({x,y,z})) return false;
  return true;
}
void construct(World& world,Cell o) {
  auto put=[&](int x,int y,int z,Block b=Block::Stone) { world.set(o+Cell{x,y,z},b); };
  auto fill=[&](int x0,int y0,int z0,int x1,int y1,int z1,Block b=Block::Stone) {
    for(int z=z0;z<=z1;++z) for(int x=x0;x<=x1;++x) for(int y=y0;y<=y1;++y) put(x,y,z,b);
  };
  // A level terrace provides a large courtyard and a car-sized forecourt.
  for(int z=-4;z<=42;++z) for(int x=-4;x<=34;++x) {
    int ground=world.terrain.height(o.x+x,o.z+z);
    for(int y=ground+1;y<o.y;++y) world.set({o.x+x,y,o.z+z},Block::Stone);
    put(x,-1,z,Block::Grass);
    for(int y=o.y;y<worldHeight;++y) world.set({o.x+x,y,o.z+z},Block::Air);
  }
  fill(0,-1,0,30,-1,30);
  fill(11,-1,29,19,-1,41,Block::Brick);
  fill(12,-1,14,18,-1,28,Block::Brick);
  // Wide curtain walls, with a drive-through arched entrance on the south side.
  fill(2,0,2,4,5,28); fill(26,0,2,28,5,28);
  fill(2,0,2,28,5,4); fill(2,0,26,28,5,28);
  fill(13,0,26,17,3,28,Block::Air);
  for(int x : {13,17}) fill(x,3,26,x,3,28);
  for(int n=7;n<=23;++n) {
    for(int y=6;y<=6+(n%3==0);++y) {
      put(2,y,n); put(28,y,n); put(n,y,2); put(n,y,28);
    }
  }
  // Four hollow towers; their doorways connect the wall walks at level six.
  for(int tx : {0,24}) for(int tz : {0,24}) {
    for(int z=0;z<=6;++z) for(int x=0;x<=6;++x) {
      bool edge=x==0 || x==6 || z==0 || z==6;
      for(int y=0;y<=8;++y) put(tx+x,y,tz+z,edge ? Block::Stone : Block::Air);
      put(tx+x,5,tz+z); put(tx+x,9,tz+z);
      if(edge) {
        put(tx+x,10,tz+z);
        if((x%2==0 && (z==0 || z==6)) || (z%2==0 && (x==0 || x==6))) put(tx+x,11,tz+z);
      }
    }
    int inwardX=tx==0 ? tx+6 : tx;
    int inwardZ=tz==0 ? tz+6 : tz;
    fill(inwardX,6,tz+2,inwardX,8,tz+4,Block::Air);
    fill(tx+2,6,inwardZ,tx+4,8,inwardZ,Block::Air);
    // Arrow slit windows, warm lamps, and brick pennants above the roof.
    for(int y : {2,3}) { put(tx+3,y,tz,Block::Glass); put(tx+3,y,tz+6,Block::Glass); }
    put(tx+1,6,tz+1,Block::Torch);
    fill(tx+1,10,tz+1,tx+1,14,tz+1,Block::Wood);
    fill(tx+2,13,tz+1,tx+3,14,tz+1,Block::Brick);
  }
  // Twelve shallow steps climb from the courtyard to the wall walk.
  for(int step=0;step<12;++step) {
    int z=23-step,whole=(step+1)/2;
    fill(7,0,z,9,whole-1,z);
    if(step%2==0) fill(7,whole,z,9,whole,z,Block::StoneSlab);
    for(int x : {6,10}) { put(x,whole-1,z); put(x,whole,z,Block::Fence); }
  }
  fill(4,5,10,9,5,11);
  for(int x=5;x<=10;++x) put(x,6,9,Block::Fence);
  // Eight more steps on the north wall reach the northeast tower roof.
  for(int step=0;step<8;++step) {
    int x=15+step,whole=6+(step+1)/2;
    fill(x,6,3,x,whole-1,4);
    if(step%2==0) fill(x,whole,3,x,whole,4,Block::StoneSlab);
    put(x,whole-1,5); put(x,whole,5,Block::Fence);
  }
  fill(23,9,3,25,9,4);
  fill(24,10,3,24,11,4,Block::Air);
  // A furnished hall inside the courtyard, with an ordinary usable bed.
  fill(13,-1,7,22,-1,14,Block::Planks);
  fill(13,0,7,22,3,7); fill(13,0,14,22,3,14);
  fill(13,0,7,13,3,14); fill(22,0,7,22,3,14);
  fill(12,4,6,23,4,15,Block::Planks);
  fill(17,0,14,17,1,14,Block::Air);
  put(17,0,14,Block::DoorZ); put(17,1,14,Block::DoorZTop);
  for(int z : {9,12}) for(int x : {13,22}) fill(x,1,z,x,2,z,Block::Glass);
  put(20,0,10,Block::BedZ); put(20,0,9,Block::BedZHead);
  fill(16,0,9,16,0,11,Block::Planks);
  for(Cell c : {Cell{14,0,8},Cell{21,0,8},Cell{8,0,25},Cell{22,0,25},Cell{12,0,30},Cell{18,0,30},Cell{8,6,10},Cell{25,10,2}})
    put(c.x,c.y,c.z,Block::Torch);
}
}
bool initializeCastle(World& world,const Player& player) {
  if(world.castleOrigin) return true;
  // Try another parcel if a player has already built, dug, or planted at a site.
  constexpr std::array sites{Cell{-48,0,28},Cell{-80,0,-36},Cell{48,0,28},Cell{48,0,-68},Cell{-112,0,28},Cell{32,0,96}};
  for(auto o : sites) {
    if(!untouched(world,player,o)) continue;
    // Match the road's outer edge to the surrounding ground, so the car can
    // leave the forecourt instead of being stranded on a raised terrace.
    o.y=world.terrain.height(o.x+15,o.z+43)+1;
    if(o.y>worldHeight-18) continue;
    world.ensure(chunkAt(o.x+15,o.z+19),2);
    construct(world,o); world.castleOrigin=o; return true;
  }
  return false;
}
bool visitCastle(World& world,Player& player) {
  if(!initializeCastle(world,player)) return false;
  auto o=*world.castleOrigin;
  world.ensure(chunkAt(o.x+15,o.z+19),3);
  for(int z : {35,36,37,38,39,32}) for(int x : {15,14,16,12,18,10,20}) {
    glm::vec3 at{o.x+x+.5f,float(o.y),o.z+z+.5f};
    if(!opaque(world.get(o+Cell{x,-1,z})) || player.collides(world,at)
        || chickensOverlap(world,{at+glm::vec3(-.3f,0,-.3f),at+glm::vec3(.3f,1.8f,.3f)})) continue;
    player.pose.position=at; player.pose.yaw=0; player.pose.pitch=.06f;
    player.stopFlying(); player.grounded=false; player.sneaking=false; return true;
  }
  return false;
}
bool atCastle(const World& world,const Player& player) {
  return world.castleOrigin && inParcel(player.pose.position,*world.castleOrigin);
}
} // namespace bw

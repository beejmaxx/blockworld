#include "castle.hpp"
#include "adventure.hpp"
#include "ranch.hpp"
#include "inventory.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void walk(World& world,Player& player,glm::vec2 to) {
  for(int frame=0;frame<1200;++frame) {
    auto delta=to-glm::vec2(player.pose.position.x,player.pose.position.z);
    if(glm::length(delta)<.08f) return;
    player.pose.yaw=std::atan2(delta.x,-delta.y);
    Movement move; move.forward=1;
    player.tick(world,move,1.f/60.f);
    check(!player.collides(world,player.pose.position),"walking cannot enter castle masonry");
  }
  std::cerr<<"Stopped at "<<player.pose.position.x<<','<<player.pose.position.y<<','<<player.pose.position.z
    <<" toward "<<to.x<<','<<to.y<<'\n';
  throw std::runtime_error("castle route must be walkable without jumping or flying");
}
World flat() {
  World world;
  for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) world.insert(Chunk{{x,z}});
  for(int z=-12;z<=12;++z) for(int x=-4;x<=4;++x) world.set({x,1,z},Block::Stone);
  return world;
}
void stairs() {
  auto w=flat();
  for(int step=0;step<8;++step) {
    int whole=(step+1)/2,z=-1-step;
    for(int x=-1;x<=1;++x) {
      for(int y=2;y<2+whole;++y) w.set({x,y,z},Block::Stone);
      if(step%2==0) w.set({x,2+whole,z},Block::StoneSlab);
    }
  }
  for(int z=-10;z<=-9;++z) for(int x=-1;x<=1;++x) w.set({x,5,z},Block::Stone);
  Player p; p.pose.position={.5f,2,1.5f};
  walk(w,p,{.5f,-9.5f});
  check(std::abs(p.pose.position.y-6)<.01f,"alternating slabs and blocks form walkable half-height stairs");
  walk(w,p,{.5f,1.5f});
  for(int i=0;i<30;++i) p.tick(w,{},1.f/60.f);
  check(std::abs(p.pose.position.y-2)<.01f,"descending the stairs returns to floor height");
  w=flat(); w.set({0,2,0},Block::StoneSlab); w.set({0,4,0},Block::Stone);
  p={}; p.pose.position={.5f,2,2}; p.pose.yaw=0;
  Movement move; move.forward=1;
  for(int i=0;i<60;++i) p.tick(w,move,1.f/60.f);
  check(p.pose.position.z>=1.299f && p.pose.position.y<2.01f,"low ceilings block step-up without clipping");
  w.set({0,4,0},Block::Air); w.set({0,2,0},Block::Stone);
  for(int i=0;i<60;++i) p.tick(w,move,1.f/60.f);
  check(p.pose.position.z>=1.299f && p.pose.position.y<2.01f,"a full block still needs a jump");
  w.set({0,2,0},Block::StoneSlab); p.pose.flying=true;
  for(int i=0;i<60;++i) p.tick(w,move,1.f/60.f);
  check(p.pose.position.z>=1.299f && p.pose.position.y<2.01f,"flight does not trigger automatic step-up");
  check(!w.raycast({.5f,2.75f,2},{0,0,-1},2),"rays pass through air above a stone slab");
  auto hit=w.raycast({.5f,4,.5f},{0,-1,0},2);
  check(hit && std::abs(hit->distance-1.5f)<.001f,"ray picking follows the actual half-height surface");
  auto mesh=buildMesh(w,w.chunks.at({0,0}));
  for(const auto& v : mesh) if(v.block==glm::vec3(0,2,0))
    check(v.position.y<=2.5f && v.material==3,"stone steps render at their collision height with stone material");
}
void castle() {
  World w(7262026,true); w.ensure({0,0},1);
  Player p; p.pose.position={10.5f,24,7.5f};
  w.set({10,45,10},Block::Glass);
  check(initializeCastle(w,p) && w.castleOrigin,"castle is built in an untouched parcel");
  auto o=*w.castleOrigin;
  check(w.get({10,45,10})==Block::Glass,"existing player builds are preserved");
  check(visitCastle(w,p) && !p.collides(w,p.pose.position),"castle shortcut lands safely in the forecourt");
  walk(w,p,{o.x+15.5f,o.z+24.5f});
  walk(w,p,{o.x+8.5f,o.z+24.5f});
  walk(w,p,{o.x+8.5f,o.z+10.5f});
  check(std::abs(p.pose.position.y-o.y-6)<.01f,"courtyard stairs reach the wall walk");
  walk(w,p,{o.x+3.5f,o.z+10.5f});
  walk(w,p,{o.x+3.5f,o.z+3.5f});
  walk(w,p,{o.x+26.5f,o.z+3.5f});
  check(std::abs(p.pose.position.y-o.y-10)<.01f,"wall stairs reach the northeast tower roof");
  // Walk the same connected route back down.
  walk(w,p,{o.x+3.5f,o.z+3.5f}); walk(w,p,{o.x+3.5f,o.z+10.5f});
  walk(w,p,{o.x+8.5f,o.z+10.5f}); walk(w,p,{o.x+8.5f,o.z+24.5f});
  for(int i=0;i<30;++i) p.tick(w,{},1.f/60.f);
  check(p.pose.position.y<o.y+.1f,"both castle staircases can be descended");
  auto removed=o+Cell{2,11,0}; check(breakBlock(w,removed),"castle blocks can be removed normally"); auto edits=w.editCount();
  check(initializeCastle(w,p) && w.get(removed)==Block::Air && w.editCount()==edits,"revisiting never rebuilds removed castle blocks");
  p.pose.position={o.x+19.5f,float(o.y),o.z+10.5f}; w.clock.phase=.9;
  check(bedSleepStatus(w,p,o+Cell{20,0,10})==SleepResult::Ready,"castle hall contains a reachable working bed");
  check(visitCastle(w,p) && bringCar(w,p).empty(),"a car can be delivered at the castle");
  auto car=w.farm.car.position;
  auto d=glm::normalize(car+glm::vec3(0,.8f,0)-p.eye()); p.pose.yaw=std::atan2(d.x,-d.z); p.pose.pitch=std::asin(d.y);
  auto target=targetRanch(w,p); RideState ride;
  check(target && target->car && mountRanch(w,p,ride,*target),"castle car can be entered without menu or tool changes");
  Movement movement; movement.forward=1;
  for(int i=0;i<90;++i) tickRanch(w,p,ride,movement,1.f/60.f);
  check(w.farm.car.position.z<o.z+25 && w.farm.car.position.y==o.y,"car can drive through the castle arch into the courtyard");
  check(leaveRide(w,p,ride),"driver can get out safely in the courtyard");
  check(w.get(o+Cell{15,-1,41})==Block::Brick
    && w.terrain.height(o.x+15,o.z+43)==o.y-1,"castle road meets the surrounding terrain");

  auto path=std::filesystem::temp_directory_path()/("blockworld-castle-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup { std::filesystem::path p; ~Cleanup(){ std::error_code e;std::filesystem::remove(p,e); } } cleanup{path};
  equipItem(w.inventory,w.crafting,Item::StoneSlab);
  w.save(path,p.pose); World loaded; auto pose=loaded.load(path); loaded.ensure(chunkAt(o.x+15,o.z+15),2);
  check(pose && loaded.castleOrigin==w.castleOrigin && loaded.get(removed)==Block::Air
    && loaded.get(o+Cell{8,0,23})==Block::StoneSlab && loaded.inventory.held()==Item::StoneSlab
    && loaded.farm.car.position==w.farm.car.position,"castle, custom edits, steps, selection, and parked car survive reload");
  initializeCastlePets(w); auto animals=w.farm.livestock.size(); auto coins=w.farm.garden.coins;
  check(farmView(w).sheep==1 && farmView(w).foxes==1,"castle receives a sheep and fox");
  initializeCastlePets(w);
  check(w.farm.livestock.size()==animals && w.farm.garden.coins==coins,"castle companions are free and are not duplicated on revisiting");
}
void protectedSite() {
  World w(7262026,true); Player p; p.pose.position={10.5f,24,7.5f};
  Cell occupied{-28,50,48}; w.ensure(chunkAt(occupied.x,occupied.z),0); w.set(occupied,Block::Glass);
  check(initializeCastle(w,p) && w.castleOrigin->x!=-48 && w.get(occupied)==Block::Glass,"an edited castle site is skipped without changing it");
}
}
int main() {
  try { stairs(); castle(); protectedSite(); std::cout<<"PASS castle construction, walkable stairs, tower route, car access, and persistence\n"; }
  catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

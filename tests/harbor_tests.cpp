#include "harbor.hpp"
#include "building.hpp"
#include "adventure.hpp"
#include "inventory.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void walk(World& w,Player& p,glm::vec3 to) {
  for(int i=0;i<3000;++i) {
    glm::vec2 delta(to.x-p.pose.position.x,to.z-p.pose.position.z);
    if(glm::length(delta)<.07f) return;
    p.pose.yaw=std::atan2(delta.x,-delta.y); Movement m; m.forward=1; p.tick(w,m,1.f/60.f);
    check(!p.collides(w,p.pose.position),"walks must not clip into walls or ceilings");
  }
  std::cerr<<"Stopped "<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z<<" to "<<to.x<<','<<to.y<<','<<to.z<<'\n';
  throw std::runtime_error("building route blocked");
}
void routes(World& w) {
  auto o=*w.coastOrigin;
  for(auto& b:harborBuildings()) {
    std::cout<<"Checking "<<b.name<<'\n';
    Player p; p.pose.position=harborPosition(o,b,{12.5f,0,b.depth+3.5f});
    w.ensure(chunkAt(int(p.pose.position.x),int(p.pose.position.z)),4);
    auto go=[&](float x,float z) { walk(w,p,harborPosition(o,b,{x,0,z})); };
    go(12.5f,12.5f); go(3.5f,12.5f);
    for(int level=0;level<b.floors;++level) {
      go(3.5f,5.5f); go(7.5f,5.5f); go(7.5f,12.5f);
      check(std::abs(p.pose.position.y-harborGround-(level+1)*harborFloorHeight)<.03,"stairs reach every floor");
      if(level+1<b.floors) go(3.5f,12.5f);
    }
    int roof=b.floors*harborFloorHeight;
    if(b.style==0) {
      go(7.5f,13.5f); go(13.5f,13.5f);
      check(std::abs(p.pose.position.y-harborGround-roof)<.03,"penthouse lounge has a clear entrance");
      go(12.5f,26.5f); go(9.5f,28.5f); go(9.5f,15.5f); go(12.5f,15.5f);
      check(std::abs(p.pose.position.y-harborGround-roof-6)<.03,"private upper terrace has walking stairs");
      go(12.5f,11.5f); go(15.5f,11.5f); go(15.5f,9.5f); go(29.5f,9.5f); go(29.5f,7.5f);
      auto bed=harborPosition(o,b,{30.5f,float(roof+6),7.5f});
      Cell c{int(bed.x),int(bed.y),int(bed.z)};
      w.clock.phase=.9;
      check(isBed(w.get(c)) && bedSleepStatus(w,p,c)==SleepResult::Ready,"duplex bedroom bed works after walking upstairs");
      go(29.5f,9.5f); go(15.5f,9.5f); go(15.5f,11.5f); go(12.5f,11.5f); go(12.5f,15.5f); go(9.5f,15.5f); go(9.5f,28.5f);
      go(15.5f,28.5f); go(15.5f,35.5f); go(25.5f,35.5f); go(25.5f,30.5f);
      check(p.pose.position.y<harborGround+roof,"rooftop pool supports swimming");
      go(25.5f,35.5f); go(15.5f,35.5f); go(15.5f,26.5f); go(12.5f,26.5f);
      check(std::abs(p.pose.position.y-harborGround-roof)<.03,"pool has a walking exit");
      go(12.5f,13.5f); go(7.5f,13.5f);
    }
    go(7.5f,12.5f);
    for(int level=b.floors;level>0;--level) {
      go(7.5f,5.5f); go(3.5f,5.5f); go(3.5f,12.5f);
      if(level>1) go(7.5f,12.5f);
    }
    go(12.5f,12.5f); go(12.5f,b.depth+3.5f);
    check(std::abs(p.pose.position.y-harborGround)<.03,"all buildings connect back to street level");
  }
}
void persistence() {
  World w(7262026,true); Player p; p.pose.position={10.5f,24,7.5f};
  check(initializeHarbor(w,p) && w.harborLots==1023,"all ten untouched building parcels initialize");
  check(w.editCount()==0,"buildings are procedural, not saved block edits");
  check(visitHarbor(w,p) && !p.collides(w,p.pose.position),"street shortcut arrives safely");
  check(visitHarbor(w,p,true) && !p.collides(w,p.pose.position),"penthouse shortcut arrives safely");
  routes(w);
  auto o=*w.coastOrigin; Cell added{o.x+220,110,o.z+350};
  w.ensure(chunkAt(added.x,added.z),0); check(w.set(added,Block::Sofa),"furniture can be placed above old height limit");
  auto path=std::filesystem::temp_directory_path()/("blockworld-harbor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}} cleanup{path};
  equipItem(w.inventory,w.crafting,Item::Planter); w.save(path,p.pose);
  World loaded; check(loaded.load(path).has_value() && loaded.harborLots==w.harborLots && loaded.inventory.held()==Item::Planter,"city parcels and furniture persist");
  loaded.ensure(chunkAt(added.x,added.z),0); check(loaded.get(added)==Block::Sofa,"high player edits override procedural buildings after reload");
  auto removed=Cell{o.x+205,24,o.z+335}; loaded.ensure(chunkAt(removed.x,removed.z),0); check(breakBlock(loaded,removed),"generated buildings are editable");
  loaded.evict({0,0},1); loaded.ensure(chunkAt(removed.x,removed.z),0); check(loaded.get(removed)==Block::Air,"streaming preserves demolition");
  World protectedWorld(7262026,true); p.pose.position={10.5f,24,7.5f};
  check(initializeCoast(protectedWorld,p),"protect existing coast fixture"); auto c=*protectedWorld.coastOrigin;
  Cell build{c.x+220,50,c.z+350}; protectedWorld.ensure(chunkAt(build.x,build.z),0); protectedWorld.set(build,Block::Brick);
  check(initializeHarbor(protectedWorld,p) && !(*protectedWorld.harborLots&1) && protectedWorld.get(build)==Block::Brick,"a player build reserves the entire building parcel");
  World legacy; legacy.coastOrigin=c; legacy.save(path,p.pose);
  std::ifstream in(path); std::vector<std::string> lines; for(std::string s;std::getline(in,s);) lines.push_back(s);
  lines[0]="BLOCKWORLD 13 7262026 0"; lines.erase(lines.begin()+13,lines.begin()+17);
  auto write=[&] {std::ofstream out(path);for(auto& s:lines) out<<s<<'\n';}; write();
  check(loaded.load(path).has_value() && loaded.coastOrigin==c && !loaded.harborLots,"v13 terrain-only saves migrate without silently adding buildings");
  lines[0]="BLOCKWORLD 14 7262026 0"; lines.insert(lines.begin()+13,"1 1024"); write();
  bool rejected=false;try{loaded.load(path);}catch(const std::exception&){rejected=true;}
  check(rejected && !loaded.harborLots,"invalid city mask rejects atomically");
}
}
int main(){try{persistence();std::cout<<"PASS city stairs, duplex bed, shortcuts, furniture, parcel protection and save migration\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

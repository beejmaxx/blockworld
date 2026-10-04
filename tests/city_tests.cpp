#include "city.hpp"
#include "building.hpp"
#include "ranch.hpp"
#include "inventory.hpp"
#include "adventure.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void walk(World& world,Player& player,glm::vec2 destination) {
  for(int i=0;i<2400;++i) {
    auto delta=destination-glm::vec2(player.pose.position.x,player.pose.position.z);
    if(glm::length(delta)<.07f) return;
    player.pose.yaw=std::atan2(delta.x,-delta.y);
    Movement move; move.forward=1; player.tick(world,move,1.f/60.f);
    check(!player.collides(world,player.pose.position),"city walking never clips through a wall or ceiling");
  }
  std::cerr<<"Stopped at "<<player.pose.position.x<<','<<player.pose.position.y<<','<<player.pose.position.z
    <<" toward "<<destination.x<<','<<destination.y<<'\n';
  throw std::runtime_error("city route must be walkable without jumping or flying");
}
void stairsAndHomes(World& world) {
  auto o=*world.cityOrigin;
  for(const auto& b : cityBuildings()) {
    Player p; p.pose.position={o.x+b.x+12.5f,float(o.y),o.z+b.z+b.depth+2.5f};
    auto go=[&](float x,float z){ walk(world,p,{o.x+b.x+x,o.z+b.z+z}); };
    go(12.5f,12.5f); go(3.5f,12.5f);
    for(int floor=0;floor<b.floors;++floor) {
      go(3.5f,5.5f); go(7.5f,5.5f); go(7.5f,12.5f);
      check(std::abs(p.pose.position.y-o.y-(floor+1)*cityFloorHeight)<.02f,"staircase reaches the next apartment floor");
      if(floor+1<b.floors) go(3.5f,12.5f);
    }
    go(7.5f,11.5f); go(12.5f,11.5f);
    check(std::abs(p.pose.position.y-o.y-b.floors*cityFloorHeight)<.02f,"rooftop penthouse has a connected entrance");
    check(world.get(o+Cell{b.x+b.width-4,b.floors*4,b.z+b.depth-4})==Block::BedZ,"penthouse contains a real bed");
    go(b.width-4.5f,11.5f); go(b.width-4.5f,b.depth-4.5f);
    world.clock.phase=.9;
    check(bedSleepStatus(world,p,o+Cell{b.x+b.width-4,b.floors*4,b.z+b.depth-4})==SleepResult::Ready,"penthouse bed is reachable and usable at night");
    go(b.width-4.5f,11.5f); go(12.5f,11.5f);
    // Descend every switchback to street level too.
    go(7.5f,11.5f); go(7.5f,12.5f);
    for(int floor=b.floors;floor>0;--floor) {
      go(7.5f,5.5f); go(3.5f,5.5f); go(3.5f,12.5f);
      if(floor>1) go(7.5f,12.5f);
    }
    go(12.5f,12.5f); go(12.5f,b.depth+2.5f);
    check(std::abs(p.pose.position.y-o.y)<.02f,"stairs lead all the way back to the street");
  }
}
void driveAndSwim(World& world) {
  auto o=*world.cityOrigin; Player p;
  check(visitCity(world,p) && bringCar(world,p).empty(),"city arrival and car delivery have clear ground");
  auto arrival=world.farm.car.position;
  auto aim=glm::normalize(arrival+glm::vec3(0,.8f,0)-p.eye());
  p.pose.yaw=std::atan2(aim.x,-aim.z); p.pose.pitch=std::asin(aim.y);
  auto target=targetRanch(world,p); RideState arrivalRide;
  check(target && mountRanch(world,p,arrivalRide,*target),"the delivered city car is within interaction reach");
  Movement forward; forward.forward=1;
  for(int i=0;i<180;++i) tickRanch(world,p,arrivalRide,forward,1.f/60.f);
  check(glm::length(world.farm.car.position-arrival)>25,"a newly delivered city car faces an open road, not the lake");
  struct Leg { glm::vec2 from,to; };
  for(auto leg : {Leg{{38,86},{38,29}},Leg{{38,29},{92,29}},Leg{{92,29},{92,94}},Leg{{92,94},{38,94}}}) {
    auto delta=leg.to-leg.from; float length=glm::length(delta);
    world.farm.car={true,{o.x+leg.from.x,float(o.y),o.z+leg.from.y},std::atan2(delta.x,-delta.y),0};
    RideState ride{true,true,0}; Movement drive; drive.forward=1;
    auto start=world.farm.car.position;
    for(int i=0;i<600 && glm::length(world.farm.car.position-start)<length-1;++i) tickRanch(world,p,ride,drive,1.f/60.f);
    check(glm::length(world.farm.car.position-start)>length-2,"car can drive each side of the waterfront loop without obstacles");
    check(std::abs(world.farm.car.position.y-o.y)<.01f,"city streets remain level under the wheels");
  }
  world.farm.car.owned=false;
  p={}; p.pose.position={o.x+52.5f,o.y-1.f,o.z+76.5f};
  for(int i=0;i<120;++i) p.tick(world,{},1.f/60.f);
  check(p.pose.position.y>o.y-1.1f && p.pose.position.y<o.y-.6f,"lake water supports the swimmer near its surface");
  walk(world,p,{o.x+52.5f,o.z+85.5f});
  check(std::abs(p.pose.position.y-o.y)<.02f,"swimmer can walk up the lake edge without being trapped");
}
void persistenceAndProtection() {
  World world(7262026,true); Player p; p.pose.position={10.5f,24,7.5f};
  world.ensure({0,0},1); Cell oldBuild{10,42,10}; world.set(oldBuild,Block::Glass);
  auto edits=world.editCount();
  check(initializeCity(world,p) && world.cityOrigin && world.editCount()==edits,"city geometry does not bloat the player's edit save");
  check(world.get(oldBuild)==Block::Glass,"existing farm builds remain unchanged");
  check(visitCity(world,p) && !p.collides(world,p.pose.position) && atCity(world,p),"T arrives on safe pavement");
  auto o=*world.cityOrigin;
  stairsAndHomes(world); driveAndSwim(world);
  Cell road=o+Cell{36,-1,85};
  auto mesh=buildMesh(world,world.chunks.at(chunkAt(road.x,road.z))); int pavement=0;
  for(const auto& v : mesh) if(v.block==glm::vec3(road.x,road.y,road.z) && v.position.y==o.y) {
    check(v.light>.5f,"graded streets receive daylight, not the old underground shading"); ++pavement;
  }
  check(pavement>0,"graded street lighting is checked on actual mesh vertices");
  Cell removed=o+Cell{44,3,4},added=o+Cell{65,1,87};
  check(breakBlock(world,removed) && world.set(added,Block::Sage),"generated buildings remain editable");
  auto path=std::filesystem::temp_directory_path()/("blockworld-city-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code e;std::filesystem::remove(path,e);} } cleanup{path};
  equipItem(world.inventory,world.crafting,Item::Lamp);
  world.save(path,p.pose);
  check(std::filesystem::file_size(path)<2048,"city saves only its layout origin and player changes");
  World loaded; check(loaded.load(path).has_value() && loaded.cityOrigin==world.cityOrigin && loaded.inventory.held()==Item::Lamp,"city origin and new materials survive saving");
  check(visitCity(loaded,p) && loaded.get(removed)==Block::Air && loaded.get(added)==Block::Sage,"player changes override generated city geometry after reload");
  loaded.evict({-100,100},1); loaded.ensure(chunkAt(removed.x,removed.z),0);
  check(loaded.get(removed)==Block::Air,"background chunk reload cannot restore demolished city blocks");

  World protectedWorld(7262026,true); Cell occupied{170,50,-86};
  protectedWorld.ensure(chunkAt(occupied.x,occupied.z),0); protectedWorld.set(occupied,Block::Brick);
  p.pose.position={10.5f,24,7.5f};
  check(initializeCity(protectedWorld,p) && protectedWorld.cityOrigin->x==-320 && protectedWorld.get(occupied)==Block::Brick,"even one edit reserves a proposed city parcel");
  // A v11 save has a castle record but no city record or new block/item IDs.
  World legacy(123,true); legacy.ensure({0,0},0); legacy.save(path,p.pose);
  std::ifstream input(path); std::vector<std::string> lines; for(std::string line;std::getline(input,line);) lines.push_back(line);
  lines[0]="BLOCKWORLD 11 123 1"; lines.erase(lines.begin()+11,lines.begin()+16);
  { std::ofstream output(path); for(const auto& line : lines) output<<line<<'\n'; }
  check(loaded.load(path).has_value() && !loaded.cityOrigin && loaded.terrain.seed()==123,"version-eleven worlds load without a city or lost state");
  auto saved=loaded.editCount(); lines[0]="BLOCKWORLD 12 123 1"; lines.insert(lines.begin()+11,"1 160 60 -96");
  { std::ofstream output(path); for(const auto& line : lines) output<<line<<'\n'; }
  bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
  check(rejected && !loaded.cityOrigin && loaded.editCount()==saved,"invalid city metadata is rejected without mutating the world");
}
}
int main() {
  try { persistenceAndProtection(); std::cout<<"PASS city streets, all apartment staircases, penthouses, swimming, streaming, and save migration\n"; }
  catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

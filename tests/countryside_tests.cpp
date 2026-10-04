#include "countryside.hpp"
#include "garden.hpp"
#include "minimap.hpp"
#include "ranch.hpp"
#include "road.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void aim(Player& p,glm::vec3 target) {
  auto d=glm::normalize(target-p.eye());p.pose.yaw=std::atan2(d.x,-d.z);p.pose.pitch=std::asin(d.y);
}
struct Fixture {
  World world{7262026,true};Player player;
  Fixture() {
    player.pose.position={10.5f,24,7.5f};world.ensure({0,0},2);
    initializeFarm(world);initializeHome(world);initializeGarden(world);
    check(initializeRoad(world,player),"fixture highway initializes");
  }
};
void gameplay() {
  Fixture f;auto& w=f.world;auto& p=f.player;
  auto oldCrops=w.farm.crops;auto road=w.road;auto oldEdits=w.editCount();
  w.farm.garden.coins=123;w.inventory.selected=3;
  check(initializeCountryside(w,p),"farm district installs beside the road");auto o=*w.countrysideOrigin;
  check(w.farm.crops.size()>450 && w.farm.crops.size()<cropLimit,"large district has hundreds of actual growing crops");
  check(w.road==road && w.farm.garden.coins==123 && w.inventory.selected==3,"farm expansion preserves road, money and inventory");
  for(auto c:oldCrops)check(w.cropAt({c.x,c.y,c.z})->age==c.age,"starter crops keep their growth");
  auto count=w.farm.crops.size(),edits=w.editCount();
  check(initializeCountryside(w,p) && w.farm.crops.size()==count && w.editCount()==edits,"reopening cannot duplicate crops or rebuild the farm");
  check(edits-oldEdits==count-oldCrops.size(),"procedural buildings do not bloat the save with block edits");
  check(w.farm.livestock.size()==4,"two cows, a horse and a sheep inhabit the pastures");
  for(const auto& animal:w.farm.livestock)check(animal.position.y==o.y && collidable(w.get({int(animal.position.x),o.y-1,int(animal.position.z)})),"new animals start above solid ground");
  w.ensure(chunkAt(o.x+10,o.z+8),0);
  check(w.get(o+Cell{10,0,8})==Block::BedZ && w.get(o+Cell{10,0,7})==Block::BedZHead,"barn contains a complete bed");
  p.pose.position={o.x+10.5f,float(o.y),o.z+10.5f};w.clock.phase=.8;
  check(bedSleepStatus(w,p,o+Cell{10,0,8})==SleepResult::Ready,"barn bed is reachable with clear headroom");
  Cell picked{};
  int expected=0;
  for(auto kind:cropKinds) {
    auto found=std::ranges::find_if(w.farm.crops,[&](auto c){return c.kind==kind && c.age>=cropGrowSeconds(kind) && c.x>=o.x;});
    check(found!=w.farm.crops.end(),"each field includes ripe crops");
    Cell c{found->x,found->y,found->z};
    p.pose.position={c.x+.5f,float(c.y),c.z+1.8f};aim(p,{c.x+.5f,c.y+.3f,c.z+.5f});
    check(harvestCrop(w,p,c)==FarmUse::Harvested,"field crops can be picked through normal farming interactions");
    expected+=cropYield(kind)*cropPrice(kind);if(kind==CropKind::Wheat)picked=c;
  }
  check(sellBasket(w)==expected && w.farm.garden.coins==123+expected,"field harvests sell for the normal crop prices");
  Cell nursery=o+Cell{40,0,7};check(w.cropShelter(nursery)==2,"glass nursery gives the greenhouse bonus");
  auto age=w.cropAt(nursery)->age;
  w.evict({-100,-100},0);for(auto& crop:w.farm.crops)crop.shelter=-1;
  check(w.cropShelter(nursery)==2,"procedural glass roof remains effective outside the loaded area");
  w.farm.garden.weather=0;w.growCrops(4);
  check(w.cropAt(nursery)->age==age+5,"unloaded nursery crops grow 25 percent faster");
  w.ensure(chunkAt(picked.x,picked.z),0);check(w.get(picked)==Block::Air,"streaming does not replant a harvested field");
  // Drive the full farm lane, through the produce stand, without a teleport.
  w.farm.car={true,{o.x+56.f,float(o.y),o.z-22.f},3.14159265f};RideState ride{true,true,0};
  for(int frame=0;frame<1000 && w.farm.car.position.z<o.z+92;++frame) {
    auto car=w.farm.car.position;w.ensure(chunkAt(int(car.x),int(car.z)),1);
    tickRanch(w,p,ride,{.forward=.25f},1.f/60);
  }
  check(w.farm.car.position.z>=o.z+92,"farm entrance, market canopy and field lane are drivable");
  std::cout<<"Farm parcel "<<o.x<<','<<o.y<<','<<o.z<<": "<<count-oldCrops.size()<<" crops; sale "<<expected<<" coins\n";
}
void protection() {
  Fixture baseline;check(initializeCountryside(baseline.world,baseline.player),"baseline farm location exists");
  auto first=*baseline.world.countrysideOrigin;Fixture f;auto occupied=first+Cell{20,0,60};
  f.world.ensure(chunkAt(occupied.x,occupied.z),0);f.world.set(occupied,Block::BlueTile);
  check(initializeCountryside(f.world,f.player) && f.world.countrysideOrigin!=first,"occupied farmland chooses another parcel");
  check(f.world.get(occupied)==Block::BlueTile,"existing player construction survives expansion");
  auto o=*f.world.countrysideOrigin;f.world.set(o+Cell{10,0,8},Block::Air);
  f.world.evict({-100,-100},0);f.world.ensure(chunkAt(o.x+10,o.z+8),0);
  check(f.world.get(o+Cell{10,0,8})==Block::Air,"removed procedural furnishings stay removed");
}
void maps() {
  Fixture f;check(initializeCountryside(f.world,f.player),"mapped farm exists");auto& w=f.world;
  auto chunks=w.chunks.size(),edits=w.editCount();
  auto local=buildMiniMap(w,{10,7},false,false),overview=buildMiniMap(w,{10,7},true,false);
  check(w.chunks.size()==chunks && w.editCount()==edits,"minimap neither loads terrain nor alters the world");
  check(overview.radius>local.radius && !overview.roads.empty(),"overview includes the highway at a wider scale");
  for(auto marker:overview.markers) {
    auto p=mapPoint(overview,marker.position);
    check(p.x>=0 && p.x<=1 && p.y>=0 && p.y<=1,"landmarks fit inside the route overview");
  }
  check(std::ranges::any_of(overview.markers,[](auto m){return m.name=="Farms";}),"farm entrance is named on the map");
  MiniMap square;square.center={-100,-200};square.radius=10;
  auto across=mapLine(square,{-120,-200},{-80,-200});
  check(across && glm::length((*across)[0]-glm::vec2(0,.5f))<.001f && glm::length((*across)[1]-glm::vec2(1,.5f))<.001f,"crossing roads clip to both edges at negative coordinates");
  check(!mapLine(square,{-120,-220},{-80,-220}),"outside parallel roads are omitted");
  check(!mapLine(square,{-120,-220},{-115,-215}),"diagonal roads outside the viewport are omitted");
}
void persistence() {
  auto path=std::filesystem::temp_directory_path()/("blockworld-country-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}} cleanup{path};
  Fixture f;auto& w=f.world;check(initializeCountryside(w,f.player),"save fixture farm exists");
  auto o=*w.countrysideOrigin;Cell c=o+Cell{8,0,52};w.set(c,Block::Air);w.save(path,f.player.pose);
  World loaded;check(loaded.load(path).has_value() && loaded.countrysideOrigin==w.countrysideOrigin && loaded.farm.crops.size()==w.farm.crops.size(),"farm layout and a crop population larger than 256 round-trip");
  loaded.ensure(chunkAt(c.x,c.z),0);check(loaded.get(c)==Block::Air,"harvested ground stays empty after save and reload");
  // The v16 origin follows the saved road. Reject bad metadata atomically.
  World blank;blank.coastOrigin=Cell{512,18,-192};blank.road={{16,24,40},{100,24,40}};blank.save(path,f.player.pose);
  std::ifstream in(path);std::vector<std::string> lines;for(std::string s;std::getline(in,s);)lines.push_back(s);
  auto write=[&](const auto& data){std::ofstream out(path);for(auto& s:data)out<<s<<'\n';};
  for(std::string bad:{"1 300 120 70","1 100000 24 70","0 300 24 70"}) {
    auto altered=lines;altered[17]=bad;write(altered);bool rejected=false;
    try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.countrysideOrigin==w.countrysideOrigin,"invalid countryside origin leaves the loaded save unchanged");
  }
  lines[0]="BLOCKWORLD 15 7262026 0";lines.erase(lines.begin()+17,lines.begin()+19);write(lines);
  check(loaded.load(path).has_value() && !loaded.countrysideOrigin && loaded.road==blank.road,"v15 highway saves load before countryside installation");
}
}
int main() {
  try{gameplay();protection();maps();persistence();std::cout<<"PASS countryside harvests, road access, shelter, preservation, saves and minimap\n";}
  catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

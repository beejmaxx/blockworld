#include "garden.hpp"
#include "inventory.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
World meadow() { World w(7262026,true); w.ensure({0,0},2); return w; }
void aim(Player& p,glm::vec3 target) {
  auto d=glm::normalize(target-p.eye()); p.pose.yaw=std::atan2(d.x,-d.z); p.pose.pitch=std::asin(d.y);
}
void equip(World& w,Item item) { check(equipItem(w.inventory,w.crafting,item),"garden tool equips"); }
void farmingSession() {
  auto w=meadow(); Player p; p.pose.position={18.5f,24,10.8f}; Cell soil{18,23,8},plant{18,24,8};
  aim(p,{18.5f,23.99f,8.5f});
  check(placementStatus(w,p,plant,Block::CarrotYoung)==PlacementStatus::NeedsSoil,"raw grass must be prepared first");
  check(!tillSoil(w,p,soil),"empty hands do not hoe soil"); equip(w,Item::Hoe);
  check(useTarget(w,p,Item::Hoe).kind==UseKind::Till && tillSoil(w,p,soil),"the visible hoe prepares the aimed patch");
  check(!tillSoil(w,p,soil) && w.get(soil)==Block::Farmland,"repeated hoe use leaves a prepared bed intact");
  check(placeBlock(w,p,plant,Block::CarrotYoung),"plant carrot seeds in the bed");
  aim(p,{18.5f,24.13f,8.5f}); equip(w,Item::Compost);
  check(useTarget(w,p,Item::Compost).kind==UseKind::Compost && applyCompost(w,p,plant),"compost is applied to the aimed growing plant");
  check(w.farm.garden.compost==2 && !applyCompost(w,p,plant) && w.farm.garden.compost==2,"compost is consumed once and cannot stack");
  equip(w,Item::WateringCan); check(waterCrop(w,p,plant),"water the carrot row"); growFarm(w,37.5f);
  aim(p,{18.5f,24.3f,8.5f});
  check(harvestCrop(w,p,plant)==FarmUse::Harvested && w.farm.carrots==4,"the first composted harvest yields four carrots");
  check(w.get(plant)==Block::Air && w.get(soil)==Block::Farmland && !w.cropAt(plant),"carrots are pulled out and leave a reusable bed");
  check(sellBasket(w)==8 && w.farm.garden.coins==8 && w.farm.carrots==0 && sellBasket(w)==0,"one sale turns the basket into coins exactly once");
  check(!buyGardenSupply(w,GardenPurchase::Sprinkler) && w.farm.garden.coins==8,"insufficient funds cannot grant an upgrade");
  check(placeBlock(w,p,plant,Block::CarrotYoung),"replant the harvested bed");
  aim(p,{18.5f,24.13f,8.5f}); check(waterCrop(w,p,plant),"water the next crop"); growFarm(w,37.5f);
  aim(p,{18.5f,24.3f,8.5f}); check(harvestCrop(w,p,plant)==FarmUse::Harvested,"pick the second harvest");
  check(sellBasket(w)==4 && buyGardenSupply(w,GardenPurchase::Sprinkler),"two harvests earn the first sprinkler");
  check(w.farm.garden.coins==0 && w.farm.garden.sprinklers==1,"buying deducts the exact price and grants one item");
  Cell sprinkler{20,24,8};
  check(placeBlock(w,p,sprinkler,Block::Sprinkler) && w.farm.garden.sprinklers==0 && w.farm.sprinklers.size()==1,"placing the sprinkler consumes its stock");
  check(!placeBlock(w,p,{21,24,8},Block::Sprinkler),"inventory catalog cannot create free sprinklers");
  check(placeBlock(w,p,plant,Block::CarrotYoung),"plant beside the new sprinkler"); growFarm(w,10);
  check(w.cropAt(plant)->age==20 && w.cropAt(plant)->water==45,"sprinkler waters its full five by five area");
  check(breakBlock(w,sprinkler) && w.farm.garden.sprinklers==1 && w.farm.sprinklers.empty(),"a removed sprinkler can be moved without buying it again");
  check(w.farm.garden.flags==63 && gardenGuide(w,p).stage==6,"the whole gardening lesson can be completed through play");
  w.farm.garden.coins=coinLimit; w.farm.carrots=2;
  check(sellBasket(w)==0 && w.farm.carrots==2,"a full coin wallet preserves the harvest");
  w.farm.garden.compost=gardenSupplyLimit;
  check(!buyGardenSupply(w,GardenPurchase::Compost) && w.farm.garden.coins==coinLimit,"a full supply bag cannot waste coins");
}
void cropChoicesAndReach() {
  auto w=meadow(); Player p; p.pose.position={18.5f,24,10.8f}; Cell plant{18,24,8};
  w.set({18,23,8},Block::Farmland); check(placeBlock(w,p,plant,Block::StrawberryYoung),"strawberries start from seeds");
  growFarm(w,105); aim(p,{18.5f,24.3f,8.5f});
  equip(w,Item::Hoe); check(useTarget(w,p,Item::Hoe).kind==UseKind::Crop,"ripe crops can be picked while holding a garden tool");
  check(harvestCrop(w,p,plant)==FarmUse::Harvested && w.get(plant)==Block::StrawberryGrowing && w.cropAt(plant)->age==52.5f,"picking berries preserves a grown flowering bush");
  check(harvestCrop(w,p,plant)==FarmUse::None,"a held harvest cannot duplicate berries");
  growFarm(w,52.5f); check(harvestCrop(w,p,plant)==FarmUse::Harvested && w.farm.strawberries==6,"the same bush produces a second crop sooner");
  w.set(plant,Block::Air);
  for(int z=6;z<=10;++z) for(int x=16;x<=20;++x) w.set({x,23,z},Block::Farmland);
  check(placeBlock(w,p,plant,Block::PumpkinYoung),"pumpkin vines fit in an open bed");
  for(auto kind : cropKinds) check(placementStatus(w,p,{19,24,9},cropStage(kind,0))==PlacementStatus::PumpkinSpace,"pumpkins reserve a gap from all other crops, including diagonals");
  check(placeBlock(w,p,{20,24,8},Block::CarrotYoung),"one empty block between crops is enough");
  w.set(plant,Block::Air);
  check(placementStatus(w,p,{19,24,8},Block::PumpkinYoung)==PlacementStatus::PumpkinSpace,"a new pumpkin cannot crowd an existing vegetable");
  w.set(plant,Block::CarrotYoung); equip(w,Item::Compost); aim(p,{18.5f,24.13f,8.5f});
  w.set({18,24,10},Block::Stone); w.set({18,25,10},Block::Stone);
  check(!applyCompost(w,p,plant) && w.farm.garden.compost==3,"compost cannot pass through walls or consume a dose on failure");
  w.set({18,24,10},Block::Air); w.set({18,25,10},Block::Air);
  p.pose.position.z=15; aim(p,{18.5f,24.13f,8.5f});
  check(!applyCompost(w,p,plant),"compost respects reach");
  equip(w,Item::Hoe); aim(p,{17.5f,23.99f,8.5f});
  check(!tillSoil(w,p,{17,23,8}),"the hoe cannot till distant land");
}
void weatherAndShelter() {
  auto w=meadow(); w.farm.garden.initialized=true; w.farm.garden.weather=149;
  Cell outdoor{16,24,8},roofed{18,24,8},glass{20,24,8};
  for(Cell c : {outdoor,roofed,glass}) { w.set(c+Cell{0,-1,0},Block::Farmland); w.set(c,Block::PumpkinYoung); }
  w.set(roofed+Cell{0,3,0},Block::Wood); w.set(glass+Cell{0,3,0},Block::Glass);
  auto many=w;
  w.growCrops(2);
  check(w.farm.garden.raining() && w.cropAt(outdoor)->age==3 && w.cropAt(outdoor)->water==45,"a shower begins at the correct point inside a time step");
  check(w.cropAt(roofed)->age==2 && w.cropAt(roofed)->water==0,"rain does not water crops through a solid roof");
  check(w.cropAt(glass)->age==2.5f && w.cropAt(glass)->water==0,"glass gives a growing bonus but still blocks rain");
  for(int i=0;i<8;++i) many.growCrops(.25f);
  check(many.cropAt(outdoor)->age==w.cropAt(outdoor)->age && many.farm.garden.weather==w.farm.garden.weather,"weather growth agrees across frame sizes");
  w.set(roofed+Cell{0,3,0},Block::Air); w.growCrops(1);
  check(w.cropAt(roofed)->water==45,"removing a roof updates rain exposure");
  w.set(glass+Cell{0,3,0},Block::Wood); w.growCrops(1);
  check(w.cropAt(glass)->age==4.75f,"changing a glass roof removes its bonus immediately");
  w.farm.garden.weather=194; w.growCrops(46);
  check(!w.farm.garden.raining() && w.farm.garden.weather==0 && w.cropAt(outdoor)->water==0,"rain stops and residual moisture expires after the shower");
  auto unloaded=w; unloaded.evict({50,50},1);
  unloaded.farm.garden.weather=150; w.farm.garden.weather=150;
  // Invalidate the cache exactly as a reload would.
  for(auto& crop : unloaded.farm.crops) crop.shelter=-1;
  w.growCrops(2); unloaded.growCrops(2);
  for(Cell c : {outdoor,roofed,glass}) check(w.cropAt(c)->age==unloaded.cropAt(c)->age && w.cropAt(c)->water==unloaded.cropAt(c)->water,"weather and roofs work in unloaded chunks");
  w.growCrops(std::numeric_limits<float>::max());
  check(std::isfinite(w.farm.garden.weather) && w.farm.garden.weather<showerPeriod,"very long time skips remain bounded");
  for(const auto& crop : w.farm.crops) check(crop.age==150,"neglected crops remain safely ripe");
}
void starterAndGreenhouse() {
  auto w=meadow(); Cell existing{16,24,7}; w.set(existing,Block::Brick);
  check(initializeGarden(w) && w.get(existing)==Block::Brick && w.farm.garden.origin!=glm::ivec3(16,24,7),"starter garden chooses another patch instead of overwriting a build");
  auto edits=w.editCount(); check(!initializeGarden(w) && w.editCount()==edits,"starter garden is created only once");
  Player p; check(visitGarden(w,p) && !p.collides(w,p.pose.position),"garden return lands on a clear path");
  auto blocked=meadow();
  for(Cell c : {Cell{16,24,7},{-4,24,-16},{14,24,-16}}) blocked.set(c,Block::Brick);
  edits=blocked.editCount(); check(!initializeGarden(blocked) && blocked.editCount()==edits,"fully occupied starter sites leave all existing terrain alone");
  w=meadow(); p.pose.position={18.5f,24,15.2f}; Cell door{18,24,12};
  equip(w,Item::Greenhouse); aim(p,{18.5f,23.99f,12.5f});
  check(!placeGreenhouse(w,p,door),"a greenhouse cannot be placed without a purchased kit");
  w.farm.garden.coins=40; check(buyGardenSupply(w,GardenPurchase::Greenhouse),"greenhouse can be bought with harvest income");
  w.set({17,24,10},Block::Brick); edits=w.editCount();
  check(!placeGreenhouse(w,p,door) && w.editCount()==edits && w.farm.garden.greenhouses==1,"obstructed greenhouse fails atomically without consuming the kit");
  w.set({17,24,10},Block::Air);
  check(placeGreenhouse(w,p,door) && w.farm.garden.greenhouses==0,"clear level ground accepts a greenhouse kit");
  check(w.get(door)==Block::DoorZ && w.get({18,27,10})==Block::Glass && w.get({18,23,10})==Block::Farmland,"greenhouse has a working doorway, glass roof, and prepared planting beds");
  check(w.cropShelter({18,24,10})==2,"the constructed greenhouse supplies its advertised growing bonus");
  check(!placeGreenhouse(w,p,door),"placing again cannot duplicate the kit");
  aim(p,{18.5f,24.5f,12.9f});
  for(auto item : {Item::Hoe,Item::Compost,Item::WateringCan,Item::Greenhouse})
    check(useTarget(w,p,item).kind==UseKind::Door,"garden tools still allow the greenhouse door to be used");
}
void homeAndBed() {
  auto world=meadow(); Player player;
  check(initializeGarden(world) && initializeHome(world),"fresh farming world receives vegetable beds and a complete home");
  check(inspectCabin(world).complete() && world.get(cabinBed.cell)==Block::BedZ,"starter home includes its bed, roof, door, and windows");
  check(visitGarden(world,player),"garden remains separately reachable");
  player.pose.flying=true; player.velocity={0,8,0};
  check(visitHome(world,player) && !player.pose.flying && player.velocity==glm::vec3(0)
        && !player.collides(world,player.pose.position),"return home leaves the garden, stops flying, and lands in clear space");
  world.clock.phase=.8;
  check(bedSleepStatus(world,player,cabinBed.cell)==SleepResult::Ready && sleepInBed(world,player,cabinBed.cell)==SleepResult::Ready,"home landing puts the bed within sleeping reach");
  world.set({10,28,-4},Block::Air);
  auto edits=world.editCount();
  check(!initializeHome(world) && world.editCount()==edits && world.get({10,28,-4})==Block::Air,"later visits never rebuild deliberate changes to the starter home");
  world=meadow(); world.set({10,25,-3},Block::Brick); edits=world.editCount();
  check(!initializeHome(world) && world.editCount()==edits && world.get({10,25,-3})==Block::Brick,"existing builds are left untouched");
}
void savingAndMigration() {
  auto dir=std::filesystem::temp_directory_path()/("blockworld-soil-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e; std::filesystem::remove_all(p,e);} } cleanup{dir};
  auto path=dir/"garden.bw"; auto w=meadow(); Cell plant{18,24,8};
  w.farm.initialized=true; w.farm.garden={true,{16,24,7},37,2,3,1,63,164};
  w.set(plant+Cell{0,-1,0},Block::Farmland); w.set(plant,Block::CarrotYoung);
  w.compostCrop(plant); w.waterCrop(plant); w.growCrops(10); w.farm.carrots=8;
  w.set({20,24,8},Block::Sprinkler);
  w.inventory.slots={Item::Hoe,Item::Compost,Item::Sprinkler,Item::Greenhouse,Item::WateringCan,Item::Carrot,Item::Strawberry,Item::Pumpkin,Item::Wheat};
  Player p; p.pose.position={18.5f,24,11}; w.save(path,p.pose);
  World loaded; check(loaded.load(path).has_value(),"new gardening save loads");
  check(loaded.farm.garden.coins==37 && loaded.farm.garden.compost==2 && loaded.farm.garden.sprinklers==3
      && loaded.farm.garden.greenhouses==1 && loaded.farm.garden.flags==63 && loaded.farm.garden.weather==174
      && loaded.farm.garden.origin==w.farm.garden.origin && loaded.farm.garden.initialized,"garden layout, coins, supplies, lesson and weather survive reload");
  check(loaded.cropAt(plant)->composted && loaded.farm.sprinklers.size()==1 && loaded.inventory==w.inventory,"composted crops, irrigation, and all held garden tools survive reload");
  loaded.growCrops(10); check(loaded.cropAt(plant)->age==40 && loaded.cropAt(plant)->water==45,"saved irrigation works before its chunk is loaded");
  std::ifstream saved(path); std::vector<std::string> lines; std::string line;
  while(std::getline(saved,line)) lines.push_back(line);
  for(auto bad : {"1 16 24 7 -1 2 3 1 63 174","1 16 24 7 37 1000 3 1 63 174",
                  "1 16 24 7 37 2 3 1 64 174","1 16 24 7 37 2 3 1 63 240",
                  "1 16 24 7 37 2 3 1 63 nan","1 100000 24 7 37 2 3 1 63 174"}) {
    auto altered=lines; altered[8]=bad; std::ofstream out(path); for(const auto& v : altered) out<<v<<'\n'; out.close();
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.farm.garden.coins==37 && loaded.cropAt(plant)->age==40,"invalid gardening data rejects without changing the loaded world");
  }
  std::ofstream old(path);
  old<<"BLOCKWORLD 8 7262026 1\n18.5 24 11 0 0 0\n127\n0.4 3\n0 0 0 0\n"
     <<"1 7 2 31 3.5 24 8.5 0 1\n18 24 8 37.5 1 12\n4 5 6\n0 22 19 20 21 14 1 2 3 4\n2\n18 24 8 34\n0 24 0 7\n";
  old.close(); check(loaded.load(path).has_value(),"version eight saves migrate"); loaded.ensure({0,0},2);
  check(loaded.farm.carrots==4 && loaded.farm.strawberries==5 && loaded.farm.pumpkins==6 && loaded.farm.wheat==7
      && loaded.cropAt(plant)->age==37.5f && loaded.cropAt(plant)->water==12 && !loaded.cropAt(plant)->composted
      && loaded.inventory.held()==Item::WateringCan && loaded.get({0,24,0})==Block::Planks,"migration preserves harvests, original crops, hotbar and buildings");
  check(!loaded.farm.garden.initialized && loaded.farm.garden.coins==0 && loaded.farm.garden.compost==3,"old farms receive a fresh gardening lesson and starter compost");
}
}
int main() {
  try {
    farmingSession(); cropChoicesAndReach(); weatherAndShelter(); starterAndGreenhouse(); homeAndBed(); savingAndMigration();
    std::cout<<"PASS vegetable garden, harvest economy, irrigation, weather, greenhouse and saves\n";
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

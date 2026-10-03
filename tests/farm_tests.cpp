#include "farm.hpp"
#include "inventory.hpp"
#include "garden.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
World flatWorld() {
  World world;
  for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) world.insert(Chunk{{x,z}});
  for(int z=-12;z<=18;++z) for(int x=-12;x<=18;++x) world.set({x,1,z},Block::Farmland);
  world.farm.home={.5f,2,.5f}; world.farm.initialized=true; return world;
}
void aim(Player& p,glm::vec3 target) {
  auto d=glm::normalize(target-p.eye()); p.pose.yaw=std::atan2(d.x,-d.z); p.pose.pitch=std::asin(d.y);
}
void addChicken(World& world,glm::vec3 position) { Chicken c; c.position=position; world.farm.chickens.push_back(c); }
void pen(World& world) {
  Player builder; builder.pose.position={10,2,10};
  for(const auto& piece : penBlueprint(world)) check(placeBlock(world,builder,piece.cell,piece.block),"pen piece can be placed");
}
void planting() {
  auto world=flatWorld(); Player p; p.pose.position={2.5f,2,3.5f}; Cell crop{2,2,0};
  world.set({2,1,0},Block::Stone);
  check(placementStatus(world,p,crop,Block::WheatYoung)==PlacementStatus::NeedsSoil
        && !placeBlock(world,p,crop,Block::WheatYoung),"wheat needs soil");
  world.set({2,1,0},Block::Farmland);
  check(placeBlock(world,p,crop,Block::WheatYoung) && world.farm.crops.size()==1,"planting registers one crop");
  check(!p.collides(world,{2.5f,2,.5f}),"plants do not trap the player");
  growFarm(world,44); check(world.get(crop)==Block::WheatYoung,"seedlings grow gradually");
  growFarm(world,1); check(world.get(crop)==Block::WheatGrowing,"half-grown wheat changes its mesh");
  growFarm(world,std::numeric_limits<float>::quiet_NaN()); growFarm(world,-1);
  check(world.farm.crops.front().age==45,"invalid time cannot poison crops");
  growFarm(world,45); check(world.get(crop)==Block::WheatRipe,"wheat ripens after ninety play seconds");
  aim(p,{2.5f,2.5f,.5f});
  check(harvestWheat(world,p,crop)==FarmUse::Harvested && world.farm.wheat==3,"use harvests three wheat within reach");
  check(world.get(crop)==Block::Air && world.farm.crops.empty(),"annual harvest leaves an empty prepared bed");
  check(harvestWheat(world,p,crop)==FarmUse::None && world.farm.wheat==3,"unripe wheat cannot be harvested again");
  check(placeBlock(world,p,crop,Block::WheatYoung),"a harvested bed can be planted again");
  growFarm(world,90); world.farm.wheat=9999;
  check(harvestWheat(world,p,crop)==FarmUse::None && world.get(crop)==Block::WheatRipe,"a full bag leaves the harvest intact");
  world.farm.wheat=0; p.pose.position={2.5f,2,8.5f}; aim(p,{2.5f,2.5f,.5f});
  check(harvestWheat(world,p,crop)==FarmUse::None,"harvesting obeys reach");
  world.evict({50,50},1); growFarm(world,500); world.ensure({0,0},1);
  check(world.get(crop)==Block::WheatRipe,"crop state survives chunk eviction");
  check(breakBlock(world,{2,1,0}) && world.get(crop)==Block::Air && world.farm.crops.empty(),"removing soil also removes its crop timer");

  world=flatWorld(); p.pose.position={-8,2,-8};
  for(int z=1;z<=16;++z) for(int x=1;x<=16;++x) check(placeBlock(world,p,{x,2,z},Block::WheatYoung),"garden within budget can be planted");
  check(world.farm.crops.size()==cropLimit && placementStatus(world,p,{0,2,0},Block::WheatYoung)==PlacementStatus::CropLimit,"crop population is bounded");
  breakBlock(world,{1,2,1}); check(placeBlock(world,p,{0,2,0},Block::WheatYoung),"removing a crop frees a garden slot");
}
void flockAndGates() {
  auto world=flatWorld(); pen(world);
  for(glm::vec3 p : {glm::vec3(-.5f,2,-.5f),{.5f,2,-.5f},{-.5f,2,.5f},{.5f,2,.5f}}) addChicken(world,p);
  Player player; player.pose.position={.5f,2,6.5f};
  for(int i=0;i<1200;++i) {
    tickFarm(world,player,.1f,false);
    for(const auto& c : world.farm.chickens)
      check(c.position.x>-2 && c.position.x<3 && c.position.z>-2 && c.position.z<3.3f
            && std::abs(c.position.y-2)<.01f,"closed pen contains every chicken throughout wandering");
  }
  check(world.farm.flags&PenBuilt,"real completed fence advances the guide");
  world.farm.chickens.clear(); addChicken(world,{.5f,2,2.3f}); world.farm.wheat=3;
  player.pose.position={.5f,2,5.5f}; Cell gate{0,2,3};
  for(int i=0;i<60;++i) tickFarm(world,player,.1f,true);
  check(world.farm.chickens.front().position.z<3.21f,"food does not draw a chicken through a closed gate");
  aim(player,{.5f,2.5f,3.5f});
  bool opened=toggleGate(world,player,gate);
  if(!opened) {
    const auto& c=world.farm.chickens.front(); auto hit=world.raycast(player.eye(),player.direction(),3.5f);
    std::cerr<<"Gate debug: chicken "<<c.position.x<<' '<<c.position.y<<' '<<c.position.z
             <<"; hit "<<(hit ? int(world.get(hit->block)) : -1)<<"; open-panel overlap "
             <<chickensOverlap(world,blockBounds(gate,Block::GateZOpen))<<'\n';
  }
  check(opened && gateOpen(world.get(gate)),"use opens the gate");
  check(!player.collides(world,{.5f,2,3.5f}),"open gate is a walkable opening");
  for(int i=0;i<40;++i) tickFarm(world,player,.1f,true);
  check(world.farm.chickens.front().position.z>4.1f,"chicken follows wheat through an open gate");
  world.farm.chickens.front().position={.5f,2,3.5f}; aim(player,{.07f,2.5f,3.5f});
  check(!toggleGate(world,player,gate) && gateOpen(world.get(gate)),"closing cannot trap a chicken in a gate");
  world.farm.chickens.front().position={.5f,2,1.5f};
  check(toggleGate(world,player,gate),"gate closes once the opening is clear");
  auto before=world.farm.chickens.front().position; world.clock.phase=0;
  for(int i=0;i<30;++i) tickFarm(world,player,.1f,true);
  check(glm::length(world.farm.chickens.front().position-before)<.01f && world.farm.chickens.front().sleeping,"flock settles down at night");
  before=world.farm.chickens.front().position; tickFarm(world,player,std::numeric_limits<float>::quiet_NaN(),true);
  check(before==world.farm.chickens.front().position,"invalid frame time does not move an animal");

  world=flatWorld(); addChicken(world,{.5f,2,.5f});
  check(placementStatus(world,player,{0,2,0},Block::Stone)==PlacementStatus::ChickenOverlap,"blocks cannot be placed inside chickens");
  world.set({0,1,0},Block::Air); world.chunks.at({0,0}).set(0,0,0,Block::Stone);
  world.clock.phase=0;
  for(int i=0;i<20;++i) tickFarm(world,player,.1f,false);
  check(std::abs(world.farm.chickens.front().position.y-1)<.01f,"chicken falls safely when its floor is removed");
}
void chickenTurning() {
  auto world=flatWorld(); addChicken(world,{.5f,2,.5f});
  Player player; player.pose.position={2.5f,2,2.5f}; world.farm.wheat=1;
  // Reproduce a bird falling into the hole left by a click on its floor.
  world.set({0,1,0},Block::Air); world.chunks.at({0,0}).set(0,0,0,Block::Stone);
  constexpr float dt=1.f/60.f,pi=3.14159265f;
  int rests=0;
  for(int i=0;i<600;++i) {
    float yaw=world.farm.chickens[0].yaw;
    tickFarm(world,player,dt,true);
    const auto& chicken=world.farm.chickens[0];
    float turned=std::abs(std::remainder(chicken.yaw-yaw,2*pi));
    check(turned<=3.f*dt+.00001f,"a chicken in a dug hole turns smoothly instead of spinning each frame");
    if(!chicken.moving) { ++rests; check(turned<.00001f,"a blocked chicken rests without rotating in place"); }
  }
  check(rests>0,"a chicken pauses when it cannot find a step");
  world=flatWorld(); addChicken(world,{.5f,2,.5f});
  auto start=world.farm.chickens[0].position;
  world.farm.wheat=1; player.pose.position={.5f,2,5.5f};
  for(int i=0;i<120;++i) tickFarm(world,player,dt,true);
  check(glm::length(world.farm.chickens[0].position-player.pose.position)<glm::length(start-player.pose.position),"smooth turning still lets hens follow wheat");
}
void chickenStepsOutOfHole() {
  auto world=flatWorld(); Player player; player.pose.position={2.5f,2,.5f};
  world.farm.wheat=1; world.clock.phase=.4;
  world.set({0,1,0},Block::Air); world.chunks.at({0,0}).set(0,0,0,Block::Stone);
  addChicken(world,{.5f,1,.5f});
  for(int i=0;i<120;++i) tickFarm(world,player,.1f,true);
  const auto& hen=world.farm.chickens.front();
  check(hen.position.x>1.2f && std::abs(hen.position.y-2)<.01f,
        "a hen follows wheat up the edge of a one-block hole");
  check(world.get({0,1,0})==Block::Air && world.get({0,0,0})==Block::Stone && world.get({1,1,0})==Block::Farmland,
        "climbing out preserves the player's terrain");

  world.farm.chickens.front().position={.5f,0,.5f}; world.chunks.at({0,0}).set(0,0,0,Block::Air);
  for(int i=0;i<120;++i) tickFarm(world,player,.1f,true);
  check(world.farm.chickens.front().position.y<.01f,
        "hens cannot climb a two-block wall without a step");
}
void gardening() {
  for(auto kind : cropKinds) {
    auto world=flatWorld(); Player player; player.pose.position={2.5f,2,3.2f}; Cell cell{2,2,0};
    auto seed=cropStage(kind,0);
    world.set({2,1,0},Block::Stone);
    check(!placeBlock(world,player,cell,seed),"every crop requires prepared soil");
    world.set({2,1,0},Block::Farmland);
    check(placeBlock(world,player,cell,seed) && world.cropAt(cell)->kind==kind,"each seed plants its own crop");
    check(!player.collides(world,{2.5f,2,.5f}),"every growing crop remains walkable");
    aim(player,{2.5f,2.12f,.5f});
    check(!waterCrop(world,player,cell),"watering requires the can in the selected slot");
    equipItem(world.inventory,world.crafting,Item::WateringCan);
    check(useTarget(world,player,Item::WateringCan).kind==UseKind::Water
          && waterCrop(world,player,cell),"the can targets and waters a seedling within reach");
    check(!waterCrop(world,player,cell) && world.cropAt(cell)->water==45,"repeated use does not stack water bonuses");
    growFarm(world,10);
    check(world.cropAt(cell)->age==20 && world.cropAt(cell)->water==35,"water doubles growth as its timer counts down");
    growFarm(world,200);
    check(cropRipe(world.get(cell)) && world.cropAt(cell)->water==0,"all crops ripen safely after water runs out");
    aim(player,{2.5f,2.3f,.5f});
    check(useTarget(world,player,Item::WateringCan).kind==UseKind::Crop,"the can can also harvest a ripe crop");
    world.farm.harvest(kind)=9999;
    check(harvestCrop(world,player,cell)==FarmUse::None && cropRipe(world.get(cell)),"full produce baskets preserve the ripe crop");
    world.farm.harvest(kind)=0;
    check(harvestCrop(world,player,cell)==FarmUse::Harvested && world.farm.harvest(kind)==cropYield(kind),"harvest awards the correct type and amount of produce");
    if(kind==CropKind::Strawberry)
      check(world.get(cell)==Block::StrawberryGrowing && world.cropAt(cell)->age==52.5f && world.cropAt(cell)->water==0,"strawberry bushes flower again without replanting");
    else check(world.get(cell)==Block::Air && !world.cropAt(cell),"annual crops leave the soil ready for a new choice of seeds");
    for(auto other : cropKinds) if(other!=kind) check(world.farm.harvest(other)==0,"produce baskets remain separate");
    check(harvestCrop(world,player,cell)==FarmUse::None,"harvesting the new seedling cannot duplicate produce");
    world.set(cell,seed);
    world.set({2,2,2},Block::Stone); world.set({2,3,2},Block::Stone); aim(player,{2.5f,2.12f,.5f});
    check(!waterCrop(world,player,cell),"watering cannot pass through a wall");
    growFarm(world,200); check(harvestCrop(world,player,cell)==FarmUse::None,"harvesting cannot pass through a wall");
    world.set({2,2,2},Block::Air); world.set({2,3,2},Block::Air); world.set(cell,seed);
    player.pose.position.z=8; aim(player,{2.5f,2.12f,.5f});
    check(!waterCrop(world,player,cell),"watering obeys the same close interaction reach");
    auto view=farmView(world);
    check(view.harvest[int(kind)]==cropYield(kind) && view.plants==1 && view.ripe==0,"garden page reports separate harvests and growing plants");
    check(breakBlock(world,{2,1,0}) && !world.cropAt(cell) && world.get(cell)==Block::Air,"removing soil cleans up every crop type");
  }
  auto wet=flatWorld(),dry=flatWorld(); Cell cell{15,2,0};
  wet.set(cell,Block::PumpkinYoung); dry.set(cell,Block::PumpkinYoung);
  wet.chunks.at({0,0}).dirty=false;
  check(wet.waterCrop(cell) && wet.chunks.at({0,0}).dirty,"watering refreshes the wet-soil appearance");
  wet.chunks.at({0,0}).dirty=false;
  growFarm(wet,60); growFarm(dry,60);
  check(wet.cropAt(cell)->age==105 && dry.cropAt(cell)->age==60 && wet.cropAt(cell)->water==0,
        "a long time step awards only the remaining water bonus");
  check(wet.chunks.at({0,0}).dirty,"drying soil refreshes its mesh");
  check(wet.waterCrop(cell),"a dry plant can be watered again without a refill");
  wet.evict({50,50},1); growFarm(wet,23); wet.ensure({0,0},1);
  check(wet.get(cell)==Block::PumpkinRipe && wet.cropAt(cell)->water==22,"watered crops grow correctly while their chunk is unloaded");
  growFarm(dry,10000);
  check(dry.get(cell)==Block::PumpkinRipe && dry.cropAt(cell)->age==150,"neglected crops ripen and remain safe indefinitely");

  auto world=flatWorld(); Cell origin{2,2,0};
  for(auto kind : cropKinds) for(int stage=0;stage<3;++stage) {
    world.set(origin,cropStage(kind,cropGrowSeconds(kind)*stage*.5f));
    auto box=blockBounds(origin,world.get(origin)); auto mesh=buildMesh(world,world.chunks.at({0,0}));
    int vertices=0;
    for(const auto& v : mesh) if(v.block==glm::vec3(origin.x,origin.y,origin.z)) {
      ++vertices;
      check(glm::all(glm::greaterThanEqual(v.position,glm::vec3(origin.x,origin.y,origin.z)-.001f))
         && glm::all(glm::lessThanEqual(v.position,glm::vec3(origin.x+1,box.max.y,origin.z+1)+.001f)),"crop geometry fits its cell and selection height at every stage");
    }
    check(vertices>0 && vertices<900,"each crop stage has a small bounded mesh");
  }
}
void gardenPersistence() {
  auto dir=std::filesystem::temp_directory_path()/("blockworld-garden-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);} } cleanup{dir};
  auto path=dir/"garden.bw"; auto world=flatWorld();
  for(auto kind : cropKinds) {
    Cell c{2+int(kind),2,0}; world.set(c,cropStage(kind,0)); world.waterCrop(c); world.farm.harvest(kind)=7+int(kind);
  }
  growFarm(world,12.5f);
  world.inventory.slots={Item::WateringCan,Item::Carrot,Item::Strawberry,Item::Pumpkin,Item::Wheat,Item::Glass,Item::Door,Item::Fence,Item::Gate};
  world.inventory.selected=0;
  addChicken(world,{.5f,2,.5f}); renameChicken(world,0,"Clover"); world.farm.chickens[0].hatchTimer=21;
  Player player; player.pose.position={6,2,4}; world.save(path,player.pose);
  World loaded; check(loaded.load(path).has_value(),"current garden loads"); loaded.ensure({0,0},1);
  for(auto kind : cropKinds) {
    auto crop=loaded.cropAt({2+int(kind),2,0});
    check(crop && crop->kind==kind && crop->age==25 && crop->water==32.5f && loaded.farm.harvest(kind)==7+int(kind),
          "crop identity, partial growth, watering time and all produce baskets survive reload");
  }
  check(loaded.inventory==world.inventory && loaded.farm.chickens[0].hatchTimer==21 && loaded.farm.chickens[0].name=="Clover",
        "new garden tools coexist with existing animal progress in saves");
  growFarm(loaded,10);
  check(loaded.cropAt({2,2,0})->age==45 && loaded.cropAt({2,2,0})->water==22.5f,"watering resumes at double speed after loading");
  auto prefix=std::string("BLOCKWORLD 8 123 1\n6 2 4 0 0 0\n0\n0.5 1\n0 0 0 0\n1 0 0 0 0.5 2 0.5 0 1\n");
  for(auto record : {"0 2 0 0 -1 0","0 2 0 0 4 0","0 2 0 76 1 0","0 2 0 0 1 -1","0 2 0 0 1 46","0 2 0 0 1 nan","0 2 0 inf 1 0","0 2 0 0 2 0"}) {
    std::ofstream(path)<<prefix<<record<<"\n0 0 0\n0 1 2 3 4 5 6 0 0 14\n1\n0 2 0 33\n";
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.cropAt({2,2,0})->age==45 && loaded.inventory==world.inventory,"invalid crop types, growth, watering and block mismatches reject atomically");
  }
  for(auto baskets : {"-1 0 0","0 10000 0","0 0 10000"}) {
    std::ofstream(path)<<prefix<<"0 2 0 0 1 0\n"<<baskets<<"\n0 1 2 3 4 5 6 0 0 14\n1\n0 2 0 33\n";
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.farm.carrots==8,"invalid produce counts never mutate the loaded farm");
  }
  std::ofstream(path)<<"BLOCKWORLD 7 123 1\n6 2 4 0 0 0\n0\n0.5 1\n0 0 0 0\n1 7 2 31 0.5 2 0.5 1 1\n"
      <<"0.5 2 0.5 0 -1 0 180 -1 21 \"Clover\"\n2 2 0 60\n0 1 2 3 4 5 6 0 0 14\n1\n2 2 0 31\n";
  check(loaded.load(path).has_value(),"version seven farms still load"); loaded.ensure({0,0},1);
  check(loaded.cropAt({2,2,0})->kind==CropKind::Wheat && loaded.cropAt({2,2,0})->age==60 && loaded.cropAt({2,2,0})->water==0
        && loaded.farm.wheat==7 && loaded.farm.carrots==0 && loaded.farm.strawberries==0 && loaded.farm.pumpkins==0
        && loaded.farm.chickens[0].hatchTimer==21,"old farms preserve wheat, names and incubation while adding empty produce baskets");
}
void feedingAndEggs() {
  auto world=flatWorld(); addChicken(world,{.5f,2,.5f}); Player p; p.pose.position={.5f,2,3.3f}; aim(p,{.5f,2.5f,.5f});
  check(targetChicken(world,p)==0 && useChicken(world,p,0)==FarmUse::None,"empty-handed interaction costs no supplies");
  world.farm.wheat=2; world.set({0,2,1},Block::Stone); world.set({0,3,1},Block::Stone);
  check(!targetChicken(world,p) && useChicken(world,p,0)==FarmUse::None && world.farm.wheat==2,"cannot feed through a wall");
  world.set({0,2,1},Block::Air); world.set({0,3,1},Block::Air);
  check(useChicken(world,p,0)==FarmUse::Fed && world.farm.wheat==1,"feeding spends exactly one wheat");
  check(useChicken(world,p,0)==FarmUse::None && world.farm.wheat==1,"repeated feeding cannot waste supplies during the egg timer");
  growFarm(world,29); check(!world.farm.chickens.front().eggReady,"egg takes time to appear");
  growFarm(world,1); check(world.farm.chickens.front().eggReady,"a fed chicken lays an egg");
  check(useChicken(world,p,0)==FarmUse::Collected && world.farm.eggs==1,"use collects one egg");
  check(useChicken(world,p,0)==FarmUse::Fed && world.farm.eggs==1 && world.farm.wheat==0,"next interaction feeds the next cycle without duplicating an egg");
  growFarm(world,500); world.farm.eggs=9999;
  check(useChicken(world,p,0)==FarmUse::None && world.farm.chickens.front().eggReady,"full egg basket preserves the egg");
  world.farm.eggs=0; world.clock.phase=0;
  check(useChicken(world,p,0)==FarmUse::Collected,"a ready egg can be collected while its chicken rests");
  world.farm.wheat=1;
  check(useChicken(world,p,0)==FarmUse::None && world.farm.wheat==1,"sleeping chickens wait until morning to eat");
}
void completeFarmGuide() {
  World world(7262026,true); world.ensure({0,0},3); initializeFarm(world); initializeFarm(world);
  check(world.farm.chickens.size()==4,"four chickens spawn once, including in an old world");
  Player player; player.pose.position={3.5f,24,13.5f};
  check(farmGuide(world,player).stage==0,"farm guide starts with a pen");
  for(glm::vec3 position : {glm::vec3(3.5f,24,13.5f),{8.5f,24,8.5f},{3.5f,24,3.5f},{-1.5f,24,8.5f}}) {
    player.pose.position=position;
    for(int i=0;i<30 && buildPenNext(world,player);++i) {}
  }
  check(penComplete(world),"assisted guide can finish the entire pen from walking positions");
  tickFarm(world,player,.01f,false); check(farmGuide(world,player).stage==1,"finished pen unlocks planting step");
  Cell crop{7,24,8}; world.set(crop+Cell{0,-1,0},Block::Farmland);
  check(placeBlock(world,player,crop,Block::WheatYoung),"plant the farm garden");
  check(farmGuide(world,player).stage==2,"guide teaches waiting for golden wheat");
  growFarm(world,90); player.pose.position={7.5f,24,10.5f}; aim(player,{7.5f,24.5f,8.5f});
  check(harvestWheat(world,player,crop)==FarmUse::Harvested && farmGuide(world,player).stage==3,"harvest unlocks feeding lesson");
  auto& chicken=world.farm.chickens.front(); player.pose.position=chicken.position+glm::vec3(0,0,1.8f); aim(player,chicken.position+glm::vec3(0,.5f,0));
  auto target=targetChicken(world,player); check(target.has_value(),"guided flock is accessible for feeding");
  check(useChicken(world,player,*target)==FarmUse::Fed && farmGuide(world,player).stage==4,"feeding advances guide");
  growFarm(world,30);
  check(useChicken(world,player,*target)==FarmUse::Collected && farmGuide(world,player).stage==5 && world.farm.flags==31,"first collected egg unlocks hatching");
  check(incubateEgg(world,*target) && farmGuide(world,player).stage==6,"giving an egg to a mother advances the guide");
  growFarm(world,eggHatchSeconds);
  check(world.farm.chickens.size()==5 && farmGuide(world,player).stage==7,"hatching advances to naming the new chick");
  check(renameChicken(world,4,"Pip") && farmGuide(world,player).stage==8,"naming completes the extended farming lesson");
}
void namesAndChicks() {
  auto world=flatWorld(); addChicken(world,{.5f,2,.5f}); Player p; p.pose.position={-8,2,-8};
  check(animalName(world.farm.chickens[0],0)=="Clover","unnamed older animals receive friendly default names");
  check(renameChicken(world,0,"  Mama Peep  ") && world.farm.chickens[0].name=="Mama Peep","names trim surrounding spaces");
  for(auto name : {"", "   ", "abcdefghijklmnopqrs", "Bad\nName", "Bad\tName", "<tag>", "...", "\xc3\xa9"})
    check(!renameChicken(world,0,name) && world.farm.chickens[0].name=="Mama Peep","unsupported names leave the old name intact");
  check(!renameChicken(world,42,"Pip") && renameChicken(world,0,"Mom's Peep-2"),"names support apostrophes and hyphens with valid targets");
  check(!incubateEgg(world,0) && world.farm.eggs==0,"hatching requires a collected egg");
  world.farm.eggs=3;
  check(incubateEgg(world,0) && world.farm.eggs==2 && world.farm.chickens[0].hatchTimer==60,"starting incubation spends exactly one egg");
  check(!incubateEgg(world,0) && !incubateEgg(world,42) && world.farm.eggs==2,"repeat and invalid requests never spend extra eggs");
  auto position=world.farm.chickens[0].position; world.farm.wheat=5;
  for(int i=0;i<100;++i) tickFarm(world,p,.1f,true);
  check(glm::length(world.farm.chickens[0].position-position)<.01f,"incubating hen stays with her egg");
  growFarm(world,49); check(world.farm.chickens.size()==1,"eggs do not hatch before their timer finishes");
  growFarm(world,1.01f);
  check(world.farm.chickens.size()==2 && world.farm.chickens[1].mother==0 && isChick(world.farm.chickens[1])
        && chickenScale(world.farm.chickens[1])<.56f && world.farm.chickens[0].hatchTimer==-1,"a small chick hatches beside its mother");
  check(world.farm.eggs==2 && !incubateEgg(world,1),"chicks cannot incubate eggs or consume the egg basket");
  auto babyPosition=world.farm.chickens[1].position;
  p.pose.position=babyPosition+glm::vec3(0,0,1.6f); aim(p,babyPosition+glm::vec3(0,.3f,0));
  check(targetChicken(world,p)==1 && useChicken(world,p,1)==FarmUse::Petted && world.farm.wheat==5,
        "small chicks can be targeted and petted without wasting wheat");
  world.farm.chickens[0].position={4.5f,2,.5f}; p.pose.position={-8,2,-8};
  float before=glm::length(world.farm.chickens[1].position-world.farm.chickens[0].position);
  for(int i=0;i<40;++i) tickFarm(world,p,.1f,false);
  check(glm::length(world.farm.chickens[1].position-world.farm.chickens[0].position)<before-1,"a chick follows its mother without wheat");
  auto view=farmView(world);
  check(view.animals[1].baby && view.animals[1].mother=="Mom's Peep-2" && !view.animals[1].hatchProblem.empty(),"farm page reports the chick and its mother");
  growFarm(world,90); check(isChick(world.farm.chickens[1]) && chickenScale(world.farm.chickens[1])>.75f,"chicks grow visibly before adulthood");
  growFarm(world,90); check(!isChick(world.farm.chickens[1]) && chickenScale(world.farm.chickens[1])==1,"three minutes of play grows a chick into an adult");
  // Feeding needs a clear ray; wandering positions are not part of this assertion.
  world.farm.chickens[0].position={-8,2,-8};
  p.pose.position=world.farm.chickens[1].position+glm::vec3(0,0,1.6f); aim(p,world.farm.chickens[1].position+glm::vec3(0,.5f,0));
  check(useChicken(world,p,1)==FarmUse::Fed,"grown chicks join the normal feeding and egg cycle");

  world=flatWorld(); addChicken(world,{.5f,2,.5f}); world.farm.eggs=1; incubateEgg(world,0);
  for(int z=-2;z<=2;++z) for(int x=-2;x<=2;++x) if(x!=0 || z!=0)
    for(int y=2;y<=4;++y) world.set({x,y,z},Block::Stone);
  growFarm(world,90);
  check(world.farm.chickens.size()==1 && world.farm.chickens[0].hatchTimer==0,"a ready egg waits when every hatching spot is blocked");
  for(int y=2;y<=4;++y) world.set({0,y,1},Block::Air);
  growFarm(world,.1f);
  check(world.farm.chickens.size()==2 && world.farm.chickens[1].position.z>1,"clearing a safe nearby patch lets the waiting egg hatch");
  growFarm(world,600); check(world.farm.chickens.size()==2,"one egg hatches once even after a large time skip");

  world=flatWorld(); addChicken(world,{.5f,2,.5f}); world.farm.eggs=1; incubateEgg(world,0);
  auto chunks=world.chunks; world.chunks.clear(); growFarm(world,120);
  check(world.farm.chickens.size()==1 && world.farm.chickens[0].hatchTimer==0,"unloaded ground cannot create a chick in unseen geometry");
  world.chunks=std::move(chunks); growFarm(world,.1f);
  check(world.farm.chickens.size()==2,"returning to the loaded farm finishes a ready hatch");

  world=flatWorld();
  pen(world); addChicken(world,{.5f,2,.5f}); world.farm.eggs=1; incubateEgg(world,0); growFarm(world,60);
  check(world.farm.chickens.size()==2,"pen has room to hatch a chick");
  p.pose.position={.5f,2,6.5f}; world.farm.wheat=3;
  for(int i=0;i<2100;++i) {
    tickFarm(world,p,.1f,true);
    for(const auto& c : world.farm.chickens)
      check(c.position.x>-2 && c.position.x<3 && c.position.z>-2 && c.position.z<3.3f
            && std::abs(c.position.y-2)<.01f,"closed pen contains the family through following and growth");
  }
  check(!isChick(world.farm.chickens[1]),"a contained chick grows into a hen");

  world=flatWorld();
  for(int i=0;i<11;++i) addChicken(world,{float((i%4)*2)+.5f,2,float((i/4)*2)+.5f});
  world.farm.eggs=5;
  check(incubateEgg(world,0) && !incubateEgg(world,1) && world.farm.eggs==4,"incubating eggs reserve the last flock slot");
  growFarm(world,70);
  check(world.farm.chickens.size()==flockLimit && !incubateEgg(world,1),"flock capacity stops safely at twelve birds");
  for(auto& c : world.farm.chickens) { c.happy=1; c.eggReady=!isChick(c); }
  check(chickenMesh(world).size()<=chickenVertexLimit,"the largest happy flock fits the GPU allocation");
}
void persistence() {
  auto dir=std::filesystem::temp_directory_path()/("blockworld-farm-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);} } cleanup{dir};
  auto path=dir/"farm.bw";
  auto world=flatWorld(); pen(world); addChicken(world,{.5f,2,.5f}); world.farm.chickens.front().eggTimer=17.5f;
  world.set({5,2,0},Block::WheatYoung); growFarm(world,12.25f); world.crafting={2,3,4,127};
  world.farm.wheat=7; world.farm.eggs=2; world.farm.flags=31; world.clock.day=12; world.clock.phase=.81;
  Player p; p.pose.position={6,2,4}; world.save(path,p.pose);
  World loaded; check(loaded.load(path).has_value(),"farm save loads"); loaded.ensure({0,0},1);
  check(loaded.farm.initialized && loaded.farm.chickens.size()==1 && loaded.farm.chickens.front().eggTimer==5.25f
        && loaded.farm.chickens.front().position==world.farm.chickens.front().position && loaded.farm.crops.front().age==12.25f,
        "animals, positions, egg timers and partial crop growth survive reload");
  initializeFarm(loaded); check(loaded.farm.chickens.size()==1,"saved flock is not spawned again");
  check(loaded.farm.wheat==7 && loaded.farm.eggs==2 && loaded.farm.flags==31 && loaded.crafting.flags==127
        && loaded.clock.day==12 && loaded.clock.phase==.81 && penComplete(loaded),"farm, tools, clock and fences persist together");
  auto prefix=std::string("BLOCKWORLD 5 123 1\n6 2 4 0 0 0\n0\n0.5 1\n0 0 0 0\n");
  for(auto data : {
       "1 -1 0 0 0.5 2 0.5 0 0\n0\n", "1 0 10000 0 0.5 2 0.5 0 0\n0\n",
       "1 0 0 32 0.5 2 0.5 0 0\n0\n", "1 0 0 0 0.5 2 0.5 5 0\n0\n",
       "1 0 0 0 0.5 2 0.5 0 257\n0\n", "1 0 0 0 0.5 2 0.5 1 0\n0.5 2 0.5 0 31 0\n0\n",
       "1 0 0 0 0.5 2 0.5 1 0\n0.5 2 0.5 0 10 1\n0\n",
       "1 0 0 0 0.5 2 0.5 0 1\n0 2 0 91\n1\n0 2 0 32\n",
       "1 0 0 0 0.5 2 0.5 0 1\n0 2 0 0\n1\n0 2 0 32\n",
       "1 0 0 0 0.5 2 0.5 0 2\n0 2 0 0\n0 2 0 0\n1\n0 2 0 30\n"}) {
    std::ofstream(path)<<prefix<<data;
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.farm.eggs==2 && loaded.farm.crops.front().age==12.25f && loaded.clock.day==12,"bad farm data is rejected without mutating the world");
  }
  std::ofstream(path)<<"BLOCKWORLD 4 123 1\n6 25 4 0 0 0\n127\n0.81 12\n2 3 4 127\n1\n1 25 1 24\n";
  check(loaded.load(path).has_value(),"previous version four still loads"); loaded.ensure({0,0},1);
  check(loaded.crafting.flags==127 && loaded.get({1,25,1})==Block::Workbench && loaded.clock.day==12
        && !loaded.farm.initialized && loaded.farm.chickens.empty(),"old workbenches, tools and clocks retain their meanings");
  initializeFarm(loaded); check(loaded.farm.chickens.size()==4 && loaded.farm.initialized,"old save receives its first four chickens");
}
void chickPersistence() {
  auto dir=std::filesystem::temp_directory_path()/("blockworld-chicks-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);} } cleanup{dir};
  auto path=dir/"farm.bw";
  World world(7262026,true); world.ensure({0,0},2); initializeFarm(world);
  world.farm.eggs=5; check(incubateEgg(world,0),"fixture egg starts warming"); growFarm(world,60);
  check(world.farm.chickens.size()==5,"fixture chick hatches");
  renameChicken(world,0,"Mama Peep"); renameChicken(world,4,"Tiny Peep"); world.farm.chickens[4].growth=72;
  check(incubateEgg(world,1),"a second hen can warm another egg"); world.farm.chickens[1].hatchTimer=21;
  world.farm.flags=255; world.crafting={8,164,18,127}; world.clock.day=6;
  world.inventory=startingInventory(world.crafting); world.inventory.selected=7;
  Player p; p.pose.position={10.5f,24,7.5f}; world.save(path,p.pose);
  World loaded; check(loaded.load(path).has_value(),"current family save loads"); loaded.ensure({0,0},2);
  check(loaded.farm.chickens.size()==5 && loaded.farm.chickens[0].name=="Mama Peep"
        && loaded.farm.chickens[4].name=="Tiny Peep" && loaded.farm.chickens[4].mother==0
        && loaded.farm.chickens[4].growth==72 && loaded.farm.chickens[1].hatchTimer==21
        && loaded.inventory==world.inventory && loaded.crafting.planks==164 && loaded.clock.day==6,
        "names, parent relationships, growth, incubation, old progress, and hotbar survive reload");
  growFarm(loaded,30);
  check(loaded.farm.chickens.size()==6 && loaded.farm.chickens[4].growth==102 && loaded.farm.chickens[5].growth==9,
        "saved incubation resumes and a time skip ages the new chick only after hatching");
  auto prefix=std::string("BLOCKWORLD 7 123 1\n6 24 4 0 0 0\n0\n0.5 1\n0 0 0 0\n1 0 0 0 0.5 24 0.5 1 0\n");
  for(auto record : {
      "0.5 24 0.5 0 -1 0 181 -1 -1 \"Clover\"",
      "0.5 24 0.5 0 -1 0 180 -1 -0.5 \"Clover\"",
      "0.5 24 0.5 0 -1 0 180 -1 61 \"Clover\"",
      "0.5 24 0.5 0 -1 0 180 0 -1 \"Clover\"",
      "0.5 24 0.5 0 -1 0 0 -1 -1 \"Clover\"",
      "0.5 24 0.5 0 -1 0 -1 -1 -1 \"Clover\"",
      "0.5 24 0.5 0 -1 0 180 -2 -1 \"Clover\"",
      "0.5 24 0.5 0 -1 0 180 -1 -1 \"\"",
      "0.5 24 0.5 0 -1 0 180 -1 -1 \"Bad\tName\"",
      "0.5 24 0.5 0 -1 0 180 -1 -1 \"abcdefghijklmnopqrs\""}) {
    std::ofstream(path)<<prefix<<record<<"\n0 1 2 3 4 5 6 0 0 14\n0\n";
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.farm.chickens.size()==6 && loaded.farm.chickens[4].name=="Tiny Peep"
          && loaded.inventory==world.inventory && loaded.clock.day==6,"bad family data is rejected without changing loaded progress");
  }
  std::ofstream old(path);
  old<<"BLOCKWORLD 6 123 1\n6 24 4 0 0 0\n127\n0.8 6\n8 164 18 127\n1 7 2 31 3.5 24 8.5 1 0\n"
     <<"3.5 24 8.5 0 12 0\n6 1 2 3 4 5 6 15 16 14\n0\n";
  old.close();
  check(loaded.load(path).has_value() && loaded.farm.chickens[0].name=="Clover" && !isChick(loaded.farm.chickens[0])
        && loaded.farm.chickens[0].hatchTimer==-1 && loaded.farm.chickens[0].eggTimer==12
        && loaded.inventory.held()==Item::Axe && loaded.farm.eggs==2 && loaded.crafting.planks==164,
        "version six migrates adult names while preserving the original farm and chosen tool");
}
glm::vec3 color(int material) {
  switch(material) {
    case 1: case 10:return {.40f,.59f,.25f}; case 2:return {.48f,.32f,.20f}; case 3:return {.53f,.55f,.55f};
    case 4:return {.83f,.77f,.53f}; case 5:return {.44f,.29f,.15f}; case 7:return {.69f,.49f,.28f};
    case 12:return {.65f,.83f,.87f}; case 13:return {.57f,.37f,.20f};
    case 24:return {.37f,.65f,.19f}; case 25:return {.92f,.72f,.25f}; case 26:return {.97f,.94f,.83f};
    case 27:return {.99f,.59f,.14f}; case 28:return {.85f,.17f,.12f}; case 29:return {.055f,.065f,.05f};
    case 30:return {.80f,.54f,.28f}; case 31:return {1.f,.94f,.75f}; case 32:return {1.f,.36f,.47f};
    case 33:return {.27f,.18f,.12f}; case 34:return {.97f,.48f,.10f}; case 35:return {.88f,.16f,.26f};
    case 36:return {.96f,.45f,.08f}; case 37:return {.38f,.79f,1.f}; case 38:return {1.f,.95f,.81f}; case 39:return {.29f,.76f,.24f};
    case 40:return {.43f,.28f,.17f}; case 41:return {.25f,.15f,.085f}; case 42:return {.25f,.59f,.67f};
    default:return {.5f,.5f,.5f};
  }
}
// A CPU geometry preview, not a substitute for the real Metal smoke test.
void preview(const std::filesystem::path& path,bool upgraded) {
  World world(7262026,true); world.ensure({0,0},2);
  check(initializeGarden(world),"preview starts with the real vegetable garden");
  if(upgraded) {
    for(int z=8;z<=10;++z) for(int x=17;x<=23;++x) if(x!=20) {
      auto kind=x<20 ? CropKind::Carrot : CropKind::Strawberry;
      Cell cell{x,24,z}; world.set(cell,cropStage(kind,cropGrowSeconds(kind)*(z==10 ? .5f : 1.f)));
      if(z==10) world.compostCrop(cell);
      world.waterCrop(cell);
    }
    world.set({18,24,13},Block::PumpkinRipe);
    for(int z=12;z<=14;++z) for(int x=21;x<=23;++x) {
      world.set({x,23,z},Block::Farmland); world.set({x,24,z},Block::WheatRipe);
    }
    Player builder; builder.pose.position={20.5f,24,15.5f};
    world.farm.garden.sprinklers=1;
    check(placeBlock(world,builder,{20,24,10},Block::Sprinkler),"preview sprinkler can be placed");
    builder.pose.position={11.5f,24,18.2f}; auto direction=glm::normalize(glm::vec3(11.5f,23.99f,15.5f)-builder.eye());
    builder.pose.yaw=std::atan2(direction.x,-direction.z); builder.pose.pitch=std::asin(direction.y);
    equipItem(world.inventory,world.crafting,Item::Greenhouse); world.farm.garden.greenhouses=1;
    check(placeGreenhouse(world,builder,{11,24,15}),"preview greenhouse fits beside the beds");
    for(int z=12;z<=14;++z) for(int x=10;x<=12;++x) world.set({x,24,z},Block::StrawberryRipe);
  }
  std::vector<Vertex> mesh;
  for(const auto& [pos,chunk] : world.chunks) {
    auto terrain=buildMesh(world,chunk);
    for(std::size_t i=0;i<terrain.size();i+=3) {
      auto block=terrain[i].block;
      if(block.x>=(upgraded ? 8 : 14) && block.x<=26 && block.z>=5 && block.z<=17 && block.y>=22)
        mesh.insert(mesh.end(),terrain.begin()+std::ptrdiff_t(i),terrain.begin()+std::ptrdiff_t(i+3));
    }
  }
  constexpr int w=1000,h=750;
  std::vector<glm::vec3> pixels(w*h,{.69f,.80f,.81f}); std::vector<float> depth(w*h,1.f);
  auto camera=upgraded ? glm::vec3(32,39,32) : glm::vec3(30,35,26);
  auto target=upgraded ? glm::vec3(17,24,11) : glm::vec3(20,24,11);
  auto matrix=glm::perspective(glm::radians(49.f),float(w)/h,.1f,70.f)*glm::lookAt(camera,target,glm::vec3(0,1,0));
  auto edge=[](glm::vec2 a,glm::vec2 b,glm::vec2 p){return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);};
  for(std::size_t i=0;i<mesh.size();i+=3) {
    std::array<glm::vec3,3> screen; bool clipped=false;
    for(int j=0;j<3;++j) { auto clip=matrix*glm::vec4(mesh[i+j].position,1); if(clip.w<=0) clipped=true;
      screen[j]={(clip.x/clip.w*.5f+.5f)*w,(.5f-clip.y/clip.w*.5f)*h,clip.z/clip.w}; }
    if(clipped) continue;
    auto a=screen[0],b=screen[1],c=screen[2]; float area=edge(a,b,c); if(std::abs(area)<1e-5f) continue;
    auto lo=glm::min(glm::min(a,b),c),hi=glm::max(glm::max(a,b),c);
    for(int y=std::max(0,int(std::floor(lo.y)));y<std::min(h,int(std::ceil(hi.y)));++y)
      for(int x=std::max(0,int(std::floor(lo.x)));x<std::min(w,int(std::ceil(hi.x)));++x) {
        glm::vec2 p{x+.5f,y+.5f}; float u=edge(b,c,p)/area,v=edge(c,a,p)/area,t=1-u-v;
        if(u<0 || v<0 || t<0) continue;
        float z=u*a.z+v*b.z+t*c.z; int index=y*w+x;
        if(z<0 || z>=depth[index]) continue;
        if(int(mesh[i].material)==12) {
          auto uv=u*mesh[i].uv+v*mesh[i+1].uv+t*mesh[i+2].uv;
          float rim=std::min({uv.x,uv.y,1-uv.x,1-uv.y});
          if(rim>.055f && std::abs(uv.x-uv.y-.28f)>.024f && std::abs(uv.x-uv.y+.30f)>.015f) continue;
        }
        depth[index]=z; pixels[index]=color(int(mesh[i].material))*(u*mesh[i].light+v*mesh[i+1].light+t*mesh[i+2].light);
      }
  }
  std::filesystem::create_directories(path.parent_path()); std::ofstream out(path,std::ios::binary);
  out<<"P6\n"<<w<<' '<<h<<"\n255\n";
  for(auto pixel : pixels) for(int c=0;c<3;++c) out.put(char(std::clamp(int(pixel[c]*255),0,255)));
  check(bool(out),"farm preview written");
}
}
int main(int argc,char** argv) {
  try {
    for(auto [name,test] : {std::pair{"wheat planting and growth",&planting},{"flock containment and gates",flockAndGates},
                           {"chickens do not spin when blocked",chickenTurning},{"chickens climb out of holes",chickenStepsOutOfHole},
                           {"mixed garden and watering",gardening},{"garden saves and migration",gardenPersistence},
                           {"feeding and eggs",feedingAndEggs},{"complete farm guide",completeFarmGuide},{"names and baby chicks",namesAndChicks},{"farm persistence and migration",persistence},{"family save migration",chickPersistence}}) {
      test(); std::cout<<"PASS "<<name<<'\n';
    }
    if(argc>=2) preview(argv[1],argc>=3 && std::string_view(argv[2])=="upgraded");
  } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

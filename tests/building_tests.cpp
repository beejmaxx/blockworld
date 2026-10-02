#include "building.hpp"
#include "farm.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
World emptyWorld() {
  World world;
  for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) world.insert(Chunk{{x,z}});
  return world;
}
RayHit aim(Cell cell,float distance=2) { return {cell,cell+Cell{0,1,0},distance}; }
BreakEvent mine(World& world,Mining& mining,Cell cell) {
  for(int i=0;i<100;++i) if(auto event=mining.tick(world,aim(cell),true,1.f/60.f)) return *event;
  throw std::runtime_error("mining did not finish within the expected time");
}
void inputAndMining() {
  BuildRepeater clicks;
  clicks.press(); clicks.release();
  check(clicks.tick(.016f,true) && !clicks.tick(.016f,true),"a quick down/up click places exactly once");
  clicks.press(); check(clicks.tick(.016f,true),"press places immediately");
  for(int i=0;i<3;++i) check(!clicks.tick(.1f,true),"holding does not repeat before the initial delay");
  check(clicks.tick(.1f,true),"holding repeats after the initial delay");
  check(!clicks.tick(.1f,true) && clicks.tick(.1f,true),"held placement repeats at a controlled rate");
  clicks.release(); check(!clicks.tick(.1f,true),"release stops repetition");
  clicks.press(); check(!clicks.tick(.1f,false) && !clicks.tick(.1f,true),"menu/pause cancels pending placement across resume");
  clicks.press(); clicks.cancel(); check(!clicks.tick(.1f,true),"resume click cannot build");

  auto world=emptyWorld(); Mining mining;
  Cell a{0,2,0},b{1,2,0}; world.set(a,Block::Wood); world.set(b,Block::Wood);
  check(!mining.tick(world,aim(a),true,.1f) && mining.progress()>0,"breaking has visible progress before removal");
  mining.tick(world,aim(a),true,.1f);
  auto progress=mining.progress(); mining.tick(world,aim(b),true,.1f);
  check(mining.target()==b && mining.progress()<progress,"changing target resets damage");
  mining.tick(world,aim(b),false,.1f);
  check(!mining.target() && mining.progress()==0 && world.get(b)==Block::Wood,"releasing X leaves the block intact");
  mine(world,mining,a);
  check(world.get(a)==Block::Air && world.crafting.wood==1 && (world.guideFlags&Broke),"finished mining removes and collects one block");
  for(int i=0;i<20;++i) mining.tick(world,aim(a),true,.1f);
  check(world.crafting.wood==1,"holding on an empty cell cannot duplicate supplies");
  for(float reach : {7.01f,-1.f,std::numeric_limits<float>::quiet_NaN()})
    check(!mining.tick(world,aim(b,reach),true,.1f) && !mining.target(),"invalid or distant targets cannot be mined");
  world.chunks.at({0,0}).set(0,1,0,Block::Bedrock);
  check(!mining.tick(world,aim({0,1,0}),true,.1f) && !mining.target(),"bedrock is unbreakable");
  mining.tick(world,aim(b),true,std::numeric_limits<float>::quiet_NaN());
  check(mining.progress()==0,"invalid elapsed time cannot finish a block");
  world.crafting.flags|=MadeAxe;
  for(int i=0;i<4;++i) check(!mining.tick(world,aim(b),true,.1f,Tool::Axe),"axe still requires a deliberate hold");
  check(mining.tick(world,aim(b),true,.1f,Tool::Axe).has_value(),"equipped axe completes wood sooner than bare hands");
  world.set({3,1,0},Block::Stone); Player p; p.pose.position={0,2,4};
  check(placeBlock(world,p,{3,2,0},Block::DoorZ),"door fixture placed");
  mine(world,mining,{3,3,0});
  check(world.get({3,2,0})==Block::Air && world.get({3,3,0})==Block::Air,"mining upper door removes both halves");
}
void accidentalRemoval() {
  auto world=emptyWorld(); Mining mining; Cell cell{0,2,0};
  world.crafting.flags=MadeAxe|MadePickaxe;
  for(auto tool : {Tool::Hands,Tool::Axe,Tool::Pickaxe}) for(int id=1;id<int(Block::Count);++id) {
    auto block=Block(id); if(block==Block::Bedrock) continue;
    check(world.set(cell,block),"quick-click fixture can be placed"); mining.reset();
    for(int click=0;click<8;++click) {
      check(!mining.tick(world,aim(cell),true,.1f,tool),"a quick click cannot remove any block with any tool");
      mining.tick(world,aim(cell),false,0,tool);
    }
    check(world.get(cell)==block && mining.progress()==0,"separate taps never accumulate breaking progress");
    if(block!=Block::Bedrock) check(breakSeconds(world.crafting,block,tool)>=.5f,"all removable objects require at least half a second");
    world.set(cell,Block::Air);
  }
  world.set(cell,Block::Glass);
  for(int i=0;i<4;++i) mining.tick(world,aim(cell),true,.1f);
  mining.reset(); // Menu, focus loss, hotbar change, or teleport cancels a held action.
  check(!mining.tick(world,aim(cell),true,.1f) && world.get(cell)==Block::Glass,"canceled progress cannot finish a block on resume");
  mining.reset();
  for(int i=0;i<4;++i) mining.tick(world,aim(cell),true,.1f);
  world.set(cell,Block::Stone);
  check(!mining.tick(world,aim(cell),true,.1f) && mining.progress()<.1f,"replacement blocks do not inherit breaking progress");

  world=emptyWorld(); mining.reset();
  for(int z=-3;z<=4;++z) for(int x=-2;x<=2;++x) world.set({x,1,z},Block::Grass);
  Chicken chicken; chicken.position={.5f,2,.5f}; world.farm.chickens.push_back(chicken);
  Player player; player.pose.position={.5f,2,3.3f};
  auto direction=glm::normalize(glm::vec3(.5f,2.4f,.5f)-player.eye());
  player.pose.yaw=std::atan2(direction.x,-direction.z); player.pose.pitch=std::asin(direction.y);
  auto floor=world.raycast(player.eye(),player.direction());
  check(floor && targetChicken(world,player,7.f)==0 && !miningTarget(world,player),"aiming at a chicken blocks digging through it into the ground");
  for(int i=0;i<30;++i) mining.tick(world,miningTarget(world,player),true,.1f);
  check(world.get(floor->block)==Block::Grass && !mining.target(),"even holding on a chicken leaves the ground intact");
  world.set({0,2,2},Block::Stone); world.set({0,3,2},Block::Stone);
  check(miningTarget(world,player).has_value(),"a chicken behind a wall does not prevent mining that wall");
}
void craftingLesson() {
  auto world=emptyWorld(); Player p; p.pose.position={.5f,2,3.5f}; Mining mining;
  check(craftLesson(world.crafting)==0 && !craft(world,p,Recipe::Planks),"empty bag starts with collecting logs");
  for(int x=-2;x<=0;++x) { world.set({x,2,0},Block::Wood); mine(world,mining,{x,2,0}); }
  check(world.crafting.wood==3 && craftLesson(world.crafting)==1,"three logs complete gathering");
  check(craft(world,p,Recipe::Planks) && world.crafting.wood==2 && world.crafting.planks==4,"one log becomes four planks");
  check(craftLesson(world.crafting)==2 && craft(world,p,Recipe::Workbench),"planks unlock the workbench recipe");
  check(world.crafting.planks==0 && craftLesson(world.crafting)==3,"workbench spends exactly four planks");
  check(!craft(world,p,Recipe::Workbench),"workbench unlock cannot be bought twice");
  world.set({0,1,1},Block::Stone);
  check(placeBlock(world,p,{0,2,1},Block::Workbench) && nearbyWorkbench(world,p),"crafted bench can be placed and used nearby");
  check(craftLesson(world.crafting)==4,"placing a bench advances the lesson");
  check(craft(world,p,Recipe::Planks) && craft(world,p,Recipe::Axe),"nearby bench enables the axe recipe");
  check(world.crafting.planks==1 && craftLesson(world.crafting)==5,"axe spends three planks");
  for(int x=-2;x<=0;++x) { world.set({x,2,0},Block::Stone); mine(world,mining,{x,2,0}); }
  check(world.crafting.stone==3 && craftLesson(world.crafting)==6,"stone collection advances the lesson");
  check(craft(world,p,Recipe::Planks),"remaining log supplies the pickaxe handle");
  for(int y=2;y<=4;++y) for(int x=-1;x<=1;++x) world.set({x,y,2},Block::Stone);
  check(!nearbyWorkbench(world,p) && !craft(world,p,Recipe::Pickaxe),"a wall blocks workbench access");
  check(world.crafting.stone==3 && world.crafting.planks==5,"failed recipe leaves supplies intact");
  for(int y=2;y<=4;++y) for(int x=-1;x<=1;++x) world.set({x,y,2},Block::Air);
  auto saved=p.pose.position; p.pose.position={.5f,2,10};
  check(!craft(world,p,Recipe::Pickaxe),"distant workbench does not enable tools"); p.pose.position=saved;
  check(craft(world,p,Recipe::Pickaxe) && craftLesson(world.crafting)==7,"pickaxe completes the lesson");
  check(world.crafting.planks==3 && world.crafting.stone==0 && world.crafting.flags==127,"recipe costs and milestones agree");
  check(!craft(world,p,Recipe::Pickaxe) && !craft(world,p,Recipe(-1)) && !craft(world,p,Recipe::Count),"owned or invalid recipes do not change supplies");
  check(bestTool(world.crafting,Block::Wood)==Tool::Axe && bestTool(world.crafting,Block::Stone)==Tool::Pickaxe
        && bestTool(world.crafting,Block::Glass)==Tool::Hands,"tools match the materials they work on");
  check(breakSeconds(world.crafting,Block::Stone,Tool::Pickaxe)<breakSeconds({},Block::Stone),"equipped pickaxe mines faster");
  check(breakSeconds(world.crafting,Block::Wood,Tool::Hands)==breakSeconds({},Block::Wood)
        && breakSeconds(world.crafting,Block::Stone,Tool::Axe)==breakSeconds({},Block::Stone)
        && breakSeconds({},Block::Wood,Tool::Axe)==breakSeconds({},Block::Wood),"unequipped, unsuitable, and uncrafted tools cannot grant a bonus");
  check(placeBlock(world,p,{4,2,0},Block::Planks) && world.crafting.planks==3,"creative building never consumes recipe supplies");
  check(breakBlock(world,{0,2,1}) && placeBlock(world,p,{0,2,1},Block::Workbench),"unlocked workbench can be replaced without recrafting");
  world.crafting.wood=9999; world.crafting.stone=9999; world.crafting.planks=9999;
  collectMaterial(world,Block::Wood); collectMaterial(world,Block::Stone); collectMaterial(world,Block::Planks);
  check(world.crafting.wood==9999 && world.crafting.stone==9999 && world.crafting.planks==9999
        && !craft(world,p,Recipe::Planks),"full storage cannot overflow or consume a log");
}
void previewsAndDebris() {
  auto world=emptyWorld(); Player p; p.pose.position={.5f,2,3.5f}; p.pose.yaw=0; p.pose.pitch=0;
  auto hit=aim({0,1,0}); world.set(hit.block,Block::Stone);
  auto preview=placementPreview(world,p,hit,Block::DoorZ);
  check(preview && preview->block==Block::DoorZ && preview->status==PlacementStatus::Ready,"door preview follows player orientation");
  p.pose.yaw=1.5707963f; preview=placementPreview(world,p,hit,Block::DoorZ);
  check(preview->block==Block::DoorX && placeBlock(world,p,preview->cell,Block::DoorZ,true)
        && world.get(preview->cell)==preview->block,"placement and preview use identical orientation");
  check(placementPreview(world,p,hit,Block::Stone)->status==PlacementStatus::Occupied,"occupied space is invalid");
  breakBlock(world,preview->cell);
  p.pose.position={.5f,2,.5f};
  check(placementPreview(world,p,hit,Block::Stone)->status==PlacementStatus::PlayerOverlap,"preview rejects player overlap");
  p.pose.position={.5f,2,3.5f};
  check(placementPreview(world,p,hit,Block::BedZ)->status==PlacementStatus::NeedsFloor,"both bed halves need floor support");
  world.set({1,1,0},Block::Stone);
  preview=placementPreview(world,p,hit,Block::BedZ);
  check(preview->block==Block::BedX && preview->status==PlacementStatus::Ready,"bed preview spans the facing axis");
  world.set({1,2,0},Block::Stone);
  check(placementPreview(world,p,hit,Block::BedZ)->status==PlacementStatus::NoRoom,"blocked pillow invalidates the whole bed");
  check(placementPreview(world,p,hit,Block::Workbench)->status==PlacementStatus::WorkbenchLocked,"bench preview explains its crafting unlock");
  check(!placementPreview(world,p,aim(hit.block,8),Block::Stone) && !placementPreview(world,p,{},Block::Stone),"preview requires a target within reach");
  for(auto block : {Block::Stone,Block::DoorX,Block::BedX,Block::Torch}) {
    auto vertices=buildPreview({0,2,0},block,22,true);
    check(vertices.size()==36,"combined placement preview fits its fixed GPU buffer");
    glm::vec3 lo(1000),hi(-1000);
    for(const auto& vertex : vertices) { lo=glm::min(lo,vertex.position); hi=glm::max(hi,vertex.position); }
    if(block==Block::BedX) check(hi.x>2 && hi.z<1.02f,"bed outline includes both halves");
    if(block==Block::DoorX) check(hi.y-lo.y>2 && hi.x-lo.x<.21f,"door outline matches its thin tall shape");
    if(block==Block::Torch) check(hi.x-lo.x<.3f,"torch outline follows its actual bounds");
  }
  DebrisCloud debris;
  for(int i=0;i<20;++i) debris.emit({0,2,0},Block::Wood);
  check(debris.size()==DebrisCloud::capacity,"debris remains bounded during rapid edits");
  auto vertices=debris.mesh({1,0,0},{0,1,0});
  check(vertices.size()==debris.size()*6,"debris upload fits its allocated buffer");
  for(std::size_t i=0;i<vertices.size();i+=6)
    check(glm::cross(vertices[i+1].position-vertices[i].position,vertices[i+2].position-vertices[i].position).z>0,"debris faces the camera");
  for(int i=0;i<10;++i) debris.tick(.1f);
  check(debris.size()==0 && debris.mesh({1,0,0},{0,1,0}).empty(),"debris expires without accumulating geometry");
  for(int i=0;i<20;++i) debris.water({0,2,0});
  check(debris.size()==DebrisCloud::capacity && debris.mesh({1,0,0},{0,1,0}).size()==DebrisCloud::capacity*6,
        "watering many plants shares the bounded particle allocation");
  for(int i=0;i<5;++i) debris.tick(.1f);
  check(debris.size()==0,"water droplets disappear after a short sprinkle");
}
void craftingSaves() {
  auto directory=std::filesystem::temp_directory_path()/("blockworld-crafting-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);} } cleanup{directory};
  auto path=directory/"world.bw";
  auto world=emptyWorld(); world.crafting={2,3,4,127}; world.clock.phase=.8; world.clock.day=12;
  world.set({1,3,1},Block::Workbench); Player p; p.pose.position={2,4,5}; world.save(path,p.pose);
  World loaded; check(loaded.load(path).has_value(),"version four loads"); loaded.ensure({0,0},1);
  check(loaded.crafting.wood==2 && loaded.crafting.planks==3 && loaded.crafting.stone==4 && loaded.crafting.flags==127
        && loaded.get({1,3,1})==Block::Workbench,"supplies, unlocks and workbench persist");
  for(auto bag : {"-1 3 4 127","2 10000 4 127","2 3 4 128","2 3 4 -1","2 3 4 4294967296"}) {
    std::ofstream(path)<<"BLOCKWORLD 4 123 1\n2 4 5 0 0 0\n0\n0.5 1\n"<<bag<<"\n0\n";
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.crafting.flags==127 && loaded.crafting.wood==2 && loaded.clock.day==12
          && loaded.get({1,3,1})==Block::Workbench,"invalid crafting data is rejected without partial mutation");
  }
  std::ofstream(path)<<"BLOCKWORLD 3 123 1\n2 4 5 0 0 0\n127\n0.8 12\n2\n1 3 1 20\n1 3 0 21\n";
  check(loaded.load(path).has_value(),"previous version three loads"); loaded.ensure({0,0},1);
  check(loaded.clock.day==12 && loaded.clock.phase==.8 && loaded.guideFlags==127 && loaded.get({1,3,1})==Block::BedZ
        && loaded.get({1,3,0})==Block::BedZHead && loaded.crafting.flags==0 && loaded.crafting.wood==0,"old clocks, beds and progress migrate with an empty craft bag");
}
}
int main() {
  try {
    for(auto [name,test] : {std::pair{"clicks and mining",inputAndMining},{"accidental removal protection",accidentalRemoval},{"crafting lesson",craftingLesson},
                           {"placement and debris",previewsAndDebris},{"crafting saves",craftingSaves}}) {
      test(); std::cout<<"PASS "<<name<<'\n';
    }
  } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

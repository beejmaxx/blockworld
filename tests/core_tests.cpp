#include "adventure.hpp"
#include <chrono>
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
void coordinates() {
  check(floorDiv(-1,16)==-1 && floorDiv(-16,16)==-1 && floorDiv(-17,16)==-2,"negative chunk division");
  check(localCoord(-1)==15 && localCoord(-16)==0 && localCoord(17)==1,"local block coordinate");
  auto world=emptyWorld();
  check(world.set({-1,5,-16},Block::Brick),"set block in negative chunk");
  check(world.get({-1,5,-16})==Block::Brick,"read block in negative chunk");
  check(!world.set({0,0,0},Block::Air) && !world.set({0,worldHeight,0},Block::Stone),"world vertical bounds");
}
void terrain() {
  Terrain a(42),b(42),c(43);
  check(a.generate({-3,2}).blocks==b.generate({-3,2}).blocks,"seed must be deterministic");
  check(a.generate({0,0}).blocks!=c.generate({0,0}).blocks,"different seeds must differ");
  auto chunk=a.generate({-1,-1});
  for(int z=0;z<16;++z) for(int x=0;x<16;++x) {
    check(chunk.get(x,0,z)==Block::Bedrock,"solid bedrock floor");
    check(solid(chunk.get(x,a.height(x-16,z-16),z)),"surface height matches terrain");
  }
}
void mesh() {
  auto world=emptyWorld();
  world.set({15,5,0},Block::Stone);
  check(buildMesh(world,world.chunks.at({0,0})).size()==36,"isolated cube has six faces");
  world.set({16,5,0},Block::Stone);
  check(buildMesh(world,world.chunks.at({0,0})).size()==30,"shared face culled across chunks");
  check(buildMesh(world,world.chunks.at({1,0})).size()==30,"neighbor shared face culled");
  for(auto& [p,c] : world.chunks) c.dirty=false;
  world.set({15,5,0},Block::Air);
  check(world.chunks.at({1,0}).dirty,"editing chunk boundary invalidates neighbor");
  auto vertices=buildMesh(world,world.chunks.at({1,0}));
  check(vertices.size()==36,"removing neighbor exposes face");
  for(std::size_t i=0;i<vertices.size();i+=3) {
    auto a=vertices[i].position,b=vertices[i+1].position,c=vertices[i+2].position;
    check(glm::dot(glm::cross(b-a,c-a),(a+b+c)/3.f-glm::vec3(16.5f,5.5f,.5f))>0,"outward triangle winding");
  }
}
void rays() {
  auto world=emptyWorld(); world.set({-1,5,0},Block::Stone);
  auto hit=world.raycast({2.5f,5.5f,.5f},{-1,0,0},4);
  check(hit && hit->block==Cell{-1,5,0} && hit->adjacent==Cell{0,5,0},"DDA traverses negative boundary");
  check(std::abs(hit->distance-2.5f)<.001f,"DDA reports exact reach");
  check(!world.raycast({2.5f,5.5f,.5f},{-1,0,0},2),"DDA reach cutoff");
  check(!world.raycast({0,6,0},{0,0,0}),"zero direction is safe");
  world.set({0,2,0},Block::Dirt);
  hit=world.raycast({.5f,5,.5f},{0,-1,0},3);
  check(hit && hit->block==Cell{0,2,0} && hit->adjacent==Cell{0,3,0},"vertical ray handles zero axes");
}
void physics() {
  auto world=emptyWorld();
  for(int z=-10;z<=10;++z) for(int x=-10;x<=10;++x) world.set({x,1,z},Block::Stone);
  for(int y=2;y<6;++y) for(int z=-4;z<4;++z) world.set({2,y,z},Block::Stone);
  Player player; player.pose.position={.5f,7,.5f}; player.pose.yaw=0;
  for(int i=0;i<180;++i) player.tick(world,{},1.f/60.f);
  check(player.grounded && std::abs(player.pose.position.y-2.f)<.003f,"gravity lands on floor");
  Movement move; move.right=1; move.sprint=true;
  for(int i=0;i<60;++i) player.tick(world,move,.1f);
  check(player.pose.position.x<=1.701f && !player.collides(world,player.pose.position),"cannot tunnel through wall");
  move={}; move.jump=true; player.tick(world,move,1.f/60.f);
  check(player.pose.position.y>2.05f,"jump lifts player");
  check(player.overlaps({1,2,0}),"placement rejects player overlap");
  check(!player.overlaps({5,2,0}),"remote block does not overlap");
  player.pose.flying=true; move={}; move.vertical=1;
  float initial=player.pose.position.y; player.tick(world,move,.1f);
  check(player.pose.position.y>initial+.8f,"creative flight ascends");
}
void saves() {
  auto name="blockworld-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  auto directory=std::filesystem::temp_directory_path()/name;
  struct Cleanup { std::filesystem::path p; ~Cleanup(){ std::error_code e;std::filesystem::remove_all(p,e); } } cleanup{directory};
  auto path=directory/"world.bw";
  World world(123); world.ensure({0,0},1);
  Cell cell{-1,50,-1}; world.set(cell,Block::Brick);
  Cell removed{1,world.terrain.height(1,1),1}; world.set(removed,Block::Air);
  PlayerPose pose; pose.position={3,45,7}; pose.yaw=.2f; pose.pitch=-.3f; pose.flying=true;
  world.clock.phase=.876543210987; world.clock.day=17;
  world.save(path,pose);
  World loaded; auto savedPose=loaded.load(path);
  check(savedPose && savedPose->position==pose.position && savedPose->flying,"player pose survives reload");
  check(loaded.terrain.seed()==123 && loaded.editCount()==2,"seed and edits survive reload");
  check(loaded.clock.phase==world.clock.phase && loaded.clock.day==17,"world time survives reload without rounding drift");
  loaded.ensure({0,0},1);
  check(loaded.get(cell)==Block::Brick && loaded.get(removed)==Block::Air,"block addition and removal survive reload");
  loaded.evict({50,50},1); loaded.ensure({0,0},1);
  check(loaded.get(cell)==Block::Brick,"edits survive chunk eviction");
  loaded.save(path,*savedPose); // Replacing an existing save is atomic on the target platform.
  std::ofstream(path)<<"BLOCKWORLD 1 123\ninvalid\n";
  bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
  check(rejected && loaded.get(cell)==Block::Brick,"bad save rejected without mutating current world");
  std::ofstream(path)<<"BLOCKWORLD 1 123\n3 45 7 0.2 -0.3 0\n1\n-1 50 -1 8\n";
  World legacy(777,true); check(legacy.load(path).has_value(),"original save format still loads");
  legacy.ensure({0,0},1);
  check(!legacy.terrain.adventure() && legacy.get(cell)==Block::Brick,"original terrain and edits remain unchanged");
  check(legacy.clock.phase==WorldClock{}.phase && legacy.clock.day==1,"old worlds begin in daylight");
  std::ofstream(path)<<"BLOCKWORLD 2 123 1\n3 25 7 0.2 -0.3 0\n63\n1\n-1 50 -1 11\n";
  check(legacy.load(path).has_value() && legacy.guideFlags==63 && legacy.terrain.adventure(),"version-2 cabin progress migrates");
  legacy.ensure({0,0},1); check(legacy.get(cell)==Block::Torch,"version-2 block IDs remain stable");
  for(auto invalid : {"1.0 4","-0.1 4","nan 4","0.5 0","0.5 4294967296"}) {
    std::ofstream(path)<<"BLOCKWORLD 3 123 1\n3 25 7 0.2 -0.3 0\n63\n"<<invalid<<"\n0\n";
    rejected=false; try { legacy.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && legacy.get(cell)==Block::Torch && legacy.clock.day==1,"invalid clock rejected without mutating world");
  }
}
void dayCycle() {
  WorldClock clock;
  clock.phase=.99; clock.advance(24);
  check(clock.day==2 && std::abs(clock.phase-.01)<1e-8,"midnight advances day and preserves remainder");
  clock.advance(WorldClock::daySeconds*2.5);
  check(clock.day==4 && std::abs(clock.phase-.51)<1e-8,"long updates preserve whole days");
  auto before=clock.phase;
  clock.advance(-10); clock.advance(std::numeric_limits<double>::quiet_NaN());
  check(clock.phase==before,"invalid deltas do not poison the clock");
  clock.phase=.5; auto noon=clock.sky();
  clock.phase=0; auto midnight=clock.sky();
  check(noon.daylight==1 && midnight.daylight==0 && noon.ambient.x>midnight.ambient.x,"night dims outdoor light");
  check(noon.sun.y>.8f && midnight.sun.y<-.8f,"sun crosses below the horizon at night");
  clock.phase=.75; check(clock.sky().twilight>.9f && clock.canSleep(),"sunset is warm and allows sleep");
  clock.wakeAtMorning(); check(clock.day==5 && clock.phase==WorldClock::morning,"evening sleep advances to next morning");
  clock.phase=.05; clock.wakeAtMorning(); check(clock.day==5,"sleep after midnight keeps the current calendar day");
  clock.phase=.5; check(!clock.canSleep(),"no daytime sleep");
}
void beds() {
  auto world=emptyWorld(); Player player; player.pose.position={13.5f,2,1.5f};
  Cell foot{15,2,0},head{15,2,-1};
  world.set({15,1,0},Block::Stone);
  check(!placeBlock(world,player,foot,Block::BedZ) && world.get(foot)==Block::Air,"both bed halves need support");
  world.set({15,1,-1},Block::Stone); world.set(head,Block::Stone);
  check(!placeBlock(world,player,foot,Block::BedZ) && world.get(foot)==Block::Air,"blocked second half leaves no partial bed");
  world.set(head,Block::Air); player.pose.position={15.5f,2,-.5f};
  check(!placeBlock(world,player,foot,Block::BedZ),"bed head cannot overlap player");
  player.pose.position={13.5f,2,1.5f};
  check(placeBlock(world,player,foot,Block::BedZ),"two-part bed spans a chunk boundary");
  check(world.get(head)==Block::BedZHead,"bed head has matching orientation");
  check(!world.raycast({13,2.8f,.5f},{1,0,0}),"rays pass above a low bed");
  auto hit=world.raycast({13,2.4f,.5f},{1,0,0});
  check(hit && hit->block==foot,"mattress is targetable");
  Player jumper; jumper.pose.position={15.5f,4,.5f};
  for(int i=0;i<100;++i) jumper.tick(world,{},1.f/60.f);
  check(jumper.grounded && std::abs(jumper.pose.position.y-2.55f)<.003f,"player lands on mattress height");
  auto time=world.clock.phase;
  check(sleepInBed(world,player,foot)==SleepResult::Daytime && world.clock.phase==time && world.guideFlags==0,"daytime interaction does not skip time");
  world.clock.phase=.9;
  world.set({15,3,-1},Block::Stone);
  check(sleepInBed(world,player,foot)==SleepResult::Obstructed && world.clock.day==1,"covered pillow blocks sleeping");
  world.set({15,3,-1},Block::Air);
  player.pose.position={0,2,0};
  check(sleepInBed(world,player,foot)==SleepResult::TooFar,"cannot sleep from far away");
  player.pose.position={13.5f,2,-.5f};
  check(sleepInBed(world,player,head)==SleepResult::Ready && world.clock.day==2
        && world.clock.phase==WorldClock::morning && (world.guideFlags&Slept),"either half sleeps until next morning");
  check(breakBlock(world,{15,1,-1}) && world.get(foot)==Block::Air && world.get(head)==Block::Air,"removing pillow support removes whole bed");
  world.set({15,1,2},Block::Stone); world.set({16,1,2},Block::Stone);
  check(placeBlock(world,player,{15,2,2},Block::BedZ,true) && world.get({16,2,2})==Block::BedXHead,"bed rotates across an X chunk boundary");
  check(breakBlock(world,{16,2,2}) && world.get({15,2,2})==Block::Air,"breaking head removes foot");
  world.set(foot,Block::BedZ); world.clock.phase=.9;
  check(sleepInBed(world,player,foot)==SleepResult::NotABed,"broken bed pair cannot be used");
}
void doorsAndTorches() {
  auto world=emptyWorld(); Player player; player.pose.position={-3,2,3};
  world.set({0,1,0},Block::Stone);
  check(placeBlock(world,player,{0,2,0},Block::DoorZ),"door placement creates both halves");
  check(world.get({0,2,0})==Block::DoorZ && world.get({0,3,0})==Block::DoorZTop,"door halves agree");
  auto hit=world.raycast({.5f,2.5f,2},{0,0,-1});
  check(hit && hit->block==Cell{0,2,0},"closed door is targetable");
  check(player.collides(world,{.5f,2,1.1f}),"closed door blocks movement");
  check(toggleDoor(world,player,{0,3,0}),"upper half can open door");
  check(world.get({0,3,0})==Block::DoorZOpenTop,"both halves open together");
  check(!player.collides(world,{.5f,2,.5f}),"open doorway is traversable");
  check(!world.raycast({.5f,2.5f,2},{0,0,-1}),"ray passes through open doorway");
  player.pose.position={.5f,2,.9f};
  check(!toggleDoor(world,player,{0,2,0}) && doorOpen(world.get({0,2,0})),"cannot close door into player");
  player.pose.position={-3,2,3};
  check(breakBlock(world,{0,3,0}) && world.get({0,2,0})==Block::Air,"breaking upper door removes lower half");
  world.set({0,3,0},Block::Stone);
  check(!placeBlock(world,player,{0,2,0},Block::DoorZ) && world.get({0,2,0})==Block::Air,"blocked doorway placement is atomic");
  world.set({0,3,0},Block::Air);
  check(placeBlock(world,player,{0,2,0},Block::DoorZ,true),"perpendicular door placement");
  check(world.get({0,2,0})==Block::DoorX,"door follows placement orientation");
  check(breakBlock(world,{0,1,0}) && world.get({0,2,0})==Block::Air && world.get({0,3,0})==Block::Air,"unsupported door removes both halves");
  check(!placeBlock(world,player,{3,2,0},Block::Torch),"torch needs support");
  world.set({3,1,0},Block::Stone);
  check(placeBlock(world,player,{3,2,0},Block::Torch),"torch can sit on floor");
  check(!player.collides(world,{3.5f,2,.5f}),"torch does not block walking");
  check(world.raycast({3.5f,2.5f,2},{0,0,-1}).has_value(),"torch stem is targetable");
  check(!world.raycast({3.05f,2.5f,2},{0,0,-1}),"ray misses empty part of torch cell");
  check(breakBlock(world,{3,1,0}) && world.get({3,2,0})==Block::Air,"unsupported torch is removed");
}
void glass() {
  auto world=emptyWorld(); world.set({1,5,1},Block::Stone); world.set({2,5,1},Block::Glass);
  auto vertices=buildMesh(world,world.chunks.at({0,0}));
  check(vertices.size()==66,"glass does not hide the opaque surface behind it");
  Player p; check(p.collides(world,{2.5f,5,1.5f}),"glass remains a solid window");
}
void guidedAdventure() {
  World world(7262026,true); world.ensure({0,0},3);
  Player player; player.pose.position={10.5f,24,7.5f}; player.pose.yaw=0;
  Adventure guide;
  check(guide.stage(world)==0,"guide starts with looking");
  guide.looked(world,100,1); check(guide.stage(world)==1,"looking advances guide");
  guide.moved(world,{0,0,0},{3,0,0}); check(guide.stage(world)==2,"walking advances guide");
  check(breakBlock(world,{7,25,3}),"practice stump exists"); world.guideFlags|=Broke;
  check(guide.stage(world)==3,"breaking advances guide");
  check(placeBlock(world,player,{7,25,3},Block::Planks),"practice placement"); world.guideFlags|=Placed;
  check(guide.stage(world)==4,"placing advances to cabin");
  auto initial=inspectCabin(world);
  check(initial.done>0 && !initial.complete(),"starter foundation supplies part of the cabin");
  auto before=initial.done;
  for(int i=0;i<3;++i) { world.set({0,40,0},Block::Planks); world.set({0,40,0},Block::Air); }
  check(inspectCabin(world).done==before,"unrelated or repeated edits cannot complete cabin");
  // Tour the four sides as a player would while holding F; every piece obeys reach and collision.
  for(int tour=0;tour<4 && !inspectCabin(world).complete();++tour)
    for(glm::vec3 position : {glm::vec3(10.5f,24,.5f),{6.5f,24,-4.5f},{14.5f,24,-4.5f},{10.5f,24,-8.5f}}) {
      player.pose.position=position;
      for(int attempts=0;attempts<150 && guide.buildNext(world,player);++attempts) {}
    }
  check(inspectCabin(world).complete(),"guide can finish the entire cabin from walkable positions");
  guide.update(world,player); check(guide.stage(world)==5,"finished cabin unlocks cave objective");
  check(world.get({10,25,-2})==Block::DoorZ && world.get({9,25,-5})==Block::Torch,"cabin includes working door and lamp");
  world.set({10,30,-4},Block::Air);
  check(!inspectCabin(world).complete(),"missing roof is visible in structural progress");
  for(int x=24;x<=38;++x) check(!solid(world.get({x,25,-4})),"cave has a continuous walkable passage");
  player.pose.position={36.5f,24,-3.5f};
  check(!player.collides(world,player.pose.position),"cave destination is accessible");
  guide.update(world,player); check(guide.stage(world)==6,"cave visit unlocks the first-night guide");
  check(pieceComplete(world,cabinBed),"guided cabin includes the two-block bed");
  breakBlock(world,cabinBed.cell); // Simulate a cabin completed before beds existed.
  player.pose.position={9.7f,25,-3.25f}; world.clock.phase=.9;
  auto next=guide.nextPiece(world,player);
  check(next && next->block==Block::BedZ && guide.buildNext(world,player),"old completed cabins can add a bed with F");
  check(sleepInBed(world,player,cabinBed.cell)==SleepResult::Ready,"cabin bed can be slept in");
  guide.update(world,player); check(guide.stage(world)==7,"first sunrise completes the guide");
  auto directory=std::filesystem::temp_directory_path()/("blockworld-adventure-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);} } cleanup{directory};
  auto path=directory/"meadow.bw"; world.save(path,player.pose);
  World restored; check(restored.load(path).has_value(),"adventure reloads"); restored.ensure({0,0},3);
  check(restored.terrain.adventure() && restored.guideFlags==127,"generation and tutorial milestones persist");
  check(pieceComplete(restored,cabinBed) && restored.clock.day==2,"bed pair, first sleep, and new day persist");
  check(restored.get({10,25,-2})==Block::DoorZ && restored.get({9,25,-5})==Block::Torch,"new block types persist");
  check(restored.get({10,30,-4})==Block::Air,"removed generated or guided blocks stay removed");
}
}
int main() {
  try {
    for(auto [name,test] : {std::pair{"coordinates",&coordinates},{"terrain",terrain},{"mesh",mesh},
                           {"raycast",rays},{"physics",physics},{"persistence",saves},
                           {"doors and torches",doorsAndTorches},{"glass",glass},{"daylight",dayCycle},{"beds",beds},
                           {"guided adventure",guidedAdventure}}) {
      test(); std::cout<<"PASS "<<name<<'\n';
    }
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

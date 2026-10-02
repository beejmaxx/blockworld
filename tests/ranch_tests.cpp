#include "ranch.hpp"
#include "garden.hpp"
#include "inventory.hpp"
#include "building.hpp"
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
World flat() {
  World w;
  for(int z=-2;z<=2;++z) for(int x=-2;x<=2;++x) w.insert(Chunk{{x,z}});
  for(int z=-25;z<=25;++z) for(int x=-25;x<=25;++x) w.set({x,1,z},Block::Grass);
  w.farm.initialized=true; return w;
}
void aim(Player& p,glm::vec3 target) {
  auto delta=glm::normalize(target-p.eye()); p.pose.yaw=std::atan2(delta.x,-delta.z); p.pose.pitch=std::asin(delta.y);
}
void purchasesAndMilk() {
  auto w=flat(); Player p; p.pose.position={.5f,2,.5f};
  check(!buyLivestock(w,p,LivestockKind::Cow).empty() && w.farm.livestock.empty(),"an empty wallet cannot buy an animal");
  w.farm.garden.coins=55;
  check(buyLivestock(w,p,LivestockKind::Cow).empty() && w.farm.garden.coins==35,"a cow costs exactly twenty coins");
  check(buyLivestock(w,p,LivestockKind::Horse).empty() && w.farm.garden.coins==0 && w.farm.livestock.size()==2,"a horse costs exactly thirty-five coins");
  const auto cow=w.farm.livestock[0].position;
  p.pose.position=cow+glm::vec3(0,0,3); aim(p,cow+glm::vec3(0,1,0));
  check(targetRanch(w,p)->index==0 && useTarget(w,p,Item::Hoe).kind==UseKind::Ranch,"cows can be used with any held tool");
  check(!miningTarget(w,p),"instant clicks never remove blocks through a cow");
  check(collectMilk(w,p,0) && w.farm.milk==1 && !collectMilk(w,p,0),"collecting milk starts a cooldown without duplicating milk");
  growFarm(w,59); check(!collectMilk(w,p,0),"milk must finish its cooldown");
  growFarm(w,1); check(collectMilk(w,p,0) && w.farm.milk==2,"cows make more milk after sixty seconds");
  w.farm.eggs=2; w.farm.carrots=3;
  check(basketValue(w.farm)==16 && farmView(w).basketValue==16 && sellBasket(w)==16,"milk and vegetables have the same price in UI and sale");
  check(w.farm.milk==0 && w.farm.carrots==0 && w.farm.eggs==2 && sellBasket(w)==0,"sale clears milk and vegetables once, preserving eggs");
  w.farm.garden.coins=coinLimit; w.farm.milk=1;
  check(sellBasket(w)==0 && w.farm.milk==1,"full wallets do not discard milk");
  World empty; empty.insert(Chunk{{0,0}}); empty.farm.garden.coins=100;
  check(!buyLivestock(empty,p,LivestockKind::Cow).empty() && empty.farm.garden.coins==100 && empty.farm.livestock.empty(),"blocked delivery never spends coins");
  check(!bringCar(empty,p).empty() && !empty.farm.car.owned,"car delivery also requires a floor");
  auto capped=flat(); capped.farm.garden.coins=100; capped.farm.livestock.resize(livestockLimit);
  check(!buyLivestock(capped,p,LivestockKind::Horse).empty() && capped.farm.garden.coins==100,"pasture capacity is enforced without charging");
}
void drivingAndRiding() {
  auto w=flat(); Player p; p.pose.position={.5f,2,.5f}; RideState ride;
  check(bringCar(w,p).empty() && w.farm.car.owned && w.farm.garden.coins==0,"car is free and arrives on clear ground");
  auto start=w.farm.car.position; aim(p,start+glm::vec3(0,.8f,0));
  check(targetRanch(w,p) && targetRanch(w,p)->car && !miningTarget(w,p),"car has a protected interaction target");
  check(mountRanch(w,p,ride,*targetRanch(w,p)) && ride.active && ride.car && !p.pose.flying,"V mounts a car and stops flight");
  auto before=w.farm.car.position; Movement drive; drive.forward=1;
  for(int i=0;i<60;++i) tickRanch(w,p,ride,drive,1.f/60.f);
  check(glm::length(w.farm.car.position-before)>8.8f && glm::length(p.pose.position-w.farm.car.position)<.4f,"driving moves car and player together");
  drive.jump=true; before=w.farm.car.position; tickRanch(w,p,ride,drive,.1f);
  check(w.farm.car.position==before && w.farm.car.speed==0,"space brakes immediately");
  drive.jump=false; drive.forward=-1; tickRanch(w,p,ride,drive,.1f);
  check(w.farm.car.position.z>before.z,"reverse moves backwards");
  drive.forward=0; drive.right=1; auto yaw=w.farm.car.yaw; tickRanch(w,p,ride,drive,.1f);
  check(w.farm.car.yaw>yaw,"steering turns the car");
  check(leaveRide(w,p,ride) && !ride.active && !p.collides(w,p.pose.position),"getting out finds safe ground beside the car");
  auto edits=w.editCount(); check(bringCar(w,p).empty() && w.editCount()==edits,"recalling a car never edits buildings or terrain");

  // Long straight drive into a full-height wall; large steps cannot tunnel through it.
  w=flat(); w.farm.car={true,{.5f,2,5.5f},0,0}; p.pose.position={.5f,2,8.5f}; aim(p,w.farm.car.position+glm::vec3(0,.8f,0));
  check(mountRanch(w,p,ride,*targetRanch(w,p)),"wall fixture mounts");
  for(int x=-15;x<=15;++x) for(int y=2;y<8;++y) w.set({x,y,0},Block::Stone);
  drive={}; drive.forward=1;
  for(int i=0;i<100;++i) tickRanch(w,p,ride,drive,.1f);
  check(w.farm.car.position.z>=2.69f && w.farm.car.speed==0,"car stops before a wall without tunneling");
  check(leaveRide(w,p,ride),"car can be exited beside a wall");

  w=flat(); w.farm.garden.coins=35; p.pose.position={.5f,2,.5f}; p.pose.yaw=0;
  check(buyLivestock(w,p,LivestockKind::Horse).empty(),"horse fixture delivered");
  auto horse=w.farm.livestock[0].position; aim(p,horse+glm::vec3(0,1.5f,0));
  check(mountRanch(w,p,ride,*targetRanch(w,p)) && !ride.car,"horses can be mounted");
  for(int i=0;i<30;++i) tickRanch(w,p,ride,drive,1.f/60.f);
  check(glm::length(w.farm.livestock[0].position-horse)>2.9f,"horse carries the rider forward");
  check(leaveRide(w,p,ride),"rider can safely dismount");
  auto mesh=ranchMesh(w); check(!mesh.empty() && mesh.size()<=ranchVertexLimit && mesh.size()%36==0,"ranch geometry fits its GPU buffer");
  for(const auto& v : mesh) check(std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z),"ranch mesh has finite vertices");
}
void persistence() {
  auto directory=std::filesystem::temp_directory_path()/("blockworld-ranch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);} } cleanup{directory};
  auto path=directory/"world.bw"; auto w=flat(); Player p; p.pose.position={.5f,2,.5f}; w.farm.garden.coins=55;
  check(buyLivestock(w,p,LivestockKind::Cow).empty() && buyLivestock(w,p,LivestockKind::Horse).empty() && bringCar(w,p).empty(),"saved fixture delivers all new objects");
  w.farm.milk=4; w.farm.livestock[0].milkTimer=12.5f; w.farm.car.speed=9;
  w.save(path,p.pose); World loaded; auto pose=loaded.load(path);
  check(pose.has_value() && loaded.farm.livestock.size()==2 && loaded.farm.milk==4 && loaded.farm.car.owned
    && loaded.farm.car.position==w.farm.car.position && loaded.farm.car.speed==0 && loaded.farm.livestock[0].milkTimer==12.5f
    && loaded.farm.livestock[1].kind==LivestockKind::Horse,"animals, milk, timers, and parked car survive reload");
  std::ifstream input(path); std::vector<std::string> lines; for(std::string line;std::getline(input,line);) lines.push_back(line);
  auto original=lines;
  // With no crops or chickens, the v10 ranch section is lines 8..11.
  lines[8]="4 999"; { std::ofstream out(path); for(const auto& line : lines) out<<line<<'\n'; }
  bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
  check(rejected && loaded.farm.livestock.size()==2 && loaded.farm.milk==4,"invalid ranch saves reject atomically");
  lines=original; lines[0].replace(0,13,"BLOCKWORLD 9 "); lines.erase(lines.begin()+8,lines.begin()+12);
  { std::ofstream out(path); for(const auto& line : lines) out<<line<<'\n'; }
  World old; check(old.load(path).has_value() && old.farm.livestock.empty() && !old.farm.car.owned && old.editCount()==w.editCount(),"version-nine worlds migrate without changing their existing builds");
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
void preview(const std::filesystem::path& path) {
  World world=flat();
  Livestock cow; cow.position={-3,2,0}; cow.yaw=-.2f;
  Livestock horse; horse.kind=LivestockKind::Horse; horse.position={0,2,0}; horse.yaw=.15f;
  world.farm.livestock={cow,horse}; world.farm.car={true,{4,2,0},-.2f,0};
  auto mesh=ranchMesh(world);
  appendBox(mesh,{{-8,1.85f,-5},{8,2,5}},{0,1,0},1,1);
  constexpr int w=1200,h=700;
  std::vector<glm::vec3> pixels(w*h,{.69f,.80f,.81f}); std::vector<float> depth(w*h,1.f);
  auto matrix=glm::perspective(glm::radians(45.f),float(w)/h,.1f,70.f)*glm::lookAt(glm::vec3(10,9,-16),glm::vec3(0,2.5f,0),glm::vec3(0,1,0));
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
  try { purchasesAndMilk(); drivingAndRiding(); persistence(); if(argc>1) preview(argv[1]); std::cout<<"PASS purchases, milk, car driving, horse riding, collision, and save migration\n"; }
  catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

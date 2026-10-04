#include "road.hpp"
#include "coast.hpp"
#include "harbor.hpp"
#include "city.hpp"
#include "castle.hpp"
#include "garden.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok)throw std::runtime_error(message); }
glm::vec2 xz(glm::vec3 p) {return {p.x,p.z};}
glm::vec3 atDistance(const std::vector<glm::vec3>& road,float distance) {
  for(std::size_t i=1;i<road.size();++i) {
    float length=glm::length(xz(road[i]-road[i-1]));
    if(distance<=length)return glm::mix(road[i-1],road[i],std::clamp(distance/length,0.f,1.f));
    distance-=length;
  }
  return road.back();
}
void drive(World& w,bool back,bool boost=false) {
  auto route=w.road;if(back)std::ranges::reverse(route);
  auto pos=atDistance(route,4),next=atDistance(route,8);
  pos.y=std::round(pos.y);
  w.farm.car={true,pos,std::atan2(next.x-pos.x,pos.z-next.z),0};
  Player p;p.pose.position=pos+glm::vec3(0,.32f,0);RideState ride{true,true,0};
  float peak=0,maxError=0;bool arrived=false;
  for(int frame=0;frame<18000;++frame) {
    auto& car=w.farm.car; auto center=chunkAt(int(std::floor(car.position.x)),int(std::floor(car.position.z)));
    w.ensure(center,1); if(frame%120==0)w.evict(center,3);
    auto sample=sampleRoad(route,car.position.x,car.position.z);
    float remaining=sample.length-sample.along;
    maxError=std::max(maxError,sample.distance);
    if(sample.distance>5.2f)throw std::runtime_error("driver left road at "+std::to_string(sample.along)+" of "+std::to_string(sample.length));
    if(remaining<3) {tickRanch(w,p,ride,{.jump=true},1.f/60);arrived=true;break;}
    auto aim=atDistance(route,sample.along+std::max(8.f,std::abs(car.speed)*.55f));
    float yaw=std::atan2(aim.x-car.position.x,car.position.z-aim.z);
    float error=std::remainder(yaw-car.yaw,6.2831853f);
    float top=boost ? carBoostSpeed : carTopSpeed;
    float wanted=std::abs(error)>.16f ? 16.f : std::abs(error)>.08f ? 26.f : top;
    wanted=std::min(wanted,std::sqrt(18.f*remaining));
    Movement m;m.right=std::clamp(error*2.f,-1.f,1.f);
    m.forward=car.speed>wanted+.5f ? 0 : wanted/top;m.boost=boost;
    tickRanch(w,p,ride,m,1.f/60);
    peak=std::max(peak,car.speed);
    if(frame>200 && car.speed<.05f) {
      std::cerr<<"Road distance "<<sample.distance<<" height "<<sample.height<<" yaw "<<car.yaw<<" steer "<<m.right<<"\n";
      for(int z=int(car.position.z)-3;z<=int(car.position.z)+3;++z) {
        for(int x=int(car.position.x)-3;x<=int(car.position.x)+3;++x) {
          int top=0;for(int y=1;y<40;++y)if(collidable(w.get({x,y,z})))top=y;
          std::cerr<<top<<' ';
        }
        std::cerr<<'\n';
      }
      throw std::runtime_error("car stuck at "+std::to_string(car.position.x)+","+std::to_string(car.position.y)+","+std::to_string(car.position.z)+" progress "+std::to_string(sample.along));
    }
    check(glm::length(p.pose.position-car.position)<.4f,"rider stays attached over streamed chunk boundaries");
  }
  check(arrived,"complete road is drivable without teleporting");
  check(peak>38.f,"long straights allow more than four times the old car speed");
  if(boost)check(peak>80.f,"boost exceeds 288 km/h on a streamed highway");
  check(w.farm.car.speed==0 && leaveRide(w,p,ride),"driver can brake and get out at either end");
  std::cout<<(back ? "City to home" : "Home to city")<<(boost ? " with boost" : "")<<": peak "<<peak*3.6f<<" km/h; lateral error "<<maxError<<" m\n";
}
void persistence(World& w,Player p) {
  auto path=std::filesystem::temp_directory_path()/("blockworld-road-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}}cleanup{path};
  auto midpoint=w.road[w.road.size()/2];Cell c{int(midpoint.x),int(std::round(midpoint.y))-1,int(midpoint.z)};
  w.ensure(chunkAt(c.x,c.z),0);w.set(c,Block::BlueTile);
  w.save(path,p.pose);World loaded;
  check(loaded.load(path).has_value() && loaded.road==w.road,"saved highway layout is stable after reload");
  loaded.ensure(chunkAt(c.x,c.z),0);check(loaded.get(c)==Block::BlueTile,"player edits override the generated highway");
  loaded.evict({-100,-100},0);loaded.ensure(chunkAt(c.x,c.z),0);
  check(loaded.get(c)==Block::BlueTile,"streaming cannot undo a player road edit");
  // Corrupt road count and height reject the whole load without mutating it.
  World blank;blank.coastOrigin=Cell{512,18,-192};blank.save(path,p.pose);
  std::ifstream input(path);std::vector<std::string> lines;for(std::string s;std::getline(input,s);)lines.push_back(s);
  auto original=lines;
  auto write=[&]{std::ofstream out(path);for(auto& s:lines)out<<s<<'\n';};
  auto reject=[&]{bool rejected=false;try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.road==w.road,"invalid road metadata rejects atomically");};
  lines[14]="999999";write();reject();
  lines=original;lines[14]="2";lines.insert(lines.begin()+15,{"0 23 0","10 100 0"});write();reject();
  lines=original;lines[0]="BLOCKWORLD 14 7262026 0";lines.erase(lines.begin()+14,lines.begin()+16);write();
  check(loaded.load(path).has_value() && loaded.road.empty() && loaded.coastOrigin==blank.coastOrigin,"v14 worlds load unchanged before road installation");
}
void protection() {
  World w(7262026,true);Player p;p.pose.position={10.5f,24,7.5f};
  w.ensure({0,0},1);initializeFarm(w);initializeHome(w);initializeGarden(w);
  initializeCastle(w,p);initializeCity(w,p);
  // A long player-built wall across the otherwise direct route forces a detour.
  for(int z=-8;z<=104;++z) {w.ensure(chunkAt(320,z),0);w.set({320,24,z},Block::Brick);}
  auto edits=w.editCount();
  check(initializeRoad(w,p) && w.editCount()==edits,"road planner preserves all construction without saving generated blocks as edits");
  check(w.road.size()>2 && sampleRoad(w.road,320,48).distance>roadClearance,"road routes around existing construction");
  for(int z=-8;z<=104;++z){w.ensure(chunkAt(320,z),0);check(w.get({320,24,z})==Block::Brick,"existing wall survives highway generation");}
  drive(w,false);drive(w,true);persistence(w,p);
  // Alternative coast placement must work too, including negative coordinates.
  World alternate(42,true);alternate.coastOrigin=Cell{-896,18,256};
  check(initializeRoad(alternate,p),"road also reaches an alternative coast parcel");
  check(coastContains(*alternate.coastOrigin,alternate.road.back().x,alternate.road.back().z),"road destination uses the saved coast origin");
}
void speedAndCollision() {
  World w;
  for(int z=-16;z<440;++z)for(int x=-5;x<=5;++x) {
    auto cp=chunkAt(x,z);if(!w.chunks.contains(cp))w.insert(Chunk{cp});w.set({x,1,z},Block::Asphalt);
  }
  w.farm.car={true,{.5f,2,4},3.14159265f,0};Player p;RideState ride{true,true,0};Movement m;m.forward=1;
  for(int i=0;i<240;++i)tickRanch(w,p,ride,m,1.f/60);
  check(w.farm.car.speed>41 && w.farm.car.speed<=carTopSpeed+.1f,"GT2 reaches bounded top speed after accelerating");
  auto before=w.farm.car.position;tickRanch(w,p,ride,{.jump=true},.1f);
  check(w.farm.car.speed==0 && w.farm.car.position==before,"Space is an immediate dependable brake at top speed");
  w.farm.car.position={.5f,2,4};m.boost=true;
  for(int i=0;i<240;++i)tickRanch(w,p,ride,m,1.f/60);
  check(w.farm.car.boosting && w.farm.car.speed>83.f && w.farm.car.speed<carBoostSpeed+.1f,"Shift doubles the speed ceiling to about 300 km/h");
  m.boost=false;for(int i=0;i<100;++i)tickRanch(w,p,ride,m,1.f/60);
  check(!w.farm.car.boosting && w.farm.car.speed<carTopSpeed+.1f,"releasing Shift slows back to the normal speed ceiling");
  w.farm.car.position={.5f,2,70};w.farm.car.speed=carBoostSpeed;m.boost=true;
  for(int x=-5;x<=5;++x)for(int y=2;y<6;++y)w.set({x,y,78},Block::Stone);
  for(int i=0;i<10;++i)tickRanch(w,p,ride,m,.1f);
  check(w.farm.car.position.z<75.78f && w.farm.car.speed==0,"full-speed GT2 cannot tunnel through walls at low frame rates");
  auto mesh=ranchMesh(w);check(mesh.size()<ranchVertexLimit && mesh.size()%3==0,"detailed GT2 fits its geometry buffer");
  auto bounds=carBounds(w.farm.car);
  for(auto v:mesh)check(v.position.x>=bounds.min.x-.01f && v.position.x<=bounds.max.x+.01f
    && v.position.y>=bounds.min.y-.01f && v.position.y<=bounds.max.y+.01f
    && v.position.z>=bounds.min.z-.01f && v.position.z<=bounds.max.z+.01f,"GT2 visible body fits its collision shape");
}
}
int main(int argc,char** argv) {
  try {
    if(argc>1) {
      World w;Player p;auto pose=w.load(argv[1]);check(pose.has_value(),"load supplied world copy");p.pose=*pose;
      auto edits=w.editCount();check(initializeRoad(w,p) && w.editCount()==edits,"real-world highway preserves all edits");
      auto originalFarm=w.farm;
      std::cout<<"Road: "<<sampleRoad(w.road,w.road[0].x,w.road[0].z).length<<" m; "<<w.road.size()<<" points; "<<edits<<" saved edits\n";
      std::cout<<"Start "<<w.road.front().x<<','<<w.road.front().y<<','<<w.road.front().z<<"; end "<<w.road.back().x<<','<<w.road.back().y<<','<<w.road.back().z<<'\n';
      drive(w,false);drive(w,true);drive(w,false,true);w.farm=originalFarm;
      if(argc>2)w.save(argv[2],p.pose);
    } else {protection();speedAndCollision();}
    std::cout<<"PASS physical road trips, GT2 acceleration/braking, streaming, protection and migration\n";
  }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

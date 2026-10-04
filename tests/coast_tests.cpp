#include "coast.hpp"
#include "city.hpp"
#include "ranch.hpp"
#include "building.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
void walk(World& w,Player& p,glm::vec2 to) {
  for(int i=0;i<3000;++i) {
    auto delta=to-glm::vec2(p.pose.position.x,p.pose.position.z);
    if(glm::length(delta)<.07f) return;
    p.pose.yaw=std::atan2(delta.x,-delta.y); Movement m; m.forward=1;
    p.tick(w,m,1.f/60.f);
    check(!p.collides(w,p.pose.position),"coastal walking cannot clip through terrain");
  }
  std::cerr<<"Stopped "<<p.pose.position.x<<','<<p.pose.position.y<<','<<p.pose.position.z<<" toward "<<to.x<<','<<to.y<<'\n';
  throw std::runtime_error("pier route must work without jumping or flying");
}
void landscape(World& w,const std::filesystem::path& map) {
  auto o=*w.coastOrigin;
  std::vector<CoastColumn> cells; cells.reserve(coastSize*coastSize);
  int water=0,road=0,highest=0,lowest=64;
  for(int z=0;z<coastSize;++z) for(int x=0;x<coastSize;++x) {
    auto c=coastColumn(w.terrain,o,o.x+x,o.z+z); cells.push_back(c);
    water+=c.water; road+=c.road; highest=std::max(highest,c.ground); lowest=std::min(lowest,c.ground);
    if(x==0 || z==0 || x==coastSize-1 || z==coastSize-1)
      check(c.ground==w.terrain.height(o.x+x,o.z+z),"region meets the existing terrain without a perimeter cliff");
    if(c.road) check(c.ground==22 && !c.water,"coastal roads stay level and dry");
  }
  check(water>22000 && road>2200 && highest>=44 && lowest<=7,"landscape includes deep bays, a road network, and substantial hills");
  auto at=[&](int x,int z){return cells[z*coastSize+x];};
  check(!at(182,236).water && at(182,236).ground>=22,"island rises above the bay");
  check(at(239,112).water && at(220,239).water && at(109,180).water,"lagoon and coves contain water");
  for(int z=1;z<coastSize-1;++z) for(int x=1;x<coastSize-1;++x) {
    if(std::abs(at(x,z).ground-at(x+1,z).ground)>4 || std::abs(at(x,z).ground-at(x,z+1).ground)>4) {
      std::cerr<<"Terrain seam "<<x<<','<<z<<": "<<at(x,z).ground<<" -> "<<at(x+1,z).ground<<','<<at(x,z+1).ground<<'\n';
      check(false,"graded roads and hills have no abrupt terrain seams");
    }
  }
  // The road follows one connected mainland shore, not isolated decorative strips.
  std::vector<bool> seen(cells.size()); std::queue<int> q;
  for(int i=0;i<int(cells.size());++i) if(cells[i].road) { q.push(i); seen[i]=true; break; }
  int reachable=0;
  while(!q.empty()) {
    int i=q.front(); q.pop(); ++reachable; int x=i%coastSize,z=i/coastSize;
    for(auto d : {glm::ivec2{1,0},glm::ivec2{-1,0},glm::ivec2{0,1},glm::ivec2{0,-1}}) {
      int nx=x+d.x,nz=z+d.y; if(nx<0 || nx>=coastSize || nz<0 || nz>=coastSize) continue;
      int n=nz*coastSize+nx; if(!seen[n] && cells[n].road) { seen[n]=true; q.push(n); }
    }
  }
  if(reachable<=road*.98f) std::cerr<<"Connected road "<<reachable<<" / "<<road<<'\n';
  check(reachable>road*.98f,"coastal loop remains connected around coves and the lagoon");
  if(!map.empty()) {
    std::filesystem::create_directories(map.parent_path()); std::ofstream out(map,std::ios::binary);
    out<<"P6\n"<<coastSize<<' '<<coastSize<<"\n255\n";
    for(auto c : cells) {
      glm::ivec3 rgb=c.water ? glm::ivec3(25+6*c.ground,66+7*c.ground,105+5*c.ground)
        : c.road ? glm::ivec3(52,57,61) : c.pavement ? glm::ivec3(200,194,170)
        : c.beach ? glm::ivec3(232,215,166) : glm::ivec3(49+c.ground,88+c.ground*2,46+c.ground);
      for(int k=0;k<3;++k) out.put(char(std::clamp(rgb[k],0,255)));
    }
  }
}
void routes(World& w,Player& p) {
  auto o=*w.coastOrigin;
  check(!p.collides(w,p.pose.position) && !p.pose.flying && p.pose.position.y==23,"arrival is safe on a road");
  p.pose.position={o.x+180.5f,23,o.z+153.5f}; p.velocity={};
  walk(w,p,{o.x+180.5f,o.z+182.5f});
  check(p.pose.position.y<coastSeaLevel,"pier steps lead gently into the water");
  walk(w,p,{o.x+180.5f,o.z+153.5f});
  check(std::abs(p.pose.position.y-23)<.02f,"a swimmer can climb the pier steps back to the road");
  // A straight stretch on the northern road is wide enough to deliver and drive.
  check(visitCoast(w,p) && bringCar(w,p).empty(),"coast supports car delivery");
  auto pos=w.farm.car.position;
  check(std::abs(pos.y-23)<.01f,"car is parked on dry level road");
  auto direction=glm::normalize(pos+glm::vec3(0,.8f,0)-p.eye());
  p.pose.yaw=std::atan2(direction.x,-direction.z); p.pose.pitch=std::asin(direction.y);
  RideState ride; auto target=targetRanch(w,p);
  check(target && mountRanch(w,p,ride,*target),"delivered coastal car is within reach");
  Movement drive; drive.forward=1;
  for(int i=0;i<120;++i) tickRanch(w,p,ride,drive,1.f/60.f);
  check(glm::length(w.farm.car.position-pos)>14,"car delivery faces along the road with room to drive");
  w.farm.car.owned=false;
}
void persistence(const std::filesystem::path& map) {
  World w(7262026,true); Player p; p.pose.position={10.5f,24,7.5f};
  w.ensure({0,0},1); w.set({10,40,10},Block::Brick); check(initializeCity(w,p),"fixture includes old city");
  auto city=w.cityOrigin;
  check(visitCoast(w,p) && w.editCount()==1 && atCoast(w,p),"new coast is procedural and preserves prior edits");
  auto o=*w.coastOrigin;
  landscape(w,map); routes(w,p);
  check(w.get({10,40,10})==Block::Brick && w.cityOrigin==city,"farm builds and old city remain unchanged");
  check(w.streamingRadius(chunkAt(o.x+180,o.z+154))==coastViewRadius && w.streamingRadius({0,0})==viewRadius,"extra coastal view distance does not burden the farm");
  auto path=std::filesystem::temp_directory_path()/("blockworld-coast-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup { std::filesystem::path p; ~Cleanup(){std::error_code e;std::filesystem::remove(p,e);} } cleanup{path};
  Cell edit{o.x+180,28,o.z+151}; w.set(edit,Block::Glass); w.save(path,p.pose);
  check(std::filesystem::file_size(path)<2048,"large landscape does not bloat the save");
  World loaded; check(loaded.load(path).has_value() && loaded.coastOrigin==w.coastOrigin && loaded.cityOrigin==city,"coast and existing city survive reload");
  loaded.ensure(chunkAt(edit.x,edit.z),0); check(loaded.get(edit)==Block::Glass,"coastal player edits override generated terrain");
  auto remove=Cell{o.x+180,22,o.z+154}; loaded.ensure(chunkAt(remove.x,remove.z),0);
  check(breakBlock(loaded,remove),"pier is editable"); loaded.evict({0,0},1); loaded.ensure(chunkAt(remove.x,remove.z),0);
  check(loaded.get(remove)==Block::Air,"streaming cannot rebuild a demolished pier block");
  World protectedWorld; Cell prior{520,40,-185}; protectedWorld.ensure(chunkAt(prior.x,prior.z),0); protectedWorld.set(prior,Block::Brick);
  p.pose.position={10.5f,24,7.5f};
  check(initializeCoast(protectedWorld,p) && protectedWorld.coastOrigin->x==-896 && protectedWorld.get(prior)==Block::Brick,"existing edits reserve a proposed landscape site");
  World legacy(123,true); legacy.cityOrigin=city; legacy.save(path,p.pose);
  std::ifstream input(path); std::vector<std::string> lines; for(std::string line;std::getline(input,line);) lines.push_back(line);
  lines[0]="BLOCKWORLD 12 123 1"; lines.erase(lines.begin()+12);
  auto write=[&]{std::ofstream out(path);for(auto& line:lines) out<<line<<'\n';}; write();
  check(loaded.load(path).has_value() && !loaded.coastOrigin && loaded.cityOrigin==city,"v12 saves load with their original city");
  lines[0]="BLOCKWORLD 13 123 1"; lines.insert(lines.begin()+12,"1 160 18 -96"); write();
  bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
  check(rejected && !loaded.coastOrigin && loaded.cityOrigin==city,"overlapping landscape metadata rejects atomically");
  for(auto bad : {"1 512 60 -192","1 513 18 -192","1 100000 18 -192","0 512 18 -192"}) {
    lines[12]=bad; write(); rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && !loaded.coastOrigin,"invalid coastal metadata cannot mutate the world");
  }
}
}
int main(int argc,char** argv) {
  try { persistence(argc>1 ? argv[1] : ""); std::cout<<"PASS coast, connected roads, shore access, streaming, preservation and v12 migration\n"; }
  catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

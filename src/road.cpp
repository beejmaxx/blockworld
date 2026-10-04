#include "road.hpp"
#include "coast.hpp"
#include "city.hpp"
#include "harbor.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace bw {
namespace {
glm::vec2 xz(glm::vec3 p) { return {p.x,p.z}; }
bool nearSegment(glm::vec2 p,glm::vec2 a,glm::vec2 b,float margin) {
  return p.x>=std::min(a.x,b.x)-margin && p.x<=std::max(a.x,b.x)+margin
    && p.y>=std::min(a.y,b.y)-margin && p.y<=std::max(a.y,b.y)+margin;
}
int terrainGround(const World& w,int x,int z) {
  if(w.coastOrigin && coastContains(*w.coastOrigin,float(x),float(z)))
    return coastColumn(w.terrain,*w.coastOrigin,x,z).ground;
  return w.terrain.height(x,z);
}
}
RoadSample sampleRoad(const std::vector<glm::vec3>& points,float x,float z) {
  RoadSample result; glm::vec2 p{x,z}; float along=0;
  for(std::size_t i=1;i<points.size();++i) {
    auto a=xz(points[i-1]),b=xz(points[i]),delta=b-a;
    float length=glm::length(delta);
    if(length<.001f) continue;
    float t=std::clamp(glm::dot(p-a,delta)/(length*length),0.f,1.f);
    auto center=a+t*delta; float distance=glm::length(p-center);
    if(distance<result.distance) {
      result.distance=distance; result.along=along+t*length;
      result.height=std::lerp(points[i-1].y,points[i].y,t);
      result.center=center; result.direction=delta/length;
    }
    along+=length;
  }
  result.length=along; return result;
}
void generateRoad(Chunk& chunk,const std::vector<glm::vec3>& points) {
  if(points.size()<2) return;
  glm::vec2 base{chunk.pos.x*chunkSize,chunk.pos.z*chunkSize};
  bool nearby=false;
  for(std::size_t i=1;i<points.size();++i)
    nearby|=nearSegment(base+glm::vec2(8),xz(points[i-1]),xz(points[i]),roadClearance+8);
  if(!nearby) return;
  for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x) {
    auto p=base+glm::vec2(x+.5f,z+.5f); auto s=sampleRoad(points,p.x,p.y);
    if(s.distance>roadClearance) continue;
    int natural=0;
    for(int y=worldHeight-1;y>0;--y) {
      auto b=chunk.get(x,y,z);
      if(opaque(b) && b!=Block::Wood && b!=Block::Leaves) { natural=y; break; }
    }
    int top=int(std::round(s.height))-1;
    // A gravel verge and graded bank soften cuttings without trees hanging
    // over the carriageway. The route planner reserves this entire corridor.
    float blend=std::clamp((s.distance-roadHalfWidth-1)/(roadClearance-roadHalfWidth-1),0.f,1.f);
    int surface=int(std::round(std::lerp(float(top),float(natural),blend)));
    for(int y=1;y<worldHeight;++y) {
      Block b=y<=surface ? (y<surface-2 ? Block::Stone : Block::Dirt) : Block::Air;
      if(y==surface) {
        b=s.distance<=roadHalfWidth ? Block::Asphalt : s.distance<=roadHalfWidth+1 ? Block::Stone : Block::Grass;
        if(s.distance<.5f && int(s.along)%12<6) b=Block::Concrete;
        if(s.distance>roadHalfWidth-1 && s.distance<=roadHalfWidth) b=Block::Concrete;
      }
      chunk.set(x,y,z,b);
    }
    // Reflector posts sit beyond the shoulder, never in a lane.
    if(s.distance>7.8f && s.distance<8.6f && int(s.along)%32==16 && s.along>20 && s.along<s.length-20) {
      chunk.set(x,surface+1,z,Block::Charcoal);
      chunk.set(x,surface+2,z,Block::Lamp);
    }
  }
}
bool initializeRoad(World& w,const Player& player) {
  if(w.road.size()>=2) return true;
  if(!initializeHarbor(w,player) || !w.coastOrigin) return false;
  auto coast=*w.coastOrigin;
  std::unordered_map<ChunkPos,bool,PositionHash> clearance;
  auto clear=[&](int x,int z) {
    ChunkPos key{x,z};
    if(auto it=clearance.find(key);it!=clearance.end()) return it->second;
    constexpr int margin=12;
    bool ok=std::abs(x)<coordinateLimit-margin && std::abs(z)<coordinateLimit-margin;
    auto region=[&](int x0,int z0,int x1,int z1) {
      return x+margin>=x0 && x-margin<=x1 && z+margin>=z0 && z-margin<=z1;
    };
    if(w.castleOrigin) { auto o=*w.castleOrigin; ok&=!region(o.x-4,o.z-4,o.x+50,o.z+50); }
    if(w.cityOrigin) { auto o=*w.cityOrigin; ok&=!region(o.x,o.z,o.x+citySize-1,o.z+citySize-1); }
    for(const auto& b : harborBuildings()) {
      int width=b.turn%2 ? b.depth : b.width,depth=b.turn%2 ? b.width : b.depth;
      ok&=!region(coast.x+b.x-2,coast.z+b.z-2,coast.x+b.x+width+1,coast.z+b.z+depth+1);
    }
    if(w.farm.initialized) ok&=!region(int(w.farm.home.x)-8,int(w.farm.home.z)-10,int(w.farm.home.x)+8,int(w.farm.home.z)+10);
    if(w.farm.garden.initialized) { auto o=w.farm.garden.origin; ok&=!region(o.x,o.z,o.x+8,o.z+8); }
    if(ok) ok=!w.editedIn({x-margin,1,z-margin},{x+margin,worldHeight-1,z+margin});
    auto animal=[&](glm::vec3 p) { return region(int(p.x)-2,int(p.z)-2,int(p.x)+2,int(p.z)+2); };
    if(ok) for(const auto& a:w.farm.chickens) ok&=!animal(a.position);
    if(ok) for(const auto& a:w.farm.livestock) ok&=!animal(a.position) && !animal(a.home);
    if(ok && w.farm.car.owned) ok=!animal(w.farm.car.position);
    if(ok && coastContains(coast,float(x),float(z))) ok=!coastColumn(w.terrain,coast,x,z).water;
    clearance.emplace(key,ok); return ok;
  };
  auto lineClear=[&](glm::vec2 a,glm::vec2 b) {
    int n=std::max(1,int(std::ceil(glm::length(b-a)/4)));
    for(int i=0;i<=n;++i) {
      auto p=glm::mix(a,b,float(i)/n);
      if(!clear(int(std::round(p.x)),int(std::round(p.y)))) return false;
    }
    return true;
  };
  // Select a clear lay-by close to home and a point on the existing southern
  // coastal loop. Saved construction and all landmark parcels are obstacles.
  glm::vec2 home{w.farm.home.x,w.farm.home.z};
  ChunkPos start{},goal{}; float best=1e9f;
  for(int z=-8;z<=8;++z) for(int x=-8;x<=8;++x) {
    int wx=(int(home.x)/8+x)*8,wz=(int(home.y)/8+z)*8;
    float distance=glm::length(glm::vec2(wx,wz)-home);
    if(distance<best && distance>=20 && clear(wx,wz)) { best=distance; start={wx,wz}; }
  }
  if(best==1e9f) return false;
  best=1e9f;
  for(int z=280;z<=328;z+=8) for(int x=88;x<=156;x+=8) {
    int wx=coast.x+x,wz=coast.z+z;
    auto c=coastColumn(w.terrain,coast,wx,wz);
    float distance=glm::length(glm::vec2(wx,wz)-glm::vec2(start.x,start.z));
    if(c.road && distance<best && clear(wx,wz)) { best=distance; goal={wx,wz}; }
  }
  if(best==1e9f) return false;
  int x0=std::min(start.x,goal.x)-160,x1=std::max(start.x,goal.x)+160;
  int z0=std::min(start.z,goal.z)-160,z1=std::max(start.z,goal.z)+160;
  struct Entry {float estimate,cost;ChunkPos at; bool operator<(const Entry& b)const{return estimate>b.estimate;}};
  std::priority_queue<Entry> open;
  std::unordered_map<ChunkPos,float,PositionHash> costs;
  std::unordered_map<ChunkPos,ChunkPos,PositionHash> previous;
  auto estimate=[&](ChunkPos a){return glm::length(glm::vec2(a.x-goal.x,a.z-goal.z));};
  open.push({estimate(start),0,start}); costs[start]=0;
  bool found=false;
  while(!open.empty() && costs.size()<50000) {
    auto current=open.top();open.pop();
    if(current.cost>costs[current.at]) continue;
    if(current.at==goal) {found=true;break;}
    for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) {
      if(x==0 && z==0) continue;
      ChunkPos next{current.at.x+x*8,current.at.z+z*8};
      if(next.x<x0 || next.x>x1 || next.z<z0 || next.z>z1 || !clear(next.x,next.z)
          || !clear(current.at.x+x*4,current.at.z+z*4)) continue;
      float length=x && z ? 11.313708f : 8.f;
      float cost=current.cost+length*(1.f+.015f*std::abs(terrainGround(w,next.x,next.z)-23));
      if(!costs.contains(next) || cost<costs[next]) {
        costs[next]=cost; previous[next]=current.at; open.push({cost+estimate(next),cost,next});
      }
    }
  }
  if(!found) return false;
  std::vector<glm::vec2> path;
  for(auto at=goal;;at=previous.at(at)) {path.push_back({at.x,at.z});if(at==start)break;}
  std::ranges::reverse(path);
  std::vector<glm::vec2> simple{path.front()};
  for(std::size_t i=0;i+1<path.size();) {
    auto j=path.size()-1; while(j>i+1 && !lineClear(path[i],path[j])) --j;
    simple.push_back(path[j]); i=j;
  }
  // Cut corners into wide bends, accepting smoothing only when the full road
  // corridor is still clear. The fallback retains the protected A* route.
  auto smooth=simple;
  for(int pass=0;pass<3;++pass) {
    std::vector<glm::vec2> next{smooth.front()};
    for(std::size_t i=1;i<smooth.size();++i) {
      next.push_back(glm::mix(smooth[i-1],smooth[i],.25f));
      next.push_back(glm::mix(smooth[i-1],smooth[i],.75f));
    }
    next.push_back(smooth.back()); smooth=std::move(next);
  }
  for(std::size_t i=1;i<smooth.size();++i) if(!lineClear(smooth[i-1],smooth[i])) {smooth=simple;break;}
  std::vector<glm::vec3> road;
  road.push_back({smooth[0].x,0,smooth[0].y});
  for(std::size_t i=1;i<smooth.size();++i) {
    float length=glm::length(smooth[i]-smooth[i-1]);
    int steps=std::max(1,int(std::ceil(length/8)));
    for(int j=1;j<=steps;++j) {
      auto p=glm::mix(smooth[i-1],smooth[i],float(j)/steps);
      if(glm::length(p-xz(road.back()))>.1f) road.push_back({p.x,0,p.y});
    }
  }
  if(road.size()>roadPointLimit) return false;
  float startHeight=float(terrainGround(w,start.x,start.z)+1);
  float total=0; for(std::size_t i=1;i<road.size();++i)total+=glm::length(xz(road[i]-road[i-1]));
  float distance=0;
  for(std::size_t i=0;i<road.size();++i) {
    if(i)distance+=glm::length(xz(road[i]-road[i-1]));
    // Keep the highway gently graded rather than following every noisy hill.
    road[i].y=std::lerp(startHeight,23.f,std::clamp(distance/total,0.f,1.f));
  }
  w.road=std::move(road);
  std::vector<ChunkPos> loaded; for(const auto& [pos,chunk]:w.chunks)loaded.push_back(pos);
  for(auto pos:loaded)w.insert(w.terrain.generate(pos));
  return true;
}
bool visitRoad(World& w,Player& p) {
  if(!initializeRoad(w,p)) return false;
  auto sample=sampleRoad(w.road,w.road.front().x,w.road.front().z);
  // The same owned car is parked facing the city. Use the regular delivery
  // collision checks so edited road blocks or animals cannot be overwritten.
  Player scout; scout.pose.position=w.road.front(); scout.pose.position.y=std::round(scout.pose.position.y);
  scout.pose.position+=glm::vec3(-sample.direction.y,0,sample.direction.x)*2.5f;
  scout.pose.yaw=std::atan2(sample.direction.x,-sample.direction.y);
  w.ensure(chunkAt(int(scout.pose.position.x),int(scout.pose.position.z)),3);
  if(!bringCar(w,scout).empty()) return false;
  p=scout;
  auto d=glm::normalize(w.farm.car.position+glm::vec3(0,.7f,0)-p.eye());
  p.pose.yaw=std::atan2(d.x,-d.z);p.pose.pitch=std::asin(d.y);p.stopFlying();
  return true;
}
} // namespace bw

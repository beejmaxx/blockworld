#include "minimap.hpp"
#include "coast.hpp"
#include "city.hpp"
#include "harbor.hpp"
#include "countryside.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
glm::vec2 mapPoint(const MiniMap& map,glm::vec2 p) {return (p-map.center)/(2*map.radius)+glm::vec2(.5f);}
std::optional<std::array<glm::vec2,2>> mapLine(const MiniMap& map,glm::vec2 a,glm::vec2 b) {
  a=mapPoint(map,a);b=mapPoint(map,b);auto d=b-a;float lo=0,hi=1;
  for(int axis=0;axis<2;++axis) {
    if(std::abs(d[axis])<1e-7f) {if(a[axis]<0 || a[axis]>1)return {};}
    else {float u=-a[axis]/d[axis],v=(1-a[axis])/d[axis];lo=std::max(lo,std::min(u,v));hi=std::min(hi,std::max(u,v));}
  }
  if(lo>hi)return {};
  return std::array{glm::clamp(a+d*lo,glm::vec2(0),glm::vec2(1)),glm::clamp(a+d*hi,glm::vec2(0),glm::vec2(1))};
}
MiniMap buildMiniMap(const World& w,glm::vec2 player,bool overview,bool driving) {
  MiniMap map;map.enabled=true;map.player=map.center=player;map.radius=driving ? 200 : 96;
  if(overview) {
    glm::vec2 lo=player,hi=player;
    auto include=[&](glm::vec2 p){lo=glm::min(lo,p);hi=glm::max(hi,p);};
    include({w.farm.home.x,w.farm.home.z});
    for(auto p:w.road)include({p.x,p.z});
    if(w.coastOrigin) {auto o=*w.coastOrigin;include({o.x,o.z});include({o.x+coastSize,o.z+coastSize});}
    if(w.cityOrigin) {auto o=*w.cityOrigin;include({o.x,o.z});include({o.x+citySize,o.z+citySize});}
    if(w.castleOrigin) {auto o=*w.castleOrigin;include({o.x,o.z});include({o.x+50,o.z+50});}
    if(w.countrysideOrigin) {auto o=*w.countrysideOrigin;include({o.x,o.z});include({o.x+countrysideWidth,o.z+countrysideDepth});}
    map.center=(lo+hi)*.5f;map.radius=std::max(120.f,std::max(hi.x-lo.x,hi.y-lo.y)*.5f+35.f);
  }
  for(int z=0;z<mapCells;++z)for(int x=0;x<mapCells;++x) {
    auto p=map.center+(glm::vec2(x+.5f,z+.5f)/float(mapCells)-glm::vec2(.5f))*2.f*map.radius;
    int wx=int(std::floor(p.x)),wz=int(std::floor(p.y));
    auto color=glm::vec3(.28f,.43f,.25f);int height=w.terrain.height(wx,wz);
    if(w.coastOrigin && coastContains(*w.coastOrigin,p.x,p.y)) {
      auto c=coastColumn(w.terrain,*w.coastOrigin,wx,wz);height=c.ground;
      color=c.water ? glm::vec3(.16f,.39f,.49f) : c.road ? glm::vec3(.32f,.34f,.32f)
        : c.beach ? glm::vec3(.65f,.60f,.39f) : glm::vec3(.28f,.43f,.25f);
      if(w.harborLots)for(std::size_t i=0;i<harborBuildings().size();++i) {
        if(!(*w.harborLots&(1u<<i)))continue;
        auto b=harborBuildings()[i];int width=b.turn%2 ? b.depth : b.width,depth=b.turn%2 ? b.width : b.depth;
        if(wx>=w.coastOrigin->x+b.x && wx<w.coastOrigin->x+b.x+width && wz>=w.coastOrigin->z+b.z && wz<w.coastOrigin->z+b.z+depth)
          color=blockColor(b.wall)*.85f;
      }
    } else if(w.cityOrigin && wx>=w.cityOrigin->x && wx<w.cityOrigin->x+citySize && wz>=w.cityOrigin->z && wz<w.cityOrigin->z+citySize)
      color={.46f,.49f,.44f};
    if(w.countrysideOrigin) {
      auto o=*w.countrysideOrigin;
      if(wx>=o.x && wx<o.x+countrysideWidth && wz>=o.z && wz<o.z+countrysideDepth)
        color=(wz-o.z>=50 && std::abs(wx-o.x-56)>10) ? glm::vec3(.57f,.53f,.26f) : glm::vec3(.38f,.52f,.30f);
    }
    // Read loaded columns, including player construction, without generating
    // any extra chunks or keeping the whole city in memory for the map.
    if(auto chunk=w.chunks.find(chunkAt(wx,wz));chunk!=w.chunks.end()) {
      for(int y=worldHeight-1;y>0;--y) {
        auto b=chunk->second.get(localCoord(wx),y,localCoord(wz));
        if(b==Block::Air || b==Block::Torch || isCrop(b))continue;
        height=y;color=blockColor(b)*.78f;break;
      }
    }
    map.colors[z*mapCells+x]=color*(.88f+std::clamp(height,0,60)*.003f);
  }
  for(std::size_t i=1;i<w.road.size();++i)
    if(auto line=mapLine(map,{w.road[i-1].x,w.road[i-1].z},{w.road[i].x,w.road[i].z}))map.roads.push_back(*line);
  map.markers.push_back({w.terrain.adventure() ? glm::vec2(10.5f,-3.5f) : glm::vec2(w.farm.home.x,w.farm.home.z),"Home",{.98f,.83f,.40f}});
  if(w.coastOrigin) {auto o=*w.coastOrigin;map.markers.push_back({{o.x+225.f,o.z+354.f},"City",{.43f,.86f,1.f}});}
  if(w.castleOrigin) {auto o=*w.castleOrigin;map.markers.push_back({{o.x+25.f,o.z+25.f},"Castle",{.81f,.74f,.93f}});}
  if(w.countrysideOrigin) {
    auto o=*w.countrysideOrigin;map.markers.push_back({{o.x+56.f,o.z+44.f},"Farms",{.70f,.92f,.37f}});
    if(auto line=mapLine(map,{o.x+56.f,o.z-24.f},{o.x+56.f,o.z+95.f}))map.roads.push_back(*line);
  }
  if(w.farm.car.owned && !driving)map.markers.push_back({{w.farm.car.position.x,w.farm.car.position.z},"GT2",{1,.39f,.27f}});
  return map;
}
} // namespace bw

#include "metropolis.hpp"
#include "city_life.hpp"
#include "city.hpp"
#include "countryside.hpp"
#include "road.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr std::array<std::string_view,36> names{
  "Jade Heights","Harbor One","Cloud House","Civic Bank Tower","Pearl Exchange","Skyline Lofts","Azure House",
  "Orchid Tower","Northstar Offices","Glasshouse","Crown Plaza","Copper Heights","Emerald Offices","Summit House",
  "Garden Gate","Parkview West","Parkview East","Silverline","Bayview Tower",
  "Grand Avenue","Pacific Offices","Lantern House","Central Exchange","Cobalt Heights","Terrace One","Horizon House",
  "Southbank","Metro Offices","Rosewood Heights","Atlas Tower","Parkside Lofts",
  "Sunset House","Canopy Tower","Meridian Offices","Coral Heights","Skygarden"};
const auto towers=[] {
  std::array<HarborBuilding,36> out{};int i=0;
  constexpr std::array walls{Block::Concrete,Block::BlueGlass,Block::Limestone,Block::Terracotta,Block::Sage,Block::Charcoal};
  constexpr std::array trims{Block::Charcoal,Block::Concrete,Block::GoldTile,Block::Limestone,Block::BlueTile};
  for(int row=0;row<metroRows;++row)for(int col=0;col<metroColumns;++col) {
    if((row==2 && (col==2 || col==3)) || (row>=4 && col>=5))continue;
    int style=i%5,floors=10+(i*7+row*3)%9;
    if(style==4)floors=std::min(17,floors);
    if(row==0 && col==3){style=4;floors=17;}
    out[i]={col*64+11,row*64+11,36+(i%3)*3,36+(i%2)*4,floors,style,0,walls[i%walls.size()],trims[i%trims.size()],names[i]};++i;
  }
  return out;
}();
class Builder {
public:
  Chunk& chunk;Cell o;
  void fill(int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
    int bx=chunk.pos.x*chunkSize-o.x,bz=chunk.pos.z*chunkSize-o.z;
    x0=std::max(x0,bx);x1=std::min(x1,bx+15);z0=std::max(z0,bz);z1=std::min(z1,bz+15);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)for(int y=std::max(y0,1-o.y);y<=std::min(y1,worldHeight-1-o.y);++y)
      chunk.set(x-bx,y+o.y,z-bz,block);
  }
  void put(int x,int y,int z,Block block){fill(x,y,z,x,y,z,block);}
  void lamp(int x,int z) {fill(x,0,z,x,4,z,Block::Charcoal);fill(x-1,5,z,x+1,5,z,Block::Lamp);}
  void tree(int x,int z) {
    fill(x,0,z,x,4,z,Block::Wood);
    fill(x-2,4,z-2,x+2,6,z+2,Block::Leaves);fill(x-1,7,z-1,x+1,7,z+1,Block::Leaves);
  }
  void garage() {
    int x=5*64+7,z=5*64+8;
    fill(x,0,z,x+113,12,z+47,Block::Air);
    fill(x,-1,z,x+113,-1,z+47,Block::Charcoal);
    fill(x,0,z,x,6,z+47,Block::Concrete);fill(x+113,0,z,x+113,6,z+47,Block::Concrete);
    fill(x,0,z+47,x+113,6,z+47,Block::Glass);
    fill(x,0,z,x+50,6,z,Block::Glass);fill(x+69,0,z,x+113,6,z,Block::Glass);
    fill(x,7,z,x+113,7,z+47,Block::Concrete);
    for(int xx=x+4;xx<x+112;xx+=12)fill(xx,7,z+5,xx+6,7,z+42,Block::Glass);
    for(int xx:{x,x+50,x+69,x+113})fill(xx,0,z,xx,7,z,Block::Charcoal);
    fill(x+51,0,z,x+68,5,z,Block::Air);
    for(int n=0;n<garageSize;++n) {
      int xx=8+(n%10)*10+(n%10>=5 ? 10 : 0),zz=n<10 ? 12 : 34;
      fill(x+xx-3,-1,z+zz-3,x+xx-3,-1,z+zz+3,Block::Concrete);
      fill(x+xx+3,-1,z+zz-3,x+xx+3,-1,z+zz+3,Block::Concrete);
      fill(x+xx-3,-1,z+zz+(n<10 ? -3 : 3),x+xx+3,-1,z+zz+(n<10 ? -3 : 3),Block::Concrete);
      put(x+xx,6,z+zz,Block::Lamp);
    }
    fill(x+51,-1,z-8,x+68,-1,z+7,Block::Asphalt);
    fill(x+54,5,z-1,x+64,5,z-1,Block::GoldTile);
  }
  void dataCenter(int index) {
    int x=(5+index)*64+9,z=4*64+9;
    fill(x,-1,z,x+45,-1,z+45,Block::Concrete);
    fill(x,0,z,x+45,7,z+45,Block::Charcoal);fill(x+1,0,z+1,x+44,6,z+44,Block::Air);
    fill(x+2,2,z,x+43,5,z,Block::BlueGlass);
    fill(x+19,0,z,x+25,3,z,Block::Air);
    fill(x+2,2,z+45,x+43,5,z+45,Block::Glass);
    for(int xx=5;xx<=35;xx+=10)for(int zz=7;zz<=31;zz+=8) {
      fill(x+xx,0,z+zz,x+xx+3,3,z+zz+3,Block::Charcoal);
      for(int yy=0;yy<4;++yy) {
        fill(x+xx,yy,z+zz-1,x+xx+3,yy,z+zz-1,Block::BlueTile);
        put(x+xx+1,yy,z+zz-1,yy%2 ? Block::Lamp : Block::Sage);
      }
    }
    for(int xx=4;xx<40;xx+=8) {fill(x+xx,8,z+5,x+xx+4,9,z+11,Block::Concrete);fill(x+xx+1,10,z+6,x+xx+3,10,z+10,Block::Charcoal);}
    for(int zz:{4,20,38})for(int xx:{3,42})put(x+xx,6,z+zz,Block::Lamp);
    fill(x+18,4,z-1,x+26,4,z-1,Block::BlueTile);put(x+22,5,z-1,Block::Lamp);
  }
  void park() {
    fill(2*64+9,-1,2*64+9,4*64-9,-1,3*64-9,Block::Grass);
    fill(2*64+9,-1,2*64+28,4*64-9,-1,2*64+34,Block::Limestone);
    fill(3*64-3,-1,2*64+9,3*64+3,-1,3*64-9,Block::Limestone);
    fill(3*64-9,-1,2*64+20,3*64+9,-1,2*64+42,Block::BlueTile);
    fill(3*64-8,0,2*64+21,3*64+8,0,2*64+41,Block::Water);
    fill(3*64-1,0,2*64+29,3*64+1,3,2*64+33,Block::Limestone);put(3*64,4,2*64+31,Block::Lamp);
    for(int xx:{146,169,216,239})for(int zz:{144,175})tree(xx,zz);
    for(int xx:{153,219}){fill(xx,0,2*64+25,xx+5,0,2*64+25,Block::Sofa);put(xx+2,0,2*64+23,Block::Table);}
  }
};
bool land(World& w,Player& p,glm::vec3 at,glm::vec3 toward) {
  w.ensure(chunkAt(int(at.x),int(at.z)),2);
  if(p.collides(w,at) || !collidable(w.get({int(at.x),int(at.y)-1,int(at.z)})))return false;
  auto d=glm::normalize(toward-at-glm::vec3(0,1.6f,0));
  p.pose.position=at;p.pose.yaw=std::atan2(d.x,-d.z);p.pose.pitch=std::asin(d.y);p.stopFlying();p.velocity={};return true;
}
}
std::span<const HarborBuilding> metroBuildings(){return towers;}
bool metroContains(Cell o,float x,float z,float margin) {
  return x>=o.x-8-margin && x<o.x+metroWidth+8+margin && z>=o.z-8-margin && z<o.z+metroDepth+8+margin;
}
std::vector<glm::vec3> metroLink(const World& w) {
  if(!w.metroOrigin || !w.coastOrigin || w.road.empty())return {};
  auto o=*w.metroOrigin,c=*w.coastOrigin;
  auto s=sampleRoad(w.road,c.x+50.f,c.z+296.f);
  std::vector<glm::vec3> points{{s.center.x,s.height,s.center.y}};
  for(glm::vec3 p:std::array{glm::vec3(c.x+50.f,23,s.center.y),glm::vec3(c.x+50.f,23,o.z),glm::vec3(o.x+64.f,23,o.z)})
    if(glm::length(glm::vec2(p.x-points.back().x,p.z-points.back().z))>.1f)points.push_back(p);
  return points;
}
void generateMetropolis(Chunk& chunk,const World& w) {
  if(!w.metroOrigin)return;auto o=*w.metroOrigin;generateRoad(chunk,metroLink(w));
  int bx=chunk.pos.x*16-o.x,bz=chunk.pos.z*16-o.z;
  if(bx>=metroWidth+8 || bx+16<=-8 || bz>=metroDepth+8 || bz+16<=-8)return;
  Builder b{chunk,o};
  for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
    int xx=bx+x,zz=bz+z;
    if(xx< -8 || xx>=metroWidth+8 || zz< -8 || zz>=metroDepth+8)continue;
    int dx=std::abs(xx-int(std::round(xx/64.f))*64),dz=std::abs(zz-int(std::round(zz/64.f))*64);
    bool road=dx<=5 || dz<=5,walk=dx<=8 || dz<=8;
    Block surface=road ? Block::Asphalt : walk ? Block::Limestone : Block::Grass;
    if(road && ((dx==0 && dz>9 && zz%12<6) || (dz==0 && dx>9 && xx%12<6)))surface=Block::Concrete;
    if((dx>=7 && dx<=11 && dz<=5 && zz%2==0) || (dz>=7 && dz<=11 && dx<=5 && xx%2==0))surface=Block::Concrete;
    for(int y=1;y<worldHeight;++y)chunk.set(x,y,z,y<21 ? Block::Stone : y<22 ? Block::Dirt : y==22 ? surface : Block::Air);
  }
  for(int z=0;z<metroRows;++z)for(int x=0;x<metroColumns;++x) {
    b.lamp(x*64+7,z*64+20);b.lamp(x*64+57,z*64+46);
    b.tree(x*64+56,z*64+12);b.tree(x*64+11,z*64+56);
  }
  for(std::size_t i=0;i<towers.size();++i) {
    auto tower=towers[i];generateHarborTower(chunk,o,tower);
    if(metroOffice(i))for(int floor=0;floor<tower.floors;++floor) {
      int x=tower.x,z=tower.z,y=floor*harborFloorHeight;
      b.fill(x+11,y,z+2,x+tower.width-2,y+3,z+tower.depth-2,Block::Air);
      for(int xx=17;xx<tower.width-6;xx+=8)for(int zz=6;zz<tower.depth-6;zz+=10) {
        b.fill(x+xx,y,z+zz,x+xx+3,y,z+zz+1,Block::Table);
        b.put(x+xx+1,y+1,z+zz,Block::BlueGlass);b.put(x+xx+1,y,z+zz+3,Block::Chair);
        b.put(x+xx+3,y+3,z+zz+1,Block::Lamp);
      }
      b.put(x+tower.width-3,y,z+tower.depth-3,Block::Planter);
    }
  }
  b.park();b.garage();b.dataCenter(0);b.dataCenter(1);
  // A recognizable ATM/counter inside the bank's tall street-level lobby.
  auto t=towers[3];b.fill(t.x+17,0,t.z+28,t.x+27,0,t.z+28,Block::Limestone);
  b.fill(t.x+20,0,t.z+25,t.x+23,2,t.z+25,Block::Charcoal);
  b.fill(t.x+20,2,t.z+26,t.x+23,2,t.z+26,Block::BlueTile);b.put(t.x+21,1,t.z+26,Block::GoldTile);
}
bool initializeMetropolis(World& w,const Player& p) {
  if(w.metroOrigin)return true;
  if(!initializeRoad(w,p) || !w.coastOrigin)return false;auto c=*w.coastOrigin;
  for(int shift:{0,64,128,192}) {
    Cell o{c.x+32,harborGround,c.z+coastSize+16+shift};
    if(std::abs(o.x)>coordinateLimit-metroWidth-16 || std::abs(o.z)>coordinateLimit-metroDepth-16)continue;
    auto in=[&](glm::vec3 v){return metroContains(o,v.x,v.z,4);};
    if(in(p.pose.position) || in(w.farm.home) || (w.farm.car.owned && in(w.farm.car.position)))continue;
    if(w.editedIn(o+Cell{-12,1-o.y,-12},o+Cell{metroWidth+12,worldHeight-o.y-1,metroDepth+12}))continue;
    bool blocked=false;
    if(w.countrysideOrigin){auto f=*w.countrysideOrigin;blocked=o.x-12<f.x+countrysideWidth+8 && o.x+metroWidth+12>f.x-8 && o.z-12<f.z+countrysideDepth+8 && o.z+metroDepth+12>f.z-24;}
    auto overlaps=[&](Cell at,int width,int depth){return o.x-12<at.x+width+8 && o.x+metroWidth+12>at.x-8 && o.z-12<at.z+depth+8 && o.z+metroDepth+12>at.z-8;};
    if(w.cityOrigin)blocked|=overlaps(*w.cityOrigin,citySize,citySize);
    if(w.castleOrigin)blocked|=overlaps(*w.castleOrigin,50,50);
    for(auto a:w.farm.chickens)blocked|=in(a.position);
    for(auto a:w.farm.livestock)blocked|=in(a.position)||in(a.home);
    if(blocked)continue;
    w.metroOrigin=o;auto link=metroLink(w);
    for(std::size_t i=1;i<link.size();++i) {
      auto lo=glm::min(link[i-1],link[i]),hi=glm::max(link[i-1],link[i]);
      blocked|=w.editedIn({int(lo.x)-11,1,int(lo.z)-11},{int(hi.x)+11,worldHeight-1,int(hi.z)+11});
    }
    if(blocked){w.metroOrigin.reset();continue;}
    w.cityLife.rentDay=w.clock.day;
    std::vector<ChunkPos> loaded;
    for(auto& [pos,chunk]:w.chunks)if(metroContains(o,float(pos.x*16),float(pos.z*16),16) || sampleRoad(link,pos.x*16+8.f,pos.z*16+8.f).distance<24)loaded.push_back(pos);
    for(auto pos:loaded)w.insert(w.terrain.generate(pos));
    return true;
  }
  return false;
}
bool visitMetropolis(World& w,Player& p,bool roof) {
  if(!initializeMetropolis(w,p))return false;auto o=*w.metroOrigin;
  auto b=towers[3];auto at=roof ? harborPosition(o,b,{7.5f,float(b.floors*5),13.5f}) : glm::vec3(o.x+192.f,23,o.z+62.f);
  return land(w,p,at,roof ? at+glm::vec3(100,-15,100) : glm::vec3(o.x+244.f,80,o.z+134.f));
}
glm::vec3 garagePosition(Cell o,int car) {
  return {o.x+5*64+7.f+8+(car%10)*10+(car%10>=5 ? 10 : 0),23,o.z+5*64+8.f+(car<10 ? 12 : 34)};
}
bool visitGarage(World& w,Player& p) {
  if(!initializeMetropolis(w,p))return false;
  auto car=parkedCar(w,w.cityLife.activeCar);w.ensure(chunkAt(int(car.position.x),int(car.position.z)),1);
  auto at=car.position+glm::vec3(4,0,w.cityLife.activeCar<10 ? 5 : -5);
  if(!land(w,p,at,car.position+glm::vec3(0,.7f,0)))return false;
  if(carFits(w,car))w.farm.car=car;
  return true;
}
bool visitMetroProperty(World& w,Player& p,int index) {
  if(index<0 || index>=int(towers.size()) || !initializeMetropolis(w,p))return false;
  auto b=towers[index];auto o=*w.metroOrigin;
  auto at=harborPosition(o,b,{12.5f,0,float(b.depth)+3.5f});
  return land(w,p,at,at+glm::vec3(0,3,-20));
}
glm::vec3 bankTerminal(Cell o){auto b=towers[3];return {o.x+b.x+21.5f,24.5f,o.z+b.z+26.5f};}
bool visitBank(World& w,Player& p) {
  if(!initializeMetropolis(w,p))return false;auto target=bankTerminal(*w.metroOrigin);
  return land(w,p,{target.x,23,target.z+3.5f},target);
}
bool visitDataCenter(World& w,Player& p,int index) {
  if(index<0 || index>1 || !initializeMetropolis(w,p))return false;auto o=*w.metroOrigin;
  return land(w,p,{o.x+(5+index)*64+31.f,23,o.z+4*64+6.f},{o.x+(5+index)*64+31.f,25,o.z+4*64+36.f});
}
glm::vec3 metroResidentHome(Cell o,int i) {
  auto b=towers[std::size_t(i)];return harborPosition(o,b,{float(b.width)-10.5f,float(1+i%4)*5,float(b.depth)-8.5f});
}
std::vector<Vertex> metropolisSkyline(const World& w,glm::vec3 eye) {
  std::vector<Vertex> mesh;if(!w.metroOrigin)return mesh;auto o=*w.metroOrigin;
  auto box=[&](glm::vec3 a,glm::vec3 b,float material,float light=.9f){appendBox(mesh,{glm::vec3(o.x,0,o.z)+a,glm::vec3(o.x,0,o.z)+b},{-100006,0,0},material,light);};
  // A coarse continuation of the real terrain grounds the distant skyline.
  // Full, editable chunks cover this mesh near the player; it has no collision.
  auto surface=[&](int x,int z) {
    float height=w.terrain.height(x,z)+1.f,material=1;
    if(w.coastOrigin && coastContains(*w.coastOrigin,float(x),float(z))) {
      auto c=coastColumn(w.terrain,*w.coastOrigin,x,z);
      height=c.water ? float(coastSeaLevel) : c.ground+1.f;
      material=c.water ? 66 : c.road ? 65 : c.pavement ? 61 : c.beach ? 4 : 1;
    }
    if(metroContains(o,float(x),float(z)))height=23;
    if(w.countrysideOrigin && countrysideContains(*w.countrysideOrigin,float(x),float(z)))height=float(w.countrysideOrigin->y);
    return glm::vec2(height-1.2f,material);
  };
  constexpr int step=24;
  int startX=int(std::floor((eye.x-900)/step))*step,startZ=int(std::floor((eye.z-900)/step))*step;
  for(int z=startZ;z<startZ+1800;z+=step)for(int x=startX;x<startX+1800;x+=step) {
    float distance=glm::length(glm::vec2(x+step*.5f-eye.x,z+step*.5f-eye.z));
    if(distance<80 || distance>880)continue;
    auto a=surface(x,z),b=surface(x,z+step),c=surface(x+step,z+step),d=surface(x+step,z);
    float material=surface(x+step/2,z+step/2).y;
    std::array<glm::vec3,4> v{{{x,a.x,z},{x,b.x,z+step},{x+step,c.x,z+step},{x+step,d.x,z}}};
    constexpr std::array<glm::vec2,4> uv{{{0,0},{0,1},{1,1},{1,0}}};
    for(int i:{0,1,2,0,2,3})mesh.push_back({v[i],uv[i],material,material==66 ? 5.f : 1.f,{-100008,0,0}});
  }
  box({-8,18,-8},{metroWidth+8.f,22.98f,metroDepth+8.f},108,1);
  for(auto b:towers) {
    float distance=glm::length(glm::vec2(o.x+b.x+b.width*.5f-eye.x,o.z+b.z+b.depth*.5f-eye.z));
    if(distance<155 || distance>850)continue;
    // Solid, inset facade proxies retain floor rhythm and crowns at long range.
    // Nearby editable voxel geometry covers them during the streaming transition.
    float roof=23+b.floors*5;
    box({b.x+.15f,23,b.z+.15f},{b.x+b.width-.15f,roof,b.z+b.depth-.15f},100+float(b.style));
    for(int floor=0;floor<=b.floors;++floor)box({b.x-.7f,22.8f+floor*5,b.z-.7f},{b.x+b.width+.7f,23.f+floor*5,b.z+b.depth+.7f},cityMaterial(b.trim));
    for(int x=0;x<=b.width;x+=6)box({b.x+x-.22f,23,b.z-.18f},{b.x+x+.22f,roof,b.z+.12f},cityMaterial(b.trim));
    box({b.x+5.f,roof,b.z+4.f},{b.x+b.width-5.f,roof+4,b.z+b.depth-10.f},100+float(b.style));
    if(b.style==4)for(int y=5;y<19;y+=2) {
      float r=std::max(1,10-(y-5)*9/13);
      box({b.x+b.width*.5f-r,roof+y,b.z+b.depth*.5f-r},{b.x+b.width*.5f+r,roof+y+2,b.z+b.depth*.5f+r},68);
    }
  }
  return mesh;
}
} // namespace bw

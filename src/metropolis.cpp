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
  void apartment(const HarborBuilding& t,int resident) {
    int x=t.x,z=t.z,w=t.width,d=t.depth,y=(1+resident%4)*harborFloorHeight;
    constexpr std::array accents{Block::Sage,Block::BlueTile,Block::Terracotta,Block::GoldTile,Block::BlueTile,Block::RedTile,Block::Sage,Block::Terracotta};
    Block accent=accents[resident];
    auto f=[&](int a,int b,int c,int aa,int bb,int cc,Block block){fill(x+a,y+b,z+c,x+aa,y+bb,z+cc,block);};
    auto p=[&](int a,int b,int c,Block block){f(a,b,c,a,b,c,block);};
    // Keep the stairs, their landing, and the balcony doors clear.
    f(15,0,2,w-2,3,d-2,Block::Air);
    f(16,-1,3,w-3,-1,9,Block::Limestone);
    f(17,0,2,w-3,0,2,Block::Concrete);
    f(17,1,2,w-3,1,2,accent);
    p(18,1,2,Block::Charcoal);p(19,1,2,Block::Charcoal);
    p(w-8,1,2,Block::BlueTile);p(w-4,1,2,Block::Planter);
    f(w-3,0,3,w-3,2,4,Block::Concrete);p(w-3,1,4,Block::Charcoal);
    f(20,0,6,23,0,7,Block::Table);
    for(int a:{20,23}){p(a,0,5,Block::Chair);p(a,0,9,Block::Chair);}
    // A dining area separates the kitchen from a spacious lounge.
    f(18,-1,13,24,-1,18,accent);f(19,-1,14,23,-1,17,Block::Limestone);
    f(20,0,15,22,0,16,Block::Table);
    for(int a:{20,22}){p(a,0,14,Block::Chair);p(a,0,18,Block::Chair);}
    p(24,0,14,Block::Planter);
    f(17,-1,d-12,23,-1,d-4,accent);f(18,-1,d-11,22,-1,d-5,Block::Limestone);
    f(18,0,d-6,22,0,d-6,Block::Sofa);
    f(19,0,d-9,21,0,d-9,Block::Table);
    p(17,0,d-9,Block::Chair);p(23,0,d-9,Block::Chair);
    p(17,0,d-6,Block::Table);p(23,0,d-6,Block::Planter);
    // A book wall and framed artwork give each room a recognizable backdrop.
    f(16,0,d-4,16,2,d-2,Block::Planks);
    f(18,1,d-2,22,2,d-2,accent);
    // Bedroom divider stops short of the route into the room.
    f(w-7,0,d-7,w-7,2,d-3,accent);
    f(w-6,-1,d-7,w-2,-1,d-2,Block::Limestone);
    for(int a:{w-5,w-4}){p(a,0,d-4,Block::BedZ);p(a,0,d-5,Block::BedZHead);}
    p(w-3,0,d-4,Block::Table);p(w-3,1,d-4,Block::Lamp);
    p(w-6,0,d-3,Block::Planter);
    // Two tables anchor a detailed crib; it can be removed or rebuilt normally.
    f(w-6,-1,d-15,w-2,-1,d-10,accent);
    f(w-5,0,d-13,w-4,0,d-13,Block::Table);
    p(w-3,0,d-15,Block::Chair);p(w-2,0,d-12,Block::Planter);
    for(auto spot:std::array{glm::ivec2(21,7),glm::ivec2(21,16),glm::ivec2(21,d-8),glm::ivec2(w-5,d-13)})p(spot.x,3,spot.y,Block::Lamp);
    p(16,0,2,Block::Planter);p(w-2,0,8,Block::Planter);
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
    if(i<residentCount)b.apartment(tower,int(i));
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
void appendApartmentDecor(std::vector<Vertex>& mesh,const World& w,int i) {
  if(!w.metroOrigin || i<0 || i>=residentCount)return;
  auto t=towers[i];auto o=*w.metroOrigin;int floor=(1+i%4)*harborFloorHeight;
  glm::vec3 origin(o.x+t.x,o.y+floor,o.z+t.z);
  auto anchor=[&](int x,int z,Block expected){return w.get({int(origin.x)+x,int(origin.y),int(origin.z)+z})==expected;};
  auto box=[&](glm::vec3 a,glm::vec3 b,float material){appendBox(mesh,{origin+a,origin+b},{-100010,0,i},material,.95f);};
  int d=t.depth,width=t.width;
  if(anchor(16,d-3,Block::Planks)) {
    for(int row=0;row<3;++row) {
      float y=.18f+row*.84f;
      box({16.97f,y-.06f,d-3.95f},{17.12f,y,d-1.05f},72);
      for(int book=0;book<12;++book){float z=d-3.9f+book*.235f;box({16.99f,y,z},{17.16f,y+.48f+(book%3)*.08f,z+.17f},80+float((book+i*3+row)%20));}
    }
  }
  if(w.get({int(origin.x)+20,int(origin.y)+1,int(origin.z)+d-2})!=Block::Air) {
    float z=d-2.035f;
    box({18.18f,1.08f,z-.04f},{22.82f,2.92f,z},72);
    box({18.30f,1.20f,z-.065f},{22.70f,2.80f,z-.05f},73);
    for(int band=0;band<5;++band) {
      float x=18.45f+band*.82f,h=.45f+float((band*3+i)%5)*.21f;
      box({x,1.34f,z-.085f},{x+.67f,1.34f+h,z-.075f},80+float((i*3+band+1)%20));
    }
  }
  if(anchor(17,d-6,Block::Table)) {
    box({17.44f,.58f,d-5.56f},{17.56f,1.85f,d-5.44f},71);
    box({17.18f,1.61f,d-5.82f},{17.82f,2.03f,d-5.18f},73);
  }
  if(anchor(width-5,d-13,Block::Table) && anchor(width-4,d-13,Block::Table)) {
    float x=width-5.f,z=d-13.f;
    box({x+.08f,.59f,z+.07f},{x+1.92f,.68f,z+.93f},73);
    for(float xx:{x+.06f,x+1.90f})box({xx,.56f,z+.03f},{xx+.05f,1.17f,z+.97f},60);
    for(float zz:{z+.04f,z+.91f}) {
      box({x+.06f,1.09f,zz},{x+1.95f,1.17f,zz+.05f},60);
      for(int bar=0;bar<10;++bar){float xx=x+.14f+bar*.18f;box({xx,.64f,zz},{xx+.04f,1.1f,zz+.04f},60);}
    }
  }
  if(anchor(21,15,Block::Table)) {
    box({20.25f,.59f,15.25f},{22.75f,.615f,15.75f},73);
    for(int fruit=0;fruit<4;++fruit){float x=21.f+fruit*.18f;box({x,.62f,15.40f},{x+.14f,.77f,15.57f},fruit%2 ? 70 : 71);}
  }
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

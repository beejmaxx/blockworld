#include "estate.hpp"
#include "metropolis.hpp"
#include "countryside.hpp"
#include "city.hpp"
#include "road.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr std::array places{
  EstatePlace{"Seabreeze villa","Glass, warm wood, a pool and a roof garden.",{60.5f,0,212.5f},{60,3,242}},
  EstatePlace{"Palm court mansion","Limestone courtyard, double-height lounge and terraces.",{264.5f,0,212.5f},{264,4,244}},
  EstatePlace{"Sunset residence","Cantilevered suites overlooking the marina.",{472.5f,0,212.5f},{473,5,240}},
  EstatePlace{"Your yacht","Walk aboard from the pier; lounge, cabins and sun deck.",{390.5f,0,349.5f},{370,5,350}},
  EstatePlace{"City airport","Terminal, departures lounge, runway and control tower.",{299.5f,0,173.5f},{299,5,130}},
  EstatePlace{"Private hangars","Parked aircraft beside the airport apron.",{472.5f,0,172.5f},{472,4,126}}
};
class Builder {
public:
  Chunk& chunk;Cell o;
  void f(int x0,int y0,int z0,int x1,int y1,int z1,Block block) {
    int bx=chunk.pos.x*16-o.x,bz=chunk.pos.z*16-o.z;
    x0=std::max(x0,bx);x1=std::min(x1,bx+15);z0=std::max(z0,bz);z1=std::min(z1,bz+15);
    for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)for(int y=std::max(y0,1-o.y);y<=std::min(y1,worldHeight-1-o.y);++y)chunk.set(x-bx,y+o.y,z-bz,block);
  }
  void p(int x,int y,int z,Block b){f(x,y,z,x,y,z,b);}
  void tree(int x,int z) {
    f(x,0,z,x,7,z,Block::Wood);
    f(x-3,7,z-1,x+3,8,z+1,Block::Leaves);f(x-1,7,z-3,x+1,8,z+3,Block::Leaves);
    for(int a:{-3,3}){f(x+a,6,z-1,x+a,6,z+1,Block::Leaves);f(x-1,6,z+a,x+1,6,z+a,Block::Leaves);}
  }
  void lamp(int x,int z){f(x,0,z,x,4,z,Block::Charcoal);p(x,5,z,Block::Lamp);}
  void house(int x,int z,int w,int d,int style) {
    if(x+w+8<chunk.pos.x*16-o.x || x-8>chunk.pos.x*16-o.x+15 || z+d+36<chunk.pos.z*16-o.z || z-20>chunk.pos.z*16-o.z+15)return;
    Block wall=style==1 ? Block::Limestone : Block::Concrete,accent=style==2 ? Block::Terracotta : style==1 ? Block::Sage : Block::Planks;
    auto q=[&](int a,int b,int c,int aa,int bb,int cc,Block block){f(x+a,b,z+c,x+aa,bb,z+cc,block);};
    auto put=[&](int a,int b,int c,Block block){q(a,b,c,a,b,c,block);};
    q(-5,-1,-5,w+5,-1,d+30,Block::Limestone);
    q(23,-1,-28,35,-1,-1,Block::Asphalt);
    q(3,-1,-28,20,-1,-6,Block::Charcoal);
    q(3,-1,-28,35,-1,-23,Block::Asphalt);
    for(int a:{7,14})q(a,-1,-21,a,-1,-10,Block::Limestone);
    q(0,-1,0,w-1,-1,d-1,Block::Planks);
    // A grounded podium, recessed glass and projecting upper volumes.
    q(0,0,0,w-1,4,0,wall);q(0,0,d-1,w-1,4,d-1,Block::Glass);
    q(0,0,0,0,4,d-1,Block::Glass);q(w-1,0,0,w-1,4,d-1,Block::Glass);
    for(int a:{0,16,34,w-1})q(a,0,0,a,5,d-1,wall);
    // Only the facade piers occupy these lines; rooms remain open behind them.
    for(int a:{16,34})q(a,0,1,a,4,d-2,Block::Air);
    q(3,1,0,21,3,0,Block::Glass);q(36,1,0,w-4,3,0,Block::Glass);
    q(26,0,0,30,3,0,Block::Air);
    q(0,5,0,w-1,5,d-1,wall);
    q(3,5,15,22,5,d-4,Block::Air); // Double-height living room.
    q(8,6,2,w-1,10,2,Block::Glass);q(8,6,d-1,w-1,10,d-1,Block::Glass);
    q(8,6,2,8,10,d-1,Block::Glass);q(w-1,6,2,w-1,10,d-1,Block::Glass);
    for(int a:{8,25,35,w-1}){q(a,6,2,a,11,2,wall);q(a,6,d-1,a,11,d-1,wall);}
    q(7,11,1,w+1,11,d+1,wall);q(4,11,14,22,11,d-4,Block::Glass);
    q(0,6,0,7,6,0,Block::Glass);q(0,6,0,0,6,d-1,Block::Glass);q(0,6,d-1,7,6,d-1,Block::Glass);
    q(8,6,10,8,8,13,Block::Air); // Bedroom terrace doorway.
    q(0,0,4,0,4,13,accent);q(w-1,6,5,w-1,10,13,accent);
    if(style==1){q(9,11,3,w-2,11,10,Block::GoldTile);q(10,11,4,w-3,11,9,Block::Glass);}
    if(style==2){q(w-2,5,18,w+5,5,d+1,wall);q(w+5,6,18,w+5,6,d+1,Block::Glass);q(w-1,6,23,w-1,8,26,Block::Air);}
    if(style==1) {
      for(int a:{18,39})q(a,0,-5,a+1,7,-4,Block::Limestone);
      q(17,8,-6,41,8,1,Block::Limestone);q(21,8,-4,37,8,-1,Block::Glass);
      for(int a:{5,w-7}){q(a,0,-4,a+2,1,-2,Block::Limestone);put(a+1,2,-3,Block::Planter);}
    } else if(style==2) {
      for(int c=5;c<d-4;c+=3)q(w,6,c,w,10,c,Block::Planks);
      q(22,4,-4,35,4,0,Block::Charcoal);q(24,4,-3,33,4,-1,Block::Glass);
    } else {
      q(24,4,-3,33,4,0,Block::Planks);put(24,3,-2,Block::Lamp);put(33,3,-2,Block::Lamp);
    }
    // Two broad half-block stair flights: ground -> bedrooms -> roof garden.
    q(26,5,20,28,5,31,Block::Air);q(30,11,20,32,11,31,Block::Air);
    for(int n=0;n<12;++n) {
      int half=n+1,yy=half/2;
      q(26,half%2 ? yy : yy-1,20+n,28,half%2 ? yy : yy-1,20+n,half%2 ? Block::StoneSlab : wall);
      q(30,6+(half%2 ? yy : yy-1),31-n,32,6+(half%2 ? yy : yy-1),31-n,half%2 ? Block::StoneSlab : wall);
    }
    // Lounge, media wall, dining and fitted kitchen.
    q(4,-1,19,22,-1,d-5,Block::Sage);q(5,-1,20,21,-1,d-6,Block::Limestone);
    q(6,0,29,17,0,29,Block::Sofa);q(6,0,23,6,0,28,Block::Sofa);
    q(10,0,25,14,0,26,Block::Table);q(9,0,16,18,0,16,accent);
    q(11,1,16,17,3,16,Block::Charcoal);q(12,2,16,16,2,16,Block::BlueGlass);
    q(38,0,3,w-3,0,3,wall);q(38,1,3,w-3,1,3,accent);
    q(w-3,0,4,w-3,0,14,wall);put(w-3,1,8,Block::BlueTile);
    q(39,0,8,44,0,10,wall);q(39,1,8,44,1,10,Block::StoneSlab);
    put(41,1,8,Block::Charcoal);put(43,1,8,Block::Charcoal);
    q(w-5,0,4,w-4,3,5,Block::Concrete);q(w-5,1,5,w-4,1,5,Block::Charcoal);
    q(38,0,23,44,0,25,Block::Table);
    for(int a:{38,41,44}){put(a,0,21,Block::Chair);put(a,0,27,Block::Chair);}
    // Upstairs suite, study and bathroom; hall stays clear beside the stairs.
    q(36,6,3,36,9,15,accent);q(37,6,16,w-2,9,16,wall);q(41,6,16,44,8,16,Block::Air);
    q(37,5,3,w-2,5,15,Block::Limestone);
    q(40,6,5,47,6,8,wall);q(41,6,6,46,6,7,Block::Water); // Bath.
    q(w-4,6,10,w-3,6,13,wall);put(w-3,7,11,Block::BlueTile);
    q(w-2,7,10,w-2,8,13,Block::BlueGlass);
    q(10,6,3,21,8,3,accent);q(11,7,4,20,7,4,Block::Charcoal); // Wardrobe.
    q(10,5,5,23,5,13,Block::Limestone);
    for(int a:{14,15,16}){put(a,6,9,Block::BedZ);put(a,6,8,Block::BedZHead);}
    for(int a:{12,18}){put(a,6,8,Block::Table);put(a,7,8,Block::Lamp);}
    q(39,6,32,45,6,32,Block::Table);put(42,7,32,Block::Charcoal);put(42,6,30,Block::Chair);
    q(39,6,20,47,6,20,Block::Sofa);
    // Nursery with three separate cribs; keep the study and stair hall accessible.
    for(int a:{39,42,45})q(a,6,36,a+1,6,36,Block::Table);
    put(48,6,36,Block::Planter);put(48,9,36,Block::Lamp);
    // Roof garden, shaded pergola and outdoor seating.
    q(7,12,1,w+1,12,1,Block::Glass);q(7,12,d+1,w+1,12,d+1,Block::Glass);
    q(7,12,1,7,12,d+1,Block::Glass);q(w+1,12,1,w+1,12,d+1,Block::Glass);
    for(int a:{37,w-4})for(int c:{5,14})q(a,12,c,a,15,c,accent);
    for(int c=4;c<=15;c+=2)q(36,16,c,w-3,16,c,accent);
    q(39,12,12,46,12,12,Block::Sofa);q(41,12,9,44,12,9,Block::Table);
    for(int a=10;a<24;a+=3)put(a,12,2,Block::Planter);
    // Pool terrace and an open route from the rear doors to the waterfront.
    q(26,0,d-1,30,3,d-1,Block::Air);
    q(8,-3,d+5,27,-2,d+20,Block::BlueTile);q(9,-1,d+6,26,-1,d+19,Block::Water);
    q(8,-1,d+5,27,-1,d+5,Block::StoneSlab);q(8,-1,d+20,27,-1,d+20,Block::StoneSlab);
    for(int a:{32,36,40})q(a,0,d+9,a+1,0,d+12,Block::Sofa);
    for(int a:{5,w-5})for(int c:{5,d-5}){put(a,0,c,Block::Planter);put(a,4,c,Block::Lamp);}
    for(int c:{6,18,34}){put(24,4,c,Block::Lamp);put(w-6,10,c,Block::Lamp);}
    for(int a:{x-4,x+w+4}){tree(a,z-6);tree(a,z+d+24);}
  }
  void yacht() {
    // A tapered displacement hull, teak decks and stepped white superstructure.
    int cx=370;
    for(int z=316;z<=377;++z) {
      int radius=std::min(11,3+(z-316)/2);
      f(cx-radius+2,-5,z,cx+radius-2,-3,z,Block::Charcoal);
      f(cx-radius,-2,z,cx+radius,-1,z,Block::Concrete);
      f(cx-radius+1,-1,z,cx+radius-1,-1,z,Block::Planks);
      if(z<375){p(cx-radius,0,z,Block::Glass);p(cx+radius,0,z,Block::Glass);}
    }
    f(362,0,334,378,3,365,Block::Glass);f(363,0,335,377,3,364,Block::Air);
    f(361,4,333,379,4,366,Block::Concrete);f(363,0,365,367,2,365,Block::Air);
    f(378,0,357,378,2,361,Block::Air);
    f(374,0,365,377,3,365,Block::Air); // Gangway enters lounge.
    f(382,-1,357,395,-1,361,Block::Planks);
    f(381,0,357,381,0,361,Block::Air);
    f(390,-1,300,397,-1,390,Block::Planks);f(345,-1,297,410,-1,305,Block::Limestone);
    for(int z=310;z<390;z+=12){f(391,-7,z,391,-2,z,Block::Wood);lamp(397,z);}
    // Ground deck salon and forward cabin.
    f(364,0,357,364,0,363,Block::Sofa);f(368,0,360,371,0,361,Block::Table);
    f(376,0,347,376,0,353,Block::Concrete);p(376,1,351,Block::BlueTile);
    f(363,0,344,377,2,344,Block::Limestone);f(369,0,344,371,2,344,Block::Air);
    for(int x:{367,368,369}){p(x,0,339,Block::BedZ);p(x,0,338,Block::BedZHead);}
    p(365,0,339,Block::Table);p(365,1,339,Block::Lamp);
    f(364,5,339,376,7,351,Block::Glass);f(365,5,340,375,7,350,Block::Air);
    f(363,8,338,377,8,352,Block::Concrete);f(365,5,351,367,7,351,Block::Air);
    f(365,5,340,375,5,340,Block::Charcoal);p(369,6,340,Block::BlueTile);p(371,6,340,Block::BlueTile);p(370,5,343,Block::Chair);
    // Exterior stairs connect aft deck to flybridge, then sun roof.
    f(374,4,354,377,4,363,Block::Air);
    for(int n=0;n<10;++n){int h=n+1,y=h/2;f(374,h%2?y:y-1,364-n,377,h%2?y:y-1,364-n,h%2?Block::StoneSlab:Block::Concrete);}
    f(374,4,353,377,4,354,Block::Concrete);
    f(365,5,358,370,5,358,Block::Sofa);f(366,5,361,369,5,361,Block::Table);
    for(int n=0;n<8;++n){int h=n+1,y=5+h/2;f(362,h%2?y:y-1,350-n,363,h%2?y:y-1,350-n,h%2?Block::StoneSlab:Block::Concrete);}
    f(366,9,347,371,9,347,Block::Sofa);f(368,9,341,369,12,342,Block::Charcoal);f(366,12,340,372,12,340,Block::Concrete);
    for(int z:{332,373}){f(365,0,z,369,0,z,Block::Sofa);p(374,0,z,Block::Planter);}
  }
  void plane(int x,int z,int size=1) {
    // Parked business jet, clear of the runway and taxi route.
    for(int zz=0;zz<31;++zz){int r=zz<4?1:zz>26?1:2;f(x-r,2,z+zz,x+r,4,z+zz,Block::Concrete);}
    f(x-1,3,z+2,x+1,3,z+5,Block::BlueGlass);
    for(int n=0;n<12;++n){f(x-3-n,2,z+13+n/2,x+3+n,2,z+14+n/2,Block::Concrete);}
    f(x,4,z+25,x,10,z+29,Block::BlueTile);f(x-7,4,z+26,x+7,4,z+28,Block::Concrete);
    for(int a:{-4,4}){f(x+a,2,z+23,x+a,4,z+28,Block::Charcoal);f(x+a,3,z+23,x+a,3,z+27,Block::Concrete);}
    for(int a:{-2,2})p(x+a,0,z+18,Block::Charcoal);p(x,0,z+6,Block::Charcoal);
    for(int zz=7;zz<22;zz+=3){p(x-2,3,z+zz,Block::BlueGlass);p(x+2,3,z+zz,Block::BlueGlass);}
    (void)size;
  }
  void airport() {
    f(18,-1,32,620,-1,80,Block::Asphalt);
    f(18,-1,32,620,-1,32,Block::Concrete);f(18,-1,80,620,-1,80,Block::Concrete);
    for(int x=34;x<607;x+=28)f(x,-1,55,x+12,-1,57,Block::Concrete);
    for(int z=37;z<=74;z+=5){f(32,-1,z,52,-1,z+2,Block::Concrete);f(586,-1,z,606,-1,z+2,Block::Concrete);}
    for(int x=24;x<622;x+=24){p(x,-1,30,Block::Lamp);p(x,-1,82,Block::Lamp);}
    f(160,-1,92,545,-1,163,Block::Charcoal);f(208,-1,78,229,-1,97,Block::Asphalt);f(552,-1,78,573,-1,163,Block::Asphalt);
    f(165,-1,95,540,-1,95,Block::GoldTile);
    // Terminal: an open arrivals hall, ticket desks and a furnished departure lounge.
    f(246,-1,110,350,-1,164,Block::Limestone);
    f(249,0,112,347,8,160,Block::Glass);f(250,0,113,346,8,159,Block::Air);
    f(246,9,109,350,9,164,Block::Concrete);f(259,9,122,335,9,147,Block::Glass);
    for(int x=250;x<=346;x+=16){f(x,0,112,x,9,112,Block::Concrete);f(x,0,160,x,9,160,Block::Concrete);}
    f(293,0,160,306,4,160,Block::Air);f(248,0,124,249,4,133,Block::Air);
    f(287,0,111,306,4,112,Block::Air);f(293,-1,164,306,-1,185,Block::Limestone);
    for(int x=255;x<=277;x+=6){f(x,0,148,x+3,0,148,Block::Concrete);p(x+1,1,148,Block::BlueGlass);}
    for(int x=311;x<=335;x+=8)for(int z:{124,134,144}){f(x,0,z,x+5,0,z,Block::Sofa);p(x+3,0,z+3,Block::Table);}
    for(int x:{252,343})for(int z:{117,155})p(x,0,z,Block::Planter);
    f(296,5,158,303,6,158,Block::Charcoal);f(297,5,158,302,5,158,Block::GoldTile);
    for(int x=256;x<345;x+=12)for(int z:{119,151})p(x,8,z,Block::Lamp);
    // Control tower with a real stairwell, full glazing and equipment.
    HarborBuilding tower{366,108,22,24,5,3,0,Block::Concrete,Block::Charcoal,"Airport control"};generateHarborTower(chunk,o,tower);
    f(366,25,108,387,28,131,Block::BlueGlass);f(367,25,109,386,28,130,Block::Air);f(365,29,107,388,29,132,Block::Concrete);
    f(368,24,113,374,24,119,Block::Air);f(378,25,110,384,25,110,Block::Charcoal);
    for(int x:{379,382})p(x,26,110,Block::BlueTile);
    // Two open-front hangars.
    for(int x:{434,492}) {
      f(x,0,112,x+45,12,157,Block::Charcoal);f(x+1,0,113,x+44,11,157,Block::Air);
      f(x-1,13,111,x+46,13,158,Block::Concrete);f(x+5,13,119,x+39,13,143,Block::Glass);
      f(x+5,0,157,x+40,9,158,Block::Air);plane(x+22,119);
    }
    plane(186,118);plane(222,118);
    for(int x=260;x<344;x+=10){f(x,-1,170,x,-1,180,Block::Concrete);}
  }
};
}
std::span<const EstatePlace> estatePlaces(){return places;}
bool estateContains(Cell o,float x,float z,float margin){return x>=o.x-margin && x<o.x+estateWidth+margin && z>=o.z-margin && z<o.z+estateDepth+margin;}
std::vector<glm::vec3> estateRoad(const World& w) {
  if(!w.metroOrigin || !w.estateOrigin)return {};
  auto m=*w.metroOrigin,o=*w.estateOrigin;
  return {{m.x+float(metroWidth),23,m.z+192.f},{o.x,23,o.z+192.f},{o.x+640.f,23,o.z+192.f}};
}
void generateEstate(Chunk& c,const World& w) {
  if(!w.estateOrigin)return;auto o=*w.estateOrigin;generateRoad(c,estateRoad(w));
  if(!estateContains(o,c.pos.x*16.f,c.pos.z*16.f,16))return;
  Builder b{c,o};int bx=c.pos.x*16-o.x,bz=c.pos.z*16-o.z;
  for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
    int xx=bx+x,zz=bz+z;if(xx<0 || xx>=estateWidth || zz<0 || zz>=estateDepth)continue;
    bool water=zz>=310;int ground=water ? 14 : zz>=300 ? 20 : 22;
    for(int y=1;y<worldHeight;++y)c.set(x,y,z,y<ground-2 ? Block::Stone : y<ground ? Block::Dirt : y==ground ? (zz>=297 ? Block::Sand : Block::Grass) : water && y<=21 ? Block::Water : Block::Air);
  }
  b.f(0,-1,184,639,-1,200,Block::Asphalt);b.f(0,-1,181,639,-1,183,Block::Limestone);b.f(0,-1,201,639,-1,204,Block::Limestone);
  for(int x=0;x<640;x+=16)b.f(x,-1,191,x+7,-1,192,Block::Concrete);
  b.f(0,-1,293,639,-1,298,Block::Limestone);b.f(388,-1,203,399,-1,307,Block::Limestone);
  for(int x=12;x<640;x+=32){b.tree(x,206);b.lamp(x,182);b.tree(x,291);}
  // Garden courts between the houses break up the broad plots with paths,
  // fountains and shaded seating, while leaving the driving lane clear.
  for(int x:{155,555}) {
    b.f(x-3,-1,204,x+3,-1,295,Block::Limestone);
    b.f(x-20,-1,249,x+20,-1,257,Block::Limestone);
    b.f(x-14,-1,233,x-6,-1,242,Block::BlueTile);b.f(x-13,0,234,x-7,0,241,Block::Water);
    b.f(x-11,0,236,x-9,2,238,Block::Limestone);b.p(x-10,3,237,Block::Lamp);
    for(int xx:{x-22,x+22})for(int zz:{224,246,274})b.tree(xx,zz);
    b.f(x+7,0,239,x+16,0,239,Block::Sofa);b.f(x+10,0,236,x+13,0,236,Block::Table);
  }
  b.house(32,220,56,40,0);b.house(236,220,64,44,1);b.house(444,220,60,40,2);
  b.airport();b.yacht();
}
bool initializeEstate(World& w,const Player& p) {
  if(w.estateOrigin)return true;if(!initializeMetropolis(w,p))return false;
  auto m=*w.metroOrigin;
  for(int shift:{0,640,1280,1920}) {
    Cell o{m.x+metroWidth+32+shift,23,m.z};
    if(std::abs(o.x)>coordinateLimit-estateWidth-32 || std::abs(o.z)>coordinateLimit-estateDepth-32)continue;
    auto overlaps=[&](Cell a,int width,int depth){return o.x-16<a.x+width && o.x+estateWidth+16>a.x && o.z-16<a.z+depth && o.z+estateDepth+16>a.z;};
    if((w.coastOrigin && overlaps(*w.coastOrigin,coastSize,coastSize)) || (w.cityOrigin && overlaps(*w.cityOrigin,citySize,citySize)) || (w.countrysideOrigin && overlaps(*w.countrysideOrigin,countrysideWidth,countrysideDepth)) || (w.castleOrigin && overlaps(*w.castleOrigin,50,50)))continue;
    auto crossesLink=[&](Cell a,int width,int depth){return m.x+metroWidth+9<a.x+width && o.x>a.x && m.z+180<a.z+depth && m.z+204>a.z;};
    if((w.coastOrigin && crossesLink(*w.coastOrigin,coastSize,coastSize)) || (w.cityOrigin && crossesLink(*w.cityOrigin,citySize,citySize)) || (w.countrysideOrigin && crossesLink(*w.countrysideOrigin,countrysideWidth,countrysideDepth)) || (w.castleOrigin && crossesLink(*w.castleOrigin,50,50)))continue;
    auto occupied=[&](glm::vec3 a){return estateContains(o,a.x,a.z,16);};
    bool blocked=occupied(p.pose.position)||occupied(w.farm.home)||(w.farm.car.owned&&occupied(w.farm.car.position));
    for(auto a:w.farm.chickens)blocked|=occupied(a.position);
    for(auto a:w.farm.livestock)blocked|=occupied(a.position)||occupied(a.home);
    if(blocked || w.editedIn(o+Cell{-16,1-o.y,-16},o+Cell{estateWidth+16,worldHeight-o.y-1,estateDepth+16}))continue;
    if(w.editedIn({m.x+metroWidth+9,1,m.z+180},{o.x-1,worldHeight-1,o.z+204}))continue;
    w.estateOrigin=o;
    std::vector<ChunkPos> reload;for(auto& [pos,chunk]:w.chunks)if(estateContains(o,pos.x*16.f,pos.z*16.f,16) || sampleRoad(estateRoad(w),pos.x*16+8.f,pos.z*16+8.f).distance<24)reload.push_back(pos);
    for(auto pos:reload)w.insert(w.terrain.generate(pos));return true;
  }
  return false;
}
bool visitEstate(World& w,Player& p,int index) {
  if(index<0 || index>=int(places.size()) || !initializeEstate(w,p))return false;
  auto o=*w.estateOrigin;auto at=glm::vec3(o.x,o.y,o.z)+places[index].arrival;
  w.ensure(chunkAt(int(at.x),int(at.z)),2);
  if(p.collides(w,at) || !collidable(w.get({int(at.x),int(at.y)-1,int(at.z)})))return false;
  p.pose.position=at;p.stopFlying();auto d=glm::normalize(glm::vec3(o.x,o.y,o.z)+places[index].target-p.eye());
  p.pose.yaw=std::atan2(d.x,-d.z);p.pose.pitch=std::asin(d.y);return true;
}
}

#include "city_life.hpp"
#include "estate.hpp"
#include "adventure.hpp"
#include <algorithm>

namespace bw {
namespace {
constexpr std::array<ServerContract,4> contracts{{
  {"Local websites",2,160},{"Shop network",2,224},
  {"Research lab",4,512},{"Cloud gaming",8,1152}
}};
bool clearStanding(const World& w,const Player& p,glm::vec3 at) {
  return !p.collides(w,at) && collidable(w.get({int(at.x),int(at.y)-1,int(at.z)}));
}
void refreshSite(World& w,int site) {
  auto o=*w.metroOrigin;int x=o.x+(5+site)*64+9,z=o.z+265;
  std::vector<ChunkPos> loaded;
  for(auto& [pos,chunk]:w.chunks)if(pos.x*16+15>=x && pos.x*16<=x+45 && pos.z*16+15>=z && pos.z*16<=z+45)loaded.push_back(pos);
  // insert reapplies every saved edit after generating the upgraded equipment.
  for(auto pos:loaded)w.insert(w.terrain.generate(pos));
}
}
glm::vec3 mansionPosition(const World& w,int home,glm::vec3 local) {
  if(!w.estateOrigin || home<0 || home>2)return {};
  auto o=*w.estateOrigin;constexpr std::array x{32,236,444};
  return glm::vec3(o.x+x[home],o.y,o.z+220)+local;
}
glm::vec3 cityResidentHome(const World& w,int resident) {
  if(!w.metroOrigin || resident<0 || resident>=residentCount)return {};
  int house=w.cityLife.residents[resident].home;
  return house<0 ? metroResidentHome(*w.metroOrigin,resident) : mansionPosition(w,house,{40.5f,6,26.5f});
}
glm::vec3 cityCribPosition(const World& w,int resident,int child) {
  int home=w.cityLife.residents[resident].home;
  return home<0 ? cityResidentHome(w,resident)+glm::vec3(6,0,-4)
    : mansionPosition(w,home,{39.5f+child*3,6,36.5f});
}
bool visitCityHome(World& w,Player& p) {
  return w.cityLife.home<0 ? visitHome(w,p) : visitEstate(w,p,w.cityLife.home);
}
bool chooseCityHome(World& w,Player& p,int home) {
  if(home<-1 || home>2)return false;
  if(!(home<0 ? visitHome(w,p) : visitEstate(w,p,home)))return false;
  w.cityLife.home=home;return true;
}
std::string moveCityHousehold(World& w,const Player& p,int resident,int home) {
  if(resident<0 || resident>=residentCount || home<-1 || home>2 || !w.metroOrigin)return "Choose a resident and a home first.";
  auto& person=w.cityLife.residents[resident];
  if(person.home==home)return "This household already lives here.";
  if(home>=0 && !person.dating)return "Start dating before inviting a partner to move in.";
  if(home>=0 && !initializeEstate(w,p))return "The mansion district needs a clear parcel.";
  if(home>=0)for(int i=0;i<residentCount;++i)if(i!=resident && w.cityLife.residents[i].home==home)
    return std::string(cityResidents()[i].name)+" already lives here. Choose another mansion.";
  int previous=person.home;person.home=home;
  auto at=cityResidentHome(w,resident);w.ensure(chunkAt(int(at.x),int(at.z)),2);
  std::array<glm::vec3,3> children{};
  for(int c=0;c<3;++c)if(person.children[c])children[c]=cityChildPosition(w,resident,c);
  person.home=previous; // Collision checks must not collide with the household being moved.
  if(std::abs(p.pose.position.x-at.x)<.65f && std::abs(p.pose.position.z-at.z)<.65f
    && std::abs(p.pose.position.y-at.y)<1.8f)return "Step away from the family area before moving the household.";
  bool clear=clearStanding(w,p,at) && clearStanding(w,p,at+glm::vec3(0,0,-2.5f));
  for(int c=0;c<3;++c)if(person.children[c]) {
    auto child=children[c];
    if(w.clock.day!=person.children[c])clear&=clearStanding(w,p,child);
    else clear&=!collidable(w.get({int(child.x),int(child.y)+1,int(child.z)}));
  }
  if(!clear)return "Clear the family area and the arrival before moving in.";
  person.home=home;
  return std::string(cityResidents()[resident].name)+(cityChildCount(person) ? " and the children moved to " : " moved to ")
    +(home<0 ? "the original apartment." : std::string(estatePlaces()[home].name)+".");
}
std::string parkHomeCar(World& w,const Player& p,int home) {
  if(home<0 || home>2 || !initializeEstate(w,p))return "Choose a mansion first.";
  auto& city=w.cityLife;auto old=city.homeCars;
  for(auto& car:city.homeCars)if(car==city.activeCar)car=-1;
  city.homeCars[home]=city.activeCar;
  auto car=parkedCar(w,city.activeCar);w.ensure(chunkAt(int(car.position.x),int(car.position.z)),1);
  if(!carFits(w,car) || glm::length(p.pose.position-car.position)<4) {
    city.homeCars=old;return "Clear the parking bay and step away before parking here.";
  }
  w.farm.car=car;
  return std::string(garageCars()[city.activeCar].name)+" parked outside. Walk over and press V to drive.";
}
void appendMansionNursery(std::vector<Vertex>& mesh,const World& w,int house) {
  if(!w.estateOrigin)return;
  for(int c=0;c<3;++c) {
    auto at=mansionPosition(w,house,{39.f+c*3,6,36});
    if(!w.chunks.contains(chunkAt(int(at.x),int(at.z))) || w.get({int(at.x),int(at.y),int(at.z)})!=Block::Table)continue;
    auto box=[&](glm::vec3 a,glm::vec3 b,float material){appendBox(mesh,{at+a,at+b},{-100010,2,house*3+c},material,.95f);};
    box({.06f,.59f,.07f},{1.94f,.68f,.93f},73);
    for(float x:{.05f,1.90f})box({x,.56f,.03f},{x+.05f,1.18f,.97f},60);
    for(float z:{.04f,.91f}) {
      box({.06f,1.1f,z},{1.95f,1.18f,z+.05f},60);
      for(int bar=0;bar<10;++bar){float x=.14f+bar*.18f;box({x,.64f,z},{x+.04f,1.1f,z+.04f},60);}
    }
  }
}
std::span<const ServerContract> serverContracts(){return contracts;}
DataCenterReport dataCenterReport(const DataCenterState& s) {
  DataCenterReport r;r.capacity=std::min(s.power,s.cooling)*4;
  for(int i=0;i<int(contracts.size());++i)if(s.contracts&(1<<i)){r.used+=contracts[i].racks;r.revenue+=contracts[i].revenue;}
  r.electricity=s.racks*8;r.cooling=r.used*8;r.profit=r.revenue-r.electricity-r.cooling;return r;
}
int serverUpgradePrice(const DataCenterState& s,ServerUpgrade upgrade) {
  return upgrade==ServerUpgrade::Rack ? 300 : upgrade==ServerUpgrade::Power ? s.power*600 : s.cooling*450;
}
std::string upgradeDataCenter(World& w,const Player& p,int site,ServerUpgrade upgrade) {
  if(!w.metroOrigin || site<0 || site>1)return "Visit the city first.";
  auto& s=w.cityLife.dataCenters[site];auto report=dataCenterReport(s);
  int level=upgrade==ServerUpgrade::Rack ? s.racks : upgrade==ServerUpgrade::Power ? s.power : s.cooling;
  if(level>=(upgrade==ServerUpgrade::Rack ? 16 : 4))return "This upgrade is already at maximum capacity.";
  if(upgrade==ServerUpgrade::Rack && s.racks>=report.capacity)return "Upgrade power and cooling before installing more racks.";
  int price=serverUpgradePrice(s,upgrade);collectCityIncome(w);
  if(w.cityLife.bank<price)return "Not enough in the bank. Deposit coins or collect daily income.";
  auto o=*w.metroOrigin;int x=o.x+(5+site)*64+9,z=o.z+265;
  Cell lo,hi;
  if(upgrade==ServerUpgrade::Rack){lo={x+5+(s.racks%4)*10,o.y,z+6+(s.racks/4)*8};hi=lo+Cell{3,3,4};}
  else if(upgrade==ServerUpgrade::Power){lo={x+4+s.power*5,o.y,z+41};hi=lo+Cell{2,3,2};}
  else {lo={x+4+s.cooling*8,o.y+8,z+5};hi=lo+Cell{4,2,6};}
  w.ensure(chunkAt(lo.x,lo.z),1);
  std::vector<Cell> cleared;
  bool construction=false;
  for(int z=lo.z;z<=hi.z;++z)for(int y=lo.y;y<=hi.y;++y)for(int x=lo.x;x<=hi.x;++x) {
    Cell c{x,y,z};if(!w.edited(c))continue;
    if(w.get(c)!=Block::Air)construction=true;else cleared.push_back(c);
  }
  // Refuse to overwrite construction or materialize hardware around the player.
  if(construction || (p.pose.position.x>lo.x-.4f && p.pose.position.x<hi.x+1.4f
    && p.pose.position.z>lo.z-.4f && p.pose.position.z<hi.z+1.4f && p.pose.position.y<hi.y+1 && p.pose.position.y+1.8f>lo.y))
    return "Clear the equipment bay of construction and stand back before installing.";
  if(upgrade==ServerUpgrade::Rack)++s.racks;
  else if(upgrade==ServerUpgrade::Power)++s.power;
  else ++s.cooling;
  // Buying equipment may fill explicitly cleared cells, but never erase construction.
  std::unordered_map<ChunkPos,Chunk,PositionHash> equipment;
  for(auto c:cleared) {
    auto pos=chunkAt(c.x,c.z);
    if(!equipment.contains(pos)){auto chunk=w.terrain.generate(pos);generateMetropolis(chunk,w);equipment.emplace(pos,std::move(chunk));}
    auto block=equipment.at(pos).get(localCoord(c.x),c.y,localCoord(c.z));
    if(block!=Block::Air)w.set(c,block);
  }
  w.cityLife.bank-=price;recordCityTransaction(w,BankKind::BusinessPurchase,price);refreshSite(w,site);
  return upgrade==ServerUpgrade::Rack ? "Rack installed. Sign a contract to put it to work." : "Capacity upgraded. You can install more racks.";
}
std::string signServerContract(World& w,int site,int contract) {
  if(!w.metroOrigin || site<0 || site>1 || contract<0 || contract>=int(contracts.size()))return "Choose a customer first.";
  auto& s=w.cityLife.dataCenters[site];
  if(s.contracts&(1<<contract))return "This customer is already paying you.";
  if(s.racks-dataCenterReport(s).used<contracts[contract].racks)return "Install enough free racks before signing this contract.";
  // Settle previous days with the old configuration; upgrades never pay retroactively.
  collectCityIncome(w);s.contracts|=1<<contract;
  return std::string(contracts[contract].name)+" signed. New profits arrive next game day.";
}
glm::vec3 dataCenterTerminal(Cell o,int site){return {o.x+(5+site)*64+40.5f,float(o.y)+1.5f,o.z+269.5f};}
} // namespace bw

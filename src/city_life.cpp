#include "city_life.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bw {
namespace {
const std::array<CarSpec,garageSize> cars{{
  {"Porsche 911 GT2","Track coupe",0,{.83f,.055f,.035f}},
  {"Azure Roadster","Open roadster",1,{.06f,.48f,.85f}},
  {"Onyx GT","Grand tourer",2,{.09f,.10f,.13f}},
  {"Alpine Rally","Rally hatch",3,{.85f,.89f,.91f}},
  {"Solar Sprint","Wedge supercar",4,{1.f,.65f,.04f}},
  {"British Classic","Classic coupe",5,{.12f,.35f,.23f}},
  {"Scarlet Spider","Open roadster",1,{.82f,.04f,.12f}},
  {"Silver Arrow","Grand tourer",2,{.61f,.68f,.72f}},
  {"Cobalt Rally","Rally hatch",3,{.09f,.16f,.72f}},
  {"Lime Vector","Wedge supercar",4,{.47f,.83f,.10f}},
  {"Copper Classic","Classic coupe",5,{.65f,.29f,.13f}},
  {"Violet Roadster","Open roadster",1,{.45f,.17f,.69f}},
  {"Pearl GT","Grand tourer",2,{.90f,.87f,.78f}},
  {"Desert Rally","Rally hatch",3,{.67f,.52f,.30f}},
  {"Glacier Vector","Wedge supercar",4,{.30f,.79f,.85f}},
  {"Midnight Classic","Classic coupe",5,{.15f,.18f,.25f}},
  {"Sunset Spider","Open roadster",1,{.94f,.30f,.06f}},
  {"Emerald GT","Grand tourer",2,{.05f,.48f,.30f}},
  {"Cherry Rally","Rally hatch",3,{.58f,.04f,.13f}},
  {"Platinum Vector","Wedge supercar",4,{.73f,.77f,.80f}}
}};
constexpr std::array<ResidentSpec,residentCount> residents{{
  {"Mei","architecture and rooftop gardens"},{"Ava","cars and coastal drives"},
  {"Sofia","cooking with fresh vegetables"},{"Lena","music and city photography"},
  {"Nora","technology and game design"},{"Iris","painting and quiet parks"},
  {"Zoe","horses and countryside walks"},{"Maya","coffee shops and books"}
}};
void record(World& w,BankKind kind,int amount) {
  if(amount<=0)return;
  auto& entries=w.cityLife.statement;entries.push_back({kind,amount,w.clock.day});
  if(entries.size()>8)entries.erase(entries.begin());
}
std::optional<float> ray(glm::vec3 origin,glm::vec3 direction,Box box,float reach) {
  float lo=0,hi=reach;
  for(int axis=0;axis<3;++axis) {
    if(std::abs(direction[axis])<1e-6f) {if(origin[axis]<box.min[axis] || origin[axis]>box.max[axis])return {};}
    else {float a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];lo=std::max(lo,std::min(a,b));hi=std::min(hi,std::max(a,b));}
  }
  return lo<=hi ? std::optional(lo) : std::nullopt;
}
Box personBox(glm::vec3 p){return {p+glm::vec3(-.32f,0,-.24f),p+glm::vec3(.32f,1.8f,.24f)};}
bool overlap(Box a,Box b) {return a.max.x>b.min.x && a.min.x<b.max.x && a.max.y>b.min.y && a.min.y<b.max.y && a.max.z>b.min.z && a.min.z<b.max.z;}
}
std::span<const CarSpec> garageCars(){return cars;}
std::span<const ResidentSpec> cityResidents(){return residents;}
FarmCar parkedCar(const World& w,int i) {
  if(!w.metroOrigin || i<0 || i>=garageSize)return {};
  return {true,garagePosition(*w.metroOrigin,i),i<10 ? 3.14159265f : 0.f};
}
bool bankTransfer(World& w,int amount,bool deposit) {
  auto& balance=w.cityLife.bank;auto& wallet=w.farm.garden.coins;
  if(amount<=0)return false;
  if(deposit) {if(amount>wallet || amount>bankLimit-balance)return false;wallet-=amount;balance+=amount;}
  else {if(amount>balance || amount>coinLimit-wallet)return false;balance-=amount;wallet+=amount;}
  record(w,deposit ? BankKind::Deposit : BankKind::Withdrawal,amount);return true;
}
int propertyRentPerDay(const World& w) {
  if(!w.metroOrigin)return 0;int rent=0;
  for(std::size_t i=0;i<metroBuildings().size();++i)rent+=metroBuildings()[i].floors*(metroOffice(i) ? 6 : 4);
  return rent;
}
int serverIncomePerDay(const World& w){return w.metroOrigin ? 256 : 0;}
void collectCityIncome(World& w) {
  auto& city=w.cityLife;
  if(!w.metroOrigin || w.clock.day<=city.rentDay)return;
  auto days=std::uint64_t(w.clock.day-city.rentDay);
  auto credit=[&](BankKind kind,int daily) {
    int amount=int(std::min<std::uint64_t>(bankLimit-city.bank,days*std::uint64_t(daily)));
    city.bank+=amount;record(w,kind,amount);
  };
  credit(BankKind::Rent,propertyRentPerDay(w));credit(BankKind::Servers,serverIncomePerDay(w));city.rentDay=w.clock.day;
}
std::string talkToResident(World& w,int index,bool askOut) {
  if(!w.metroOrigin || index<0 || index>=residentCount)return "Visit the city to meet its residents.";
  auto& r=w.cityLife.residents[index];auto name=std::string(residents[index].name);
  if(askOut) {
    if(r.dating)return name+": I'm glad you came over. Let's spend some time together.";
    if(r.conversations<2)return name+": Let's get to know each other a little first.";
    r.dating=true;return name+": Yes, I'd love to go out with you! Come visit me again.";
  }
  r.conversations=std::min(3,r.conversations+1);
  if(r.conversations==1)return name+": Hi! I'm 18. Welcome to my apartment.";
  if(r.conversations==2)return name+": I love "+std::string(residents[index].interest)+".";
  return name+(r.dating ? ": It's lovely seeing you again." : ": I'd enjoy getting to know you better.");
}
bool visitResident(World& w,Player& p,int index) {
  if(index<0 || index>=residentCount || !initializeMetropolis(w,p))return false;
  auto home=metroResidentHome(*w.metroOrigin,index);auto at=home+glm::vec3(0,0,-2.5f);
  w.ensure(chunkAt(int(at.x),int(at.z)),2);
  if(p.collides(w,at))return false;
  p.pose.position=at;p.pose.yaw=3.14159265f;p.pose.pitch=-.05f;p.stopFlying();p.velocity={};return true;
}
bool takeGarageCar(World& w,Player& p,RideState& ride,int index) {
  if(index<0 || index>=garageSize || !w.metroOrigin)return false;
  auto car=parkedCar(w,index);w.ensure(chunkAt(int(car.position.x),int(car.position.z)),1);
  int previous=w.cityLife.activeCar;w.cityLife.activeCar=index;
  if(!carFits(w,car)) {w.cityLife.activeCar=previous;return false;}
  w.farm.car=car;ride={true,true,0};p.pose.position=car.position+glm::vec3(0,.32f,0);
  p.pose.yaw=car.yaw;p.pose.pitch=-.12f;p.stopFlying();p.velocity={};return true;
}
std::optional<CityTarget> targetCity(const World& w,const Player& p,float reach) {
  if(!w.metroOrigin)return {};float nearest=reach;
  if(auto hit=w.raycast(p.eye(),p.direction(),reach))nearest=hit->distance+.02f;
  std::optional<CityTarget> found;
  auto test=[&](Box box,CityTargetKind kind,int index){if(auto distance=ray(p.eye(),p.direction(),box,nearest)){nearest=*distance;found=CityTarget{kind,index,nearest};}};
  for(int i=0;i<residentCount;++i)test(personBox(metroResidentHome(*w.metroOrigin,i)),CityTargetKind::Resident,i);
  for(int i=0;i<garageSize;++i)if(i!=w.cityLife.activeCar)test(carBounds(parkedCar(w,i)),CityTargetKind::Car,i);
  auto b=bankTerminal(*w.metroOrigin);test({b-glm::vec3(1.5f,1.5f,.65f),b+glm::vec3(1.5f,1.5f,.65f)},CityTargetKind::Bank,0);
  return found;
}
std::string cityPrompt(const World& w,CityTarget target) {
  if(target.kind==CityTargetKind::Bank)return "Civic Bank / V to open your account";
  if(target.kind==CityTargetKind::Car)return std::string(cars[target.index].name)+" / V to drive";
  return std::string(residents[target.index].name)+" / 18 / "+(w.cityLife.residents[target.index].dating ? "Girlfriend / " : "")+"V to talk";
}
bool cityPeopleOverlap(const World& w,Box box) {
  if(!w.metroOrigin)return false;
  for(int i=0;i<residentCount;++i)if(overlap(box,personBox(metroResidentHome(*w.metroOrigin,i))))return true;
  for(int i=0;i<garageSize;++i)if(i!=w.cityLife.activeCar && overlap(box,carBounds(parkedCar(w,i))))return true;
  return false;
}
std::vector<Vertex> residentMesh(const World& w,float time) {
  std::vector<Vertex> mesh;if(!w.metroOrigin)return mesh;
  for(int i=0;i<residentCount;++i) {
    auto home=metroResidentHome(*w.metroOrigin,i);
    if(!w.chunks.contains(chunkAt(int(home.x),int(home.z))))continue;
    float sway=std::sin(time*1.8f+i)*.014f;
    auto box=[&](glm::vec3 a,glm::vec3 b,float m){appendBox(mesh,{home+a,home+b},{-100007,0,i},m,.92f);};
    float skin=i%3==0 ? 105 : metroOffice(i) ? 106 : 107,hair=metroOffice(i) ? 72 : 75,shirt=80+float((i*3+1)%20);
    for(float side:{-1.f,1.f}) {
      float x=side*.115f;box({x-.08f,.06f,-.095f},{x+.08f,.74f,.095f},75);
      box({x-.085f,0,-.16f},{x+.085f,.12f,.11f},77);
      float arm=side*.265f;box({arm-.065f,.74f+sway,-.09f},{arm+.065f,1.28f+sway,.09f},shirt);
      box({arm-.062f,.64f+sway,-.085f},{arm+.062f,.82f+sway,.085f},skin);
    }
    box({-.22f,.72f,-.13f},{.22f,1.29f,.13f},shirt);
    box({-.08f,1.27f,-.08f},{.08f,1.38f,.08f},skin);
    box({-.19f,1.34f,-.17f},{.19f,1.74f,.17f},skin);
    box({-.20f,1.64f,-.185f},{.20f,1.80f,.20f},hair);
    box({-.21f,1.25f,.11f},{.21f,1.75f,.22f},hair);
    float hairLength=i%3==0 ? 1.08f : i%3==1 ? 1.25f : 1.38f;
    for(float x:{-.235f,.18f})box({x,hairLength,-.19f},{x+.055f,1.73f,.20f},hair);
    box({-.18f,1.61f,-.20f},{.07f,1.74f,-.17f},hair);
    if(i%3==1)box({-.08f,1.13f,.19f},{.08f,1.65f,.29f},hair);
    if(i%3==2)box({-.235f,.43f,-.15f},{.235f,.82f,.15f},shirt);
    if(i%2==0)box({-.17f,.83f,-.143f},{.17f,.865f,-.131f},77);
    for(float x:{-.10f,.07f})box({x,1.52f,-.181f},{x+.035f,1.56f,-.169f},75);
    box({-.055f,1.43f,-.182f},{.055f,1.45f,-.170f},79);
  }
  return mesh;
}
} // namespace bw

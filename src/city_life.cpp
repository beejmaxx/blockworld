#include "city_life.hpp"
#include "adventure.hpp"
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
std::optional<float> ray(glm::vec3 origin,glm::vec3 direction,Box box,float reach) {
  float lo=0,hi=reach;
  for(int axis=0;axis<3;++axis) {
    if(std::abs(direction[axis])<1e-6f) {if(origin[axis]<box.min[axis] || origin[axis]>box.max[axis])return {};}
    else {float a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];lo=std::max(lo,std::min(a,b));hi=std::min(hi,std::max(a,b));}
  }
  return lo<=hi ? std::optional(lo) : std::nullopt;
}
Box personBox(glm::vec3 p){return {p+glm::vec3(-.32f,0,-.29f),p+glm::vec3(.32f,1.8f,.24f)};}
bool overlap(Box a,Box b) {return a.max.x>b.min.x && a.min.x<b.max.x && a.max.y>b.min.y && a.min.y<b.max.y && a.max.z>b.min.z && a.min.z<b.max.z;}
double familyTime(const WorldClock& clock){return (double(clock.day)+clock.phase)*WorldClock::daySeconds;}
bool infant(const World& w,int resident,int child){return w.clock.day==w.cityLife.residents[resident].children[child];}
Box childBox(const World& w,int resident,int child){auto at=cityChildPosition(w,resident,child);return {at+glm::vec3(-.34f,0,-.22f),at+glm::vec3(.34f,infant(w,resident,child) ? .28f : .95f,.22f)};}
double workerPhase(const World& w,int i) {return std::fmod(w.clock.phase*WorldClock::daySeconds*.06+i*.371,1.0);}
bool workerVisible(const World& w,glm::vec3 at) {
  return w.chunks.contains(chunkAt(int(std::floor(at.x)),int(std::floor(at.z))))
    && collidable(w.get({int(at.x),22,int(at.z)})) && !collidable(w.get({int(at.x),23,int(at.z)}));
}
}
void recordCityTransaction(World& w,BankKind kind,int amount) {
  if(amount<=0)return;
  auto& entries=w.cityLife.statement;entries.push_back({kind,amount,w.clock.day});
  if(entries.size()>8)entries.erase(entries.begin());
}
std::span<const CarSpec> garageCars(){return cars;}
std::span<const ResidentSpec> cityResidents(){return residents;}
FarmCar parkedCar(const World& w,int i) {
  if(!w.metroOrigin || i<0 || i>=garageSize)return {};
  for(int home=0;home<3;++home)if(w.estateOrigin && w.cityLife.homeCars[home]==i)
    return {true,mansionPosition(w,home,{10.5f,0,-15.5f}),0};
  return {true,garagePosition(*w.metroOrigin,i),i<10 ? 3.14159265f : 0.f};
}
glm::vec3 cityWorkerPosition(const World& w,int i) {
  if(!w.metroOrigin || i<0 || i>=cityWorkerCount)return {};
  auto o=*w.metroOrigin;double phase=workerPhase(w,i);float patrol=float(phase<.5 ? phase*2 : 2-phase*2);
  return {o.x+((i/2)%metroColumns)*64+22.f+20*patrol,23,o.z+(i/(metroColumns*2))*64+56.8f+(i%2)*1.4f};
}
int cityChildCount(const ResidentState& r){return int(std::ranges::count_if(r.children,[](auto day){return day>0;}));}
int hungryCityChildren(const ResidentState& r,const WorldClock& clock) {
  int count=0;
  for(int c=0;c<3;++c)if(r.children[c] && (r.lastFed[c]==0 || familyTime(clock)-r.lastFed[c]>=150))++count;
  return count;
}
bool cityChildFeeding(const World& w,int resident,int child) {
  auto& r=w.cityLife.residents[resident];double elapsed=familyTime(w.clock)-r.lastFed[child];
  return r.children[child] && r.lastFed[child]>0 && elapsed>=0 && elapsed<8;
}
glm::vec3 cityChildPosition(const World& w,int resident,int child) {
  auto home=cityResidentHome(w,resident);
  if(infant(w,resident,child)) {
    if(cityChildFeeding(w,resident,child))return home+glm::vec3(0,1.04f,-.39f);
    auto crib=cityCribPosition(w,resident,child);
    if(w.get({int(crib.x),int(crib.y),int(crib.z)})==Block::Table)return crib+glm::vec3(0,.69f,0);
  }
  constexpr std::array<glm::vec3,3> offsets{{{1.5f,0,.5f},{-1.5f,0,.5f},{0,0,-1.25f}}};
  return home+offsets[child];
}
std::string feedCityFamily(World& w,int index) {
  if(!w.metroOrigin || index<0 || index>=residentCount)return "Visit your family first.";
  auto& r=w.cityLife.residents[index];std::string name(residents[index].name);
  if(!cityChildCount(r))return name+": We don't have a child to feed yet.";
  if(!hungryCityChildren(r,w.clock))return name+": The children are fed and happy.";
  bool baby=false;
  for(int c=0;c<3;++c)if(r.children[c]){r.lastFed[c]=familyTime(w.clock);baby|=infant(w,index,c);}
  return name+(baby ? ": I'll feed our baby. Come sit with us." : ": Snack time! Let's eat together.");
}
std::string startCityFamily(World& w,int index) {
  if(!w.metroOrigin || index<0 || index>=residentCount)return "Visit an apartment first.";
  auto& r=w.cityLife.residents[index];auto name=std::string(residents[index].name);
  if(!r.dating)return name+": Let's build a relationship before starting a family.";
  if(r.pregnancyDue)return name+": We're already expecting a baby. I'm excited to meet them!";
  if(cityChildCount(r)==3)return name+": Our family of three children keeps us busy!";
  if(w.clock.day>std::numeric_limits<std::uint32_t>::max()-2)return "The calendar cannot advance further.";
  r.pregnancyDue=w.clock.day+2;
  return name+": Yes, I'd like to start a family with you. Our baby is due on day "+std::to_string(r.pregnancyDue)+".";
}
void updateCityFamilies(World& w) {
  if(!w.metroOrigin)return;
  for(auto& r:w.cityLife.residents)if(r.pregnancyDue && w.clock.day>=r.pregnancyDue) {
    for(auto& birth:r.children)if(!birth){birth=r.pregnancyDue;break;}
    r.pregnancyDue=0;
  }
}
bool bankTransfer(World& w,int amount,bool deposit) {
  auto& balance=w.cityLife.bank;auto& wallet=w.farm.garden.coins;
  if(amount<=0)return false;
  if(deposit) {if(amount>wallet || amount>bankLimit-balance)return false;wallet-=amount;balance+=amount;}
  else {if(amount>balance || amount>coinLimit-wallet)return false;balance-=amount;wallet+=amount;}
  recordCityTransaction(w,deposit ? BankKind::Deposit : BankKind::Withdrawal,amount);return true;
}
int propertyRentPerDay(const World& w) {
  if(!w.metroOrigin)return 0;int rent=0;
  for(std::size_t i=0;i<metroBuildings().size();++i)rent+=metroBuildings()[i].floors*(metroOffice(i) ? 6 : 4);
  return rent;
}
int serverIncomePerDay(const World& w){
  if(!w.metroOrigin)return 0;int total=0;
  for(const auto& site:w.cityLife.dataCenters)total+=dataCenterReport(site).profit;
  return total;
}
void collectCityIncome(World& w) {
  auto& city=w.cityLife;
  if(!w.metroOrigin || w.clock.day<=city.rentDay)return;
  auto days=std::uint64_t(w.clock.day-city.rentDay);
  auto credit=[&](BankKind kind,int daily) {
    int amount=int(std::min<std::uint64_t>(bankLimit-city.bank,days*std::uint64_t(daily)));
    city.bank+=amount;recordCityTransaction(w,kind,amount);
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
  auto home=cityResidentHome(w,index);auto at=home+glm::vec3(0,0,-2.5f);
  w.ensure(chunkAt(int(at.x),int(at.z)),2);
  if(p.collides(w,at))return false;
  p.pose.position=at;p.pose.yaw=3.14159265f;p.pose.pitch=-.05f;p.stopFlying();p.velocity={};return true;
}
std::optional<std::string> partnerNightProblem(const World& w,const Player& p,int index) {
  if(!w.metroOrigin || index<0 || index>=residentCount || residents[index].age<18 || !w.cityLife.residents[index].dating)
    return "Get to know each other and start dating first.";
  if(glm::length(cityResidentHome(w,index)-p.pose.position)>=4)return "Visit your partner's home first.";
  if(w.clock.day==std::numeric_limits<std::uint32_t>::max() && w.clock.phase>=WorldClock::morning)return "The calendar cannot advance further.";
  auto b=metroBuildings()[index];auto origin=harborPosition(*w.metroOrigin,b,{0,float(1+index%4)*harborFloorHeight,0});
  int home=w.cityLife.residents[index].home;
  int width=b.width,depth=b.depth;
  if(home>=0){origin=mansionPosition(w,home,{0,6,0});width=home==0 ? 56 : home==1 ? 64 : 60;depth=home==1 ? 44 : 40;}
  for(int z=2;z<depth-1;++z)for(int x=home>=0 ? 9 : 15;x<width-1;++x) {
    Cell bed{int(origin.x)+x,int(origin.y),int(origin.z)+z};auto block=w.get(bed);
    if(!isBed(block) || bedHead(block))continue;
    Player atBed=p;atBed.pose.position=glm::vec3(bed.x+.5f,bed.y,bed.z+.5f);
    auto status=bedSleepStatus(w,atBed,bed);
    if(status==SleepResult::Ready || status==SleepResult::Daytime)return {};
  }
  return "Your bedroom needs a complete bed with clear space above it.";
}
bool spendNightWithResident(World& w,const Player& p,int index) {
  if(partnerNightProblem(w,p,index))return false;
  auto before=w.clock;w.clock.wakeAtMorning();
  double elapsed=(double(w.clock.day)-before.day+w.clock.phase-before.phase)*WorldClock::daySeconds;
  growFarm(w,float(elapsed));collectCityIncome(w);updateCityFamilies(w);
  return true;
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
  for(int i=0;i<residentCount;++i) {
    test(personBox(cityResidentHome(w,i)),CityTargetKind::Resident,i);
    for(int c=0;c<3;++c)if(w.cityLife.residents[i].children[c])test(childBox(w,i,c),CityTargetKind::Child,i*3+c);
  }
  for(int i=0;i<cityWorkerCount;++i) {auto at=cityWorkerPosition(w,i);if(workerVisible(w,at))test(personBox(at),CityTargetKind::Worker,i);}
  for(int i=0;i<garageSize;++i)if(i!=w.cityLife.activeCar)test(carBounds(parkedCar(w,i)),CityTargetKind::Car,i);
  auto b=bankTerminal(*w.metroOrigin);test({b-glm::vec3(1.5f,1.5f,.65f),b+glm::vec3(1.5f,1.5f,.65f)},CityTargetKind::Bank,0);
  for(int i=0;i<2;++i){auto t=dataCenterTerminal(*w.metroOrigin,i);test({t-glm::vec3(1.5f,1.5f,.7f),t+glm::vec3(1.5f,1.5f,.7f)},CityTargetKind::DataCenter,i);}
  return found;
}
std::string cityPrompt(const World& w,CityTarget target) {
  if(target.kind==CityTargetKind::DataCenter)return "Data center / V to manage racks and contracts";
  if(target.kind==CityTargetKind::Worker)return "City resident / V to say hello";
  if(target.kind==CityTargetKind::Child)return "Your child with "+std::string(residents[target.index/3].name)+" / V to visit your family";
  if(target.kind==CityTargetKind::Bank)return "Civic Bank / V to open your account";
  if(target.kind==CityTargetKind::Car)return std::string(cars[target.index].name)+" / V to drive";
  return std::string(residents[target.index].name)+" / 18 / "+(w.cityLife.residents[target.index].dating ? "Girlfriend / " : "")+"V to talk";
}
bool cityPeopleOverlap(const World& w,Box box) {
  if(!w.metroOrigin)return false;
  for(int i=0;i<residentCount;++i) {
    if(overlap(box,personBox(cityResidentHome(w,i))))return true;
    for(int c=0;c<3;++c)if(w.cityLife.residents[i].children[c] && overlap(box,childBox(w,i,c)))return true;
  }
  for(int i=0;i<garageSize;++i)if(i!=w.cityLife.activeCar && overlap(box,carBounds(parkedCar(w,i))))return true;
  return false;
}
std::vector<Vertex> residentMesh(const World& w,float time) {
  std::vector<Vertex> mesh;if(!w.metroOrigin)return mesh;
  for(int i=0;i<residentCount;++i) {
    appendApartmentDecor(mesh,w,i);
    auto home=cityResidentHome(w,i);
    if(!w.chunks.contains(chunkAt(int(home.x),int(home.z))))continue;
    float sway=std::sin(time*1.8f+i)*.014f;
    auto box=[&](glm::vec3 a,glm::vec3 b,float m){appendBox(mesh,{home+a,home+b},{-100007,0,i},m,.92f);};
    constexpr std::array<float,8> hairColors{72,71,75,62,71,72,62,75};
    float skin=106,hair=hairColors[i],shirt=80+float((i*3+1)%20);
    bool nursing=false;
    for(int c=0;c<3;++c)nursing|=infant(w,i,c) && cityChildFeeding(w,i,c);
    for(float side:{-1.f,1.f}) {
      float x=side*.115f;box({x-.08f,.06f,-.095f},{x+.08f,.74f,.095f},75);
      box({x-.085f,0,-.16f},{x+.085f,.12f,.11f},77);
      float arm=side*.265f;
      if(nursing) {
        box({arm-.065f,.90f,-.17f},{arm+.065f,1.28f,.09f},shirt);
        box({std::min(arm,arm*.2f)-.05f,.93f,-.49f},{std::max(arm,arm*.2f)+.05f,1.04f,-.14f},skin);
      } else {
        box({arm-.065f,.74f+sway,-.09f},{arm+.065f,1.28f+sway,.09f},shirt);
        box({arm-.062f,.64f+sway,-.085f},{arm+.062f,.82f+sway,.085f},skin);
      }
    }
    box({-.20f,.72f,-.13f},{.20f,.94f,.13f},shirt);
    box({-.18f,.94f,-.12f},{.18f,1.05f,.12f},shirt);
    box({-.22f,1.05f,-.15f},{.22f,1.29f,.13f},shirt);
    for(float side:{-.105f,.105f})box({side-.093f,1.04f,-.245f},{side+.093f,1.23f,-.13f},shirt);
    if(w.cityLife.residents[i].pregnancyDue)box({-.19f,.77f,-.23f},{.19f,1.13f,-.12f},shirt);
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
    for(int c=0;c<3;++c)if(w.cityLife.residents[i].children[c]) {
      auto at=cityChildPosition(w,i,c);float size=.64f;
      auto piece=[&](glm::vec3 a,glm::vec3 b,float m){appendBox(mesh,{at+a*size,at+b*size},{-100007,1,i*3+c},m,.92f);};
      if(infant(w,i,c)) {
        at.y+=nursing ? std::sin(time*2)*.012f : 0;
        piece({-.48f,0,-.25f},{.20f,.31f,.25f},73);
        piece({.14f,.05f,-.22f},{.49f,.40f,.22f},skin);
        piece({.38f,.16f,-.225f},{.42f,.20f,-.213f},75);
        continue;
      }
      piece({-.23f,.40f,-.15f},{.23f,.97f,.15f},80+float((i+c+4)%20));
      piece({-.24f,.96f,-.20f},{.24f,1.41f,.20f},skin);
      piece({-.25f,1.33f,-.21f},{.25f,1.45f,.21f},hair);
      for(float side:{-.15f,.15f}) {
        piece({side-.06f,0,-.09f},{side+.06f,.43f,.09f},75);
        piece({side*1.8f-.055f,.45f,-.08f},{side*1.8f+.055f,.88f,.08f},skin);
        piece({side*.7f-.025f,1.15f,-.212f},{side*.7f+.025f,1.19f,-.201f},75);
      }
    }
  }
  for(int house=0;house<3;++house)appendMansionNursery(mesh,w,house);
  for(int i=0;i<cityWorkerCount;++i) {
    auto at=cityWorkerPosition(w,i);if(!workerVisible(w,at))continue;
    std::size_t start=mesh.size();float skin=106,shirt=80+float((i*7+2)%20);
    float stride=std::sin(float(w.clock.phase*WorldClock::daySeconds)*7+i)*.10f;
    auto box=[&](glm::vec3 a,glm::vec3 b,float m){appendBox(mesh,{a,b},{-100009,0,i},m,.92f);};
    for(float side:{-1.f,1.f}) {
      float x=side*.115f,step=side*stride;
      box({x-.08f,.05f,-.09f+step},{x+.08f,.73f,.09f+step},75);
      box({x-.09f,0,-.14f+step},{x+.09f,.12f,.11f+step},77);
      box({side*.265f-.06f,.69f,-.08f-step},{side*.265f+.06f,1.25f,.08f-step},shirt);
      box({side*.265f-.055f,.62f,-.08f-step},{side*.265f+.055f,.79f,.08f-step},skin);
    }
    box({-.22f,.73f,-.13f},{.22f,1.30f,.13f},shirt);
    box({-.17f,1.33f,-.15f},{.17f,1.72f,.15f},skin);
    box({-.18f,1.65f,-.16f},{.18f,1.78f,.17f},i%2 ? 72 : 75);
    for(float x:{-.09f,.065f})box({x,1.51f,-.16f},{x+.027f,1.55f,-.151f},75);
    box({-.045f,1.42f,-.16f},{.045f,1.44f,-.151f},79);
    float direction=workerPhase(w,i)<.5 ? 1 : -1;
    for(std::size_t v=start;v<mesh.size();++v) {
      auto p=mesh[v].position;mesh[v].position=at+glm::vec3(-p.z*direction,p.y,p.x*direction);
    }
  }
  return mesh;
}
} // namespace bw

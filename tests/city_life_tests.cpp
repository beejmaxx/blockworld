#include "city_life.hpp"
#include "garden.hpp"
#include "road.hpp"
#include "building.hpp"
#include "minimap.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
struct Fixture {
  World w{7262026,true};Player p;
  Fixture(){p.pose.position={10.5f,24,7.5f};w.ensure({0,0},1);initializeFarm(w);initializeHome(w);initializeGarden(w);check(initializeMetropolis(w,p),"city initializes");}
};
void walk(World& w,Player& p,glm::vec3 target) {
  for(int frame=0;frame<2500;++frame) {
    glm::vec2 delta{target.x-p.pose.position.x,target.z-p.pose.position.z};
    if(glm::length(delta)<.08f)return;
    p.pose.yaw=std::atan2(delta.x,-delta.y);p.tick(w,{.forward=1},1.f/60);
    check(!p.collides(w,p.pose.position),"player clips into a wall during a walking route");
  }
  throw std::runtime_error("walking route blocked at "+std::to_string(p.pose.position.x)+","+std::to_string(p.pose.position.y)+","+std::to_string(p.pose.position.z));
}
void driveTo(World& w,Player& p,RideState& ride,glm::vec3 target) {
  for(int frame=0;frame<5000;++frame) {
    auto& car=w.farm.car;auto delta=target-car.position;delta.y=0;
    if(glm::length(delta)<.12f){tickRanch(w,p,ride,{.jump=true},1.f/60);return;}
    w.ensure(chunkAt(int(car.position.x),int(car.position.z)),1);
    float yaw=std::atan2(delta.x,-delta.z),error=std::remainder(yaw-car.yaw,6.2831853f);
    Movement m;m.right=std::clamp(error*4,-1.f,1.f);
    m.jump=std::abs(error)>.12f;m.forward=m.jump ? 0 : std::min(4.f,glm::length(delta)*3)/carTopSpeed;
    tickRanch(w,p,ride,m,1.f/60);
    check(carFits(w,car),"car intersected a wall or another parked car");
  }
  throw std::runtime_error("car cannot reach route waypoint at "+std::to_string(target.x)+","+std::to_string(target.z));
}
void access(World& w,Player p) {
  auto o=*w.metroOrigin;
  check(metroBuildings().size()==36,"36 new towers");std::set<std::string_view> names;
  for(int i=0;i<36;++i) {
    auto b=metroBuildings()[i];names.insert(b.name);
    check(visitMetroProperty(w,p,i),"property visit works: "+std::string(b.name));
    auto center=chunkAt(int(p.pose.position.x),int(p.pose.position.z));w.ensure(center,4);
    auto go=[&](float x,float z){walk(w,p,harborPosition(o,b,{x,0,z}));};
    go(12.5f,12.5f);go(3.5f,12.5f);
    for(int floor=0;floor<b.floors;++floor) {
      go(3.5f,5.5f);go(7.5f,5.5f);go(7.5f,12.5f);
      check(std::abs(p.pose.position.y-23-(floor+1)*5)<.03f,"stairs reach every occupied floor and roof");
      if(floor+1<b.floors)go(3.5f,12.5f);
    }
    for(int floor=b.floors;floor>0;--floor) {
      go(7.5f,5.5f);go(3.5f,5.5f);go(3.5f,12.5f);if(floor>1)go(7.5f,12.5f);
    }
    go(12.5f,12.5f);go(12.5f,b.depth+3.5f);
    check(std::abs(p.pose.position.y-23)<.03f,"return to street after walking down");
    w.evict(center,4);
  }
  check(names.size()==36,"every tower has a unique name");
  check(visitMetropolis(w,p) && visitMetropolis(w,p,true),"street and rooftop shortcuts are safe");
  check(visitBank(w,p),"bank counter is reachable");
  check(targetCity(w,p) && targetCity(w,p)->kind==CityTargetKind::Bank,"bank terminal responds to V");
  for(int i=0;i<2;++i) {
    check(visitDataCenter(w,p,i),"data center front door is open");
    auto at=p.pose.position;walk(w,p,at+glm::vec3(0,0,7));
    check(!p.collides(w,p.pose.position),"server room can be entered");
  }
  std::cout<<"Walked all 36 towers, every floor up and down, both server rooms and bank.\n";
}
void residents(World& w,Player p) {
  check(cityResidents().size()==8,"eight adult residents");std::set<std::string_view> names;
  for(int i=0;i<residentCount;++i) {
    auto person=cityResidents()[i];names.insert(person.name);check(person.age==18,"every resident is 18");
    check(visitResident(w,p,i),"resident apartment is reachable: "+std::string(person.name));
    auto target=targetCity(w,p);check(target && target->kind==CityTargetKind::Resident && target->index==i,"resident can be targeted inside own apartment");
    check(!miningTarget(w,p),"remove mode cannot dig through a resident");
    auto home=metroResidentHome(*w.metroOrigin,i);
    auto mesh=residentMesh(w,0);check(!mesh.empty() && mesh.size()<peopleVertexLimit,"resident and population geometry stays within GPU budget");
    talkToResident(w,i,true);check(!w.cityLife.residents[i].dating,"asking before getting acquainted is declined");
    talkToResident(w,i,false);talkToResident(w,i,false);talkToResident(w,i,true);
    check(w.cityLife.residents[i].dating,"dating is independent for every adult resident");
    auto wall=Cell{int(home.x),int(home.y)+1,int(home.z)-1};auto old=w.get(wall);w.set(wall,Block::Concrete);
    check(!targetCity(w,p),"resident interactions cannot reach through a wall");w.set(wall,old);
  }
  check(names.size()==8,"residents have distinct identities");
}
void garage(World& w,Player p) {
  RideState ride;auto o=*w.metroOrigin;check(visitGarage(w,p),"garage entrance works");
  std::set<std::string_view> names;std::set<int> bodies;
  for(int i=0;i<garageSize;++i) {
    names.insert(garageCars()[i].name);bodies.insert(garageCars()[i].body);
    check(takeGarageCar(w,p,ride,i),"car can be selected: "+std::string(garageCars()[i].name));
    auto bay=w.farm.car.position;
    driveTo(w,p,ride,{bay.x,23,o.z+351.f});
    driveTo(w,p,ride,{o.x+385.f,23,o.z+351.f});
    driveTo(w,p,ride,{o.x+385.f,23,o.z+317.f});
    check(leaveRide(w,p,ride),"driver can leave car on the street");
    check(visitGarage(w,p),"car returns to its numbered bay when visiting garage");
    check(w.farm.car.position==parkedCar(w,i).position,"active vehicle is parked with the collection");
  }
  check(names.size()==20 && bodies.size()==6,"twenty distinct cars with six body styles");
  auto mesh=ranchMesh(w);check(mesh.size()<ranchVertexLimit,"twenty-car garage fits its GPU buffer");
  check(!takeGarageCar(w,p,ride,-1) && !takeGarageCar(w,p,ride,20),"invalid vehicle indexes do not mutate the active car");
  // Drive the entire physical link between the old highway and downtown.
  auto route=metroLink(w);check(route.size()>=2,"city has a road connection");
  w.farm.car={true,route.front(),0};ride={true,true,0};
  for(std::size_t i=1;i<route.size();++i)driveTo(w,p,ride,route[i]);
  check(leaveRide(w,p,ride),"exit after driving highway to city");
  std::cout<<"Drove all 20 cars out of the garage, and drove the highway connection.\n";
}
void finance(World& w) {
  auto& c=w.cityLife;w.farm.garden.coins=750;
  check(bankTransfer(w,250,true) && c.bank==250 && w.farm.garden.coins==500,"deposit conserves coins");
  check(bankTransfer(w,75,false) && c.bank==175 && w.farm.garden.coins==575,"withdrawal conserves coins");
  check(!bankTransfer(w,-1,true) && !bankTransfer(w,600,true) && !bankTransfer(w,176,false) && c.bank==175,"invalid bank transfers are atomic");
  int bank=c.bank,income=propertyRentPerDay(w)+serverIncomePerDay(w);check(income>2000,"portfolio has substantial daily income");
  collectCityIncome(w);check(c.bank==bank,"no retroactive rent at installation");
  ++w.clock.day;collectCityIncome(w);check(c.bank==bank+income,"one day's rent and server leases reach bank");
  collectCityIncome(w);check(c.bank==bank+income,"same day cannot pay twice");
  w.clock.day+=3;collectCityIncome(w);check(c.bank==bank+income*4,"sleep or skipped days pay correct income");
  for(int i=0;i<20;++i)check(bankTransfer(w,1,i%2==0),"repeated transfers succeed");
  check(c.statement.size()==8,"transaction history remains bounded");
  c.bank=bankLimit-3;++w.clock.day;collectCityIncome(w);check(c.bank==bankLimit,"income saturates safely at balance limit");
  check(!bankTransfer(w,1,true),"cannot overflow bank account");
  c.bank=1000;w.farm.garden.coins=coinLimit;check(!bankTransfer(w,1,false),"cannot overflow wallet");
  w.farm.garden.coins=575;
}
void families(World& w,Player p) {
  auto& r=w.cityLife.residents[0];auto& other=w.cityLife.residents[1];
  r.dating=false;startCityFamily(w,0);check(!r.pregnancyDue,"starting a family requires a relationship");r.dating=true;
  startCityFamily(w,0);auto due=w.clock.day+2;check(r.pregnancyDue==due,"pregnancy is due two game days later");
  startCityFamily(w,0);check(r.pregnancyDue==due,"repeated requests do not restart pregnancy");
  startCityFamily(w,1);check(other.pregnancyDue==due,"each partner's household is independent");
  ++w.clock.day;updateCityFamilies(w);check(cityChildCount(r)==0,"baby does not arrive early");
  ++w.clock.day;updateCityFamilies(w);check(cityChildCount(r)==1 && cityChildCount(other)==1 && !r.pregnancyDue,"birth occurs once on the due day");
  updateCityFamilies(w);check(cityChildCount(r)==1,"repeated updates cannot duplicate a baby");
  check(visitResident(w,p,0),"apartment is still accessible after a birth");
  auto child=cityChildPosition(w,0,0);
  p.pose.position=child+glm::vec3(0,-.69f,-2);auto d=glm::normalize(child+glm::vec3(0,.15f,0)-p.eye());
  p.pose.yaw=std::atan2(d.x,-d.z);p.pose.pitch=std::asin(d.y);
  auto hit=targetCity(w,p);check(hit && hit->kind==CityTargetKind::Child,"child is a separate family interaction, not a dating target");
  check(hungryCityChildren(r,w.clock)==1,"new baby is ready for a feed");
  feedCityFamily(w,0);check(!hungryCityChildren(r,w.clock) && cityChildFeeding(w,0,0),"feeding satisfies hunger and starts the cradle animation");
  check(cityChildPosition(w,0,0)!=child,"feeding moves the baby from the crib into the parent's arms");
  auto fed=r.lastFed;feedCityFamily(w,0);check(r.lastFed==fed,"repeat feeding does not reset a satisfied child's meal");
  check(hungryCityChildren(other,w.clock)==1,"feeding one household does not feed another");
  w.clock.advance(9);check(!cityChildFeeding(w,0,0) && cityChildPosition(w,0,0)==child,"baby returns to its crib after feeding");
  Cell crib{int(child.x),int(child.y),int(child.z)};auto cribBlock=w.get(crib);w.set(crib,Block::Air);
  check(cityChildPosition(w,0,0).y<child.y,"removing a crib cannot leave the baby floating above it");w.set(crib,cribBlock);
  w.clock.advance(150);check(hungryCityChildren(r,w.clock)==1,"time makes the next feed available");
  feedCityFamily(w,0);
  startCityFamily(w,0);check(r.pregnancyDue>w.clock.day,"a later pregnancy can coexist with an existing child");
  collectCityIncome(w);
  auto at=cityWorkerPosition(w,0);w.clock.phase+=.001;
  check(glm::length(at-cityWorkerPosition(w,0))>.1f,"city population walks while game time advances");
  w.ensure(chunkAt(int(at.x),int(at.z)),2);
  auto mesh=residentMesh(w,0);check(mesh.size()<peopleVertexLimit,"workers and families fit the render buffer");
  for(auto v:mesh)check(std::isfinite(v.position.x) && std::isfinite(v.position.y) && std::isfinite(v.position.z),"population mesh is finite");
}
void persistence(World& w,Player p) {
  auto path=std::filesystem::temp_directory_path()/("blockworld-metro-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}}cleanup{path};
  w.save(path,p.pose);World loaded;check(loaded.load(path).has_value(),"new city save loads");
  check(loaded.metroOrigin==w.metroOrigin && loaded.cityLife.bank==w.cityLife.bank && loaded.cityLife.activeCar==w.cityLife.activeCar && loaded.cityLife.residents[7].dating,"bank, cars and relationships survive reload");
  check(loaded.cityLife.residents[0].children==w.cityLife.residents[0].children && loaded.cityLife.residents[0].pregnancyDue==w.cityLife.residents[0].pregnancyDue,"children and pregnancy survive reload");
  check(loaded.cityLife.residents[0].lastFed==w.cityLife.residents[0].lastFed,"feeding status survives reload");
  int bank=loaded.cityLife.bank;collectCityIncome(loaded);check(bank==loaded.cityLife.bank,"reload does not duplicate rent");
  std::vector<std::string> lines;std::ifstream in(path);for(std::string s;std::getline(in,s);)lines.push_back(s);
  auto write=[&](const auto& data){std::ofstream out(path);for(auto& s:data)out<<s<<'\n';};
  int index=16+int(w.farm.chickens.size()+w.farm.crops.size()+w.farm.livestock.size()+w.road.size());
  std::vector<std::string> tokens;std::istringstream fields(lines[index]);for(std::string s;fields>>s;)tokens.push_back(s);
  for(auto [field,value]:std::array<std::pair<int,std::string>,9>{{{6,"0"},{2,"120"},{4,"-1"},{4,"1000000000"},{5,"20"},{6,"99999"},{7,"4"},{8,"2"},{23,"99"}}}) {
    auto changed=lines,parts=tokens;parts[field]=value;std::string line;for(auto& v:parts)line+=v+" ";changed[index]=line;write(changed);
    bool rejected=false;try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.cityLife.bank==bank && loaded.metroOrigin==w.metroOrigin,"corrupt city data is rejected atomically");
  }
  int familyFields=24+std::stoi(tokens[23])*3;
  for(int field:{familyFields,familyFields+1,familyFields+3}) {
    auto changed=lines,parts=tokens;parts[field]="999999999";std::string line;for(auto& v:parts)line+=v+" ";changed[index]=line;write(changed);
    bool rejected=false;try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.cityLife.residents[0].children==w.cityLife.residents[0].children,"corrupt family dates are rejected atomically");
  }
  for(auto [field,value]:std::array<std::pair<int,std::string>,3>{{{familyFields+32,"-1"},{familyFields+32,"999999999999"},{familyFields+34,"1"}}}) {
    auto changed=lines,parts=tokens;parts[field]=value;std::string line;for(auto& v:parts)line+=v+" ";changed[index]=line;write(changed);
    bool rejected=false;try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.cityLife.residents[0].lastFed==w.cityLife.residents[0].lastFed,"invalid feeding records are rejected atomically");
  }
  auto v18=lines;v18[0]="BLOCKWORLD 18 7262026 1";std::string previousState;
  for(int i=0;i<familyFields+32;++i)previousState+=tokens[i]+" ";v18[index]=previousState;write(v18);
  check(loaded.load(path).has_value() && loaded.cityLife.residents[0].children==w.cityLife.residents[0].children && loaded.cityLife.residents[0].lastFed==std::array<double,3>{},"v18 migration preserves children and enables feeding");
  auto v17=lines;v17[0]="BLOCKWORLD 17 7262026 1";std::string oldState;
  for(int i=0;i<familyFields;++i)oldState+=tokens[i]+" ";v17[index]=oldState;write(v17);
  check(loaded.load(path).has_value() && loaded.cityLife.residents[0].dating && !loaded.cityLife.residents[0].pregnancyDue && cityChildCount(loaded.cityLife.residents[0])==0,"v17 relationships migrate without inventing family members");
  auto legacy=lines;legacy[0]="BLOCKWORLD 16 7262026 1";legacy.erase(legacy.begin()+index);write(legacy);
  check(loaded.load(path).has_value() && !loaded.metroOrigin && loaded.cityLife.bank==0 && loaded.editCount()==w.editCount() && loaded.farm.crops.size()==w.farm.crops.size(),"v16 migration retains farm, inventory and all edits");
}
void preservation() {
  Fixture f;auto o=*f.w.metroOrigin;f.w.metroOrigin.reset();f.w.ensure(chunkAt(o.x+100,o.z+20),0);
  Cell build{o.x+100,24,o.z+20};f.w.set(build,Block::RedTile);auto edits=f.w.editCount();
  check(initializeMetropolis(f.w,f.p) && f.w.metroOrigin->z>o.z,"existing construction moves the whole new city to another parcel");
  check(f.w.editCount()==edits && f.w.get(build)==Block::RedTile,"city generation cannot overwrite player construction");
}
void apartments(World& w,Player p) {
  for(int i=0;i<residentCount;++i) {
    check(visitResident(w,p,i),"decorated apartment arrival is clear");auto home=metroResidentHome(*w.metroOrigin,i);
    walk(w,p,home+glm::vec3(0,0,-4));
    walk(w,p,home+glm::vec3(5,0,-4));
    walk(w,p,home+glm::vec3(5,0,5));
    auto b=metroBuildings()[i];auto origin=harborPosition(*w.metroOrigin,b,{0,float(1+i%4)*5,0});
    auto get=[&](int x,int y,int z){return w.get({int(origin.x)+x,int(origin.y)+y,int(origin.z)+z});};
    check(isBed(get(b.width-5,0,b.depth-4)) && get(b.width-5,0,b.depth-13)==Block::Table,"each home has a usable bed and a crib anchor");
    check(get(16,1,b.depth-3)==Block::Planks && get(20,0,15)==Block::Table && get(20,0,b.depth-6)==Block::Sofa,"book wall, dining room and lounge are furnished");
    auto mesh=residentMesh(w,0);check(mesh.size()<peopleVertexLimit,"decorated homes and residents fit the GPU budget");
  }
}
}
int main(int argc,char** argv) {
  try {
    if(argc>1) {
      World w;Player p;auto pose=w.load(argv[1]);check(pose.has_value(),"load supplied world");p.pose=*pose;
      auto edits=w.editCount();check(initializeMetropolis(w,p) && edits==w.editCount(),"install city without changing a single saved block");
      auto o=*w.metroOrigin;std::cout<<"Installed at "<<o.x<<','<<o.y<<','<<o.z<<"; preserved "<<edits<<" edits; daily rent "<<propertyRentPerDay(w)<<" + servers "<<serverIncomePerDay(w)<<'\n';
      if(argc>2)w.save(argv[2],p.pose);
    } else {
      Fixture f;access(f.w,f.p);residents(f.w,f.p);apartments(f.w,f.p);garage(f.w,f.p);finance(f.w);families(f.w,f.p);persistence(f.w,f.p);preservation();
      auto map=buildMiniMap(f.w,{10,10},true,false);auto o=*f.w.metroOrigin;auto point=mapPoint(map,{o.x+metroWidth,o.z+metroDepth});
      check(point.x<1 && point.y<1 && std::ranges::any_of(map.markers,[](auto m){return m.name=="Garage";}),"overview contains whole city and garage marker");
      auto mesh=metropolisSkyline(f.w,{o.x-100.f,120,o.z-100.f});check(!mesh.empty() && mesh.size()<=skylineVertexLimit,"skyline fits its draw budget");
    }
    std::cout<<"PASS downtown, bank, residents, 20 cars, rentals, server rooms, migration and preservation\n";
  }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

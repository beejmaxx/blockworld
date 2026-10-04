#include "city_life.hpp"
#include "estate.hpp"
#include "garden.hpp"
#include "minimap.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const std::string& what){if(!ok)throw std::runtime_error(what);}
struct Fixture {
  World w{7262026,true};Player p;
  Fixture(){p.pose.position={10.5f,24,7.5f};w.ensure({0,0},1);initializeFarm(w);initializeHome(w);initializeGarden(w);check(initializeEstate(w,p),"district initializes");}
};
void households(Fixture& f) {
  auto& w=f.w;auto& p=f.p;
  check(w.cityLife.home==-1 && visitCityHome(w,p) && p.pose.position.x<100,"existing home remains the farm cabin");
  check(chooseCityHome(w,p,0) && w.cityLife.home==0,"mansion becomes R destination");
  auto at=p.pose.position;p.pose.position.x+=50;check(visitCityHome(w,p) && p.pose.position==at,"R returns to selected mansion");
  auto map=buildMiniMap(w,{at.x,at.z},true,false);bool homeMarked=false,cabinMarked=false;
  for(auto marker:map.markers){if(marker.name=="Home")homeMarked=glm::length(marker.position-glm::vec2(at.x,at.z))<.1f;if(marker.name=="Cabin")cabinMarked=true;}
  check(homeMarked && cabinMarked,"map follows chosen home and retains original cabin");
  auto& r=w.cityLife.residents[0];moveCityHousehold(w,p,0,0);check(r.home==-1,"moving in requires a relationship");
  r.dating=true;r.conversations=2;r.children[0]=w.clock.day;r.children[1]=w.clock.day;
  auto births=r.children;auto oldHome=cityResidentHome(w,0);
  auto arrival=p.pose;p.pose.position=mansionPosition(w,0,{40.5f,6,26.5f});moveCityHousehold(w,p,0,0);
  check(r.home==-1,"resident cannot materialize around player");p.pose=arrival;
  auto message=moveCityHousehold(w,p,0,0);check(r.home==0,"household moves: "+message);
  check(r.children==births && cityResidentHome(w,0)!=oldHome,"move retains family dates and changes residence");
  check(cityChildPosition(w,0,0)!=cityChildPosition(w,0,1),"babies have separate mansion cribs");
  check(visitResident(w,p,0) && !p.collides(w,p.pose.position),"moved household has a safe visitor arrival");
  auto target=targetCity(w,p);check(target && target->kind==CityTargetKind::Resident && target->index==0,"interaction follows resident to new home");
  check(!partnerNightProblem(w,p,0),"mansion bedroom supports partner nights");
  feedCityFamily(w,0);check(hungryCityChildren(r,w.clock)==0 && cityChildFeeding(w,0,0),"feeding works after moving");
  auto& other=w.cityLife.residents[1];other.dating=true;other.conversations=2;
  moveCityHousehold(w,p,1,0);check(other.home==-1 && r.home==0,"occupied house is not silently reassigned");
  auto wall=mansionPosition(w,1,{40,6,26});Cell cell{int(wall.x),int(wall.y),int(wall.z)};
  w.ensure(chunkAt(cell.x,cell.z),2);w.set(cell,Block::Concrete);
  moveCityHousehold(w,p,1,1);check(other.home==-1,"blocked family area rejects move");w.set(cell,Block::Air);
  moveCityHousehold(w,p,1,1);check(other.home==1 && r.home==0,"separate households keep separate mansions");
  moveCityHousehold(w,p,1,-1);check(other.home==-1 && visitResident(w,p,1),"return to original apartment is reversible");
  w.clock.advance(10);check(chooseCityHome(w,p,0),"return to driveway");
  w.cityLife.activeCar=0;message=parkHomeCar(w,p,0);
  check(w.cityLife.homeCars[0]==0 && w.farm.car.position==parkedCar(w,0).position && carFits(w,w.farm.car),"car parks on clear mansion bay: "+message);
  RideState ride;check(takeGarageCar(w,p,ride,0),"parked car can be driven");float start=w.farm.car.position.z;
  for(int i=0;i<240;++i)tickRanch(w,p,ride,{.forward=.2f},1.f/60);
  check(w.farm.car.position.z<start-10 && carFits(w,w.farm.car),"drive out of mansion bay onto road");
  check(leaveRide(w,p,ride),"driver can get out on estate road");
  check(chooseCityHome(w,p,1),"visit next house");parkHomeCar(w,p,1);
  check(w.cityLife.homeCars[0]==-1 && w.cityLife.homeCars[1]==0,"one car cannot appear at two homes");
  w.cityLife.activeCar=1;parkHomeCar(w,p,1);check(w.cityLife.homeCars[1]==1 && parkedCar(w,0).position==garagePosition(*w.metroOrigin,0),"replaced favorite returns to its original garage bay");
  auto car=w.farm.car.position;check(visitGarage(w,p) && w.farm.car.position==car,"garage visit does not teleport mansion car");
  check(residentMesh(w,0).size()<peopleVertexLimit,"nurseries stay within mesh capacity");
  check(chooseCityHome(w,p,-1) && w.cityLife.home==-1,"cabin can become main home again");
}
void business(Fixture& f) {
  auto& w=f.w;auto& p=f.p;auto& city=w.cityLife;auto& s=city.dataCenters[0];
  check(serverIncomePerDay(w)==256,"starter contracts preserve previous total income");
  city.bank=10000;int before=city.bank;signServerContract(w,0,1);
  check(s.contracts==1 && city.bank==before,"insufficient rack capacity rejects customer without a charge");
  auto o=*w.metroOrigin;Cell rack{o.x+354,o.y,o.z+271};w.ensure(chunkAt(rack.x,rack.z),2);w.set(rack,Block::Brick);
  upgradeDataCenter(w,p,0,ServerUpgrade::Rack);check(s.racks==2 && city.bank==before && w.get(rack)==Block::Brick,"equipment does not overwrite a player's construction");
  w.set(rack,Block::Air);upgradeDataCenter(w,p,0,ServerUpgrade::Rack);
  check(s.racks==3 && w.get(rack)!=Block::Air && city.bank==before-300,"clearing obstruction permits purchased equipment to occupy the bay");
  upgradeDataCenter(w,p,1,ServerUpgrade::Rack);check(city.dataCenters[1].racks==3 && s.racks==3,"other data center can expand independently");
}
void businessEconomy() {
  Fixture f;auto& w=f.w;auto& p=f.p;auto& city=w.cityLife;auto& s=city.dataCenters[0];
  city.bank=10000;int rent=propertyRentPerDay(w);
  ++w.clock.day;upgradeDataCenter(w,p,0,ServerUpgrade::Rack);
  check(city.bank==10000+rent+256-300 && s.racks==3,"old day settles before upgrade, then purchase is charged once");
  upgradeDataCenter(w,p,0,ServerUpgrade::Rack);check(s.racks==4,"second rack purchase expands capacity");
  int bank=city.bank;upgradeDataCenter(w,p,0,ServerUpgrade::Rack);check(city.bank==bank && s.racks==4,"power and cooling cap blocks purchase without charge");
  signServerContract(w,0,1);auto report=dataCenterReport(s);
  check(report.used==4 && report.revenue==384 && report.electricity==32 && report.cooling==32 && report.profit==320,"customer revenue minus both costs produces net profit");
  signServerContract(w,0,1);check(city.bank==bank && s.contracts==3,"contract cannot be signed twice");
  ++w.clock.day;collectCityIncome(w);check(city.bank==bank+rent+320+128,"daily bank credit uses net business profits");
  bank=city.bank;collectCityIncome(w);check(city.bank==bank,"daily settlement is idempotent");
  upgradeDataCenter(w,p,0,ServerUpgrade::Power);check(s.power==2 && dataCenterReport(s).capacity==4,"power alone does not bypass cooling");
  upgradeDataCenter(w,p,0,ServerUpgrade::Cooling);check(s.cooling==2 && city.bank==bank-1050,"capacity upgrades spend exactly their displayed prices");
  for(int n=0;n<4;++n)upgradeDataCenter(w,p,0,ServerUpgrade::Rack);
  signServerContract(w,0,2);check(s.racks==8 && s.contracts==7 && dataCenterReport(s).profit==768,"research contract activates after expansion");
  bank=city.bank;w.clock.day+=3;collectCityIncome(w);check(city.bank==bank+3*(rent+768+128),"skipped days use correct net profit without double payment");
  city.bank=0;upgradeDataCenter(w,p,0,ServerUpgrade::Power);check(city.bank==0 && s.power==2,"purchase cannot overdraw bank");
  city.bank=bankLimit-1;++w.clock.day;collectCityIncome(w);check(city.bank==bankLimit,"settlement safely saturates at bank limit");
  auto terminal=dataCenterTerminal(*w.metroOrigin,0);w.ensure(chunkAt(int(terminal.x),int(terminal.z)),2);
  p.pose.position={terminal.x,float(w.metroOrigin->y),terminal.z+3};p.pose.yaw=0;p.pose.pitch=0;
  auto hit=targetCity(w,p);check(hit && hit->kind==CityTargetKind::DataCenter && hit->index==0,"server room management terminal is interactable");
}
void persistence(Fixture& f) {
  auto& w=f.w;chooseCityHome(w,f.p,0);w.cityLife.bank=3210;
  auto path=std::filesystem::temp_directory_path()/("city-management-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
  struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}}cleanup{path};
  w.save(path,f.p.pose);World loaded;check(loaded.load(path).has_value(),"new save loads");
  check(loaded.cityLife.home==0 && loaded.cityLife.homeCars==w.cityLife.homeCars && loaded.cityLife.residents[0].home==0,"home, household and parking survive reload");
  check(loaded.cityLife.dataCenters[1].racks==3 && loaded.cityLife.bank==3210,"business and bank survive reload");
  std::vector<std::string> lines;std::ifstream in(path);for(std::string line;std::getline(in,line);)lines.push_back(line);in.close();
  int record=16+int(w.farm.chickens.size()+w.farm.crops.size()+w.farm.livestock.size()+w.road.size());
  std::vector<std::string> fields;std::istringstream input(lines.at(record));for(std::string v;input>>v;)fields.push_back(v);
  auto write=[&](std::vector<std::string> values,int version){auto data=lines;data[0]="BLOCKWORLD "+std::to_string(version)+" 7262026 1";data[record].clear();for(auto& v:values)data[record]+=v+" ";std::ofstream out(path);for(auto& line:data)out<<line<<'\n';};
  int offset=int(fields.size())-20;
  for(auto [field,value]:std::array<std::pair<int,std::string>,7>{{{0,"3"},{1,"20"},{5,"0"},{12,"17"},{13,"0"},{14,"9"},{15,"15"}}}) {
    auto bad=fields;bad[offset+field]=value;write(bad,21);bool rejected=false;
    try{loaded.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && loaded.cityLife.bank==3210 && loaded.cityLife.home==0,"invalid home/business state rejected atomically");
  }
  // Legacy fixtures have no new purchase transaction type.
  auto legacy=fields;int entries=std::stoi(legacy[23]);for(int n=0;n<entries;++n)if(legacy[24+n*3]=="4")legacy[24+n*3]="1";
  legacy.resize(legacy.size()-20);write(legacy,20);World old;check(old.load(path).has_value(),"v20 save migrates");
  check(old.cityLife.home==-1 && old.cityLife.residents[0].home==-1 && old.cityLife.residents[0].children==w.cityLife.residents[0].children && old.cityLife.bank==3210 && serverIncomePerDay(old)==256,"legacy migration keeps family, money and income without inventing a move");
}
}
int main(){try{Fixture f;households(f);business(f);businessEconomy();persistence(f);std::cout<<"PASS mansion households, R home, cars, server contracts, costs, upgrades, bank settlement and save migration\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

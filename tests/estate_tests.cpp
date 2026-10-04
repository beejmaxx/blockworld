#include "estate.hpp"
#include "metropolis.hpp"
#include "garden.hpp"
#include "ranch.hpp"
#include "minimap.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace bw;
void check(bool yes,const std::string& what){if(!yes)throw std::runtime_error(what);}
void walk(World& w,Player& p,glm::vec3 target) {
  for(int frame=0;frame<2000;++frame) {
    auto d=glm::vec2(target.x-p.pose.position.x,target.z-p.pose.position.z);
    if(glm::length(d)<.09f)return;
    p.pose.yaw=std::atan2(d.x,-d.y);p.tick(w,{.forward=1},1.f/60);
    check(!p.collides(w,p.pose.position),"walking route clips a block");
  }
  throw std::runtime_error("walking route blocked: "+std::to_string(p.pose.position.x)+","+std::to_string(p.pose.position.y)+","+std::to_string(p.pose.position.z)+" -> "+std::to_string(target.x)+","+std::to_string(target.z));
}
int main(int argc,char** argv) {
  try {
    World w{7262026,true};Player p;p.pose.position={10.5f,24,7.5f};
    if(argc==3) {
      auto pose=w.load(argv[1]);check(pose.has_value(),"source save loads");p.pose=*pose;
      auto edits=w.editCount();auto family=w.cityLife.residents;auto bank=w.cityLife.bank;
      check(initializeEstate(w,p),"estate installs into real saved world");w.save(argv[2],p.pose);
      World loaded;check(loaded.load(argv[2]).has_value(),"migrated save reloads");
      check(loaded.editCount()==edits && loaded.cityLife.bank==bank && loaded.estateOrigin==w.estateOrigin,"edits, money and new district preserved");
      for(int i=0;i<residentCount;++i)check(loaded.cityLife.residents[i].children==family[i].children && loaded.cityLife.residents[i].dating==family[i].dating,"families preserved");
      std::cout<<"SAVE PASS: "<<edits<<" edits; bank "<<bank<<"; estate "<<w.estateOrigin->x<<","<<w.estateOrigin->z<<'\n';return 0;
    }
    w.ensure({0,0},1);initializeFarm(w);initializeHome(w);initializeGarden(w);
    check(initializeEstate(w,p),"new district initializes");auto o=*w.estateOrigin;auto base=glm::vec3(o.x,o.y,o.z);
    auto go=[&](glm::vec3 at){walk(w,p,base+at);};
    for(int i=0;i<6;++i)check(visitEstate(w,p,i),"each destination has a safe arrival");
    for(int i=0;i<3;++i) {
      check(visitEstate(w,p,i),"mansion arrival");int x=i==0?32:i==1?236:444;
      w.ensure(chunkAt(o.x+x+28,o.z+240),4);
      go({x+28.5f,0,237.5f});go({x+27.5f,0,239.5f});go({x+27.5f,0,252.5f});
      check(std::abs(p.pose.position.y-29)<.03f,"mansion stairs reach bedrooms");
      go({x+31.5f,6,252.5f});go({x+31.5f,6,239.5f});
      check(std::abs(p.pose.position.y-35)<.03f,"second flight reaches roof garden");
      go({x+31.5f,12,252.5f});go({x+27.5f,6,252.5f});go({x+27.5f,6,237.5f});
      check(std::abs(p.pose.position.y-23)<.03f,"stairs return to ground floor");
      go({x+28.5f,0,237.5f});go({x+28.5f,0,210.5f});
    }
    check(visitEstate(w,p,3),"yacht pier");w.ensure(chunkAt(o.x+370,o.z+352),3);
    go({390.5f,0,360.5f});go({379.5f,0,360.5f});go({379.5f,0,370.5f});go({365.5f,0,370.5f});go({365.5f,0,361.5f});
    check(std::abs(p.pose.position.y-23)<.03f,"walk from pier into yacht salon without flight");
    go({365.5f,0,370.5f});go({375.5f,0,370.5f});go({375.5f,0,353.5f});
    check(std::abs(p.pose.position.y-28)<.03f,"yacht stairs reach flybridge");
    go({362.5f,5,353.5f});go({362.5f,5,342.5f});
    check(std::abs(p.pose.position.y-32)<.03f,"yacht stairs reach sun deck");
    check(visitEstate(w,p,4),"terminal arrival");w.ensure(chunkAt(o.x+300,o.z+140),3);go({299.5f,0,108.5f});
    check(std::abs(p.pose.position.y-23)<.03f,"walk through terminal to apron");
    auto route=estateRoad(w);check(route.size()==3,"physical road connection");
    for(std::size_t n=1;n<route.size();++n) {
      float distance=glm::length(route[n]-route[n-1]);
      for(float d=0;d<=distance;d+=4) {
        auto at=glm::mix(route[n-1],route[n],d/distance);w.ensure(chunkAt(int(at.x),int(at.z)),1);
        FarmCar car;car.owned=true;car.position=at;car.yaw=1.5707963f;
        check(carFits(w,car),"city-to-estate road has continuous support and vehicle clearance");
      }
    }
    auto map=buildMiniMap(w,{o.x+370.f,o.z+350.f},true,false);int markers=0;
    for(auto marker:map.markers)if(marker.name=="Estates" || marker.name=="Yacht" || marker.name=="Airport") {
      ++markers;auto at=mapPoint(map,marker.position);check(at.x>=0 && at.x<=1 && at.y>=0 && at.y<=1,"destination marker inside overview");
    }
    check(markers==3,"district map destinations");
    Cell edit{o.x+51,28,o.z+249};w.set(edit,Block::Brick);
    auto path=std::filesystem::temp_directory_path()/("estate-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".bw");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}}cleanup{path};
    w.save(path,p.pose);World saved;check(saved.load(path).has_value() && saved.estateOrigin==w.estateOrigin,"district persists");
    saved.ensure(chunkAt(edit.x,edit.z),0);check(saved.get(edit)==Block::Brick,"player edits override generated mansion");
    std::vector<std::string> lines;std::ifstream input(path);for(std::string line;std::getline(input,line);)lines.push_back(line);input.close();
    int record=16+int(w.farm.chickens.size()+w.farm.crops.size()+w.farm.livestock.size()+w.road.size());
    std::vector<std::string> fields;std::istringstream tokens(lines.at(record));for(std::string value;tokens>>value;)fields.push_back(value);
    auto rewrite=[&](int version,const std::vector<std::string>& values){auto data=lines;data[0]="BLOCKWORLD "+std::to_string(version)+" 7262026 1";data[record].clear();for(auto& v:values)data[record]+=v+" ";std::ofstream out(path);for(auto& line:data)out<<line<<'\n';};
    auto invalid=fields;invalid[invalid.size()-2]="100";rewrite(20,invalid);
    bool rejected=false;try{saved.load(path);}catch(const std::exception&){rejected=true;}
    check(rejected && saved.estateOrigin==w.estateOrigin && saved.get(edit)==Block::Brick,"bad estate record rejected without changing the live world");
    auto legacy=fields;legacy.resize(legacy.size()-4);rewrite(19,legacy);
    World old;check(old.load(path).has_value() && !old.estateOrigin && old.editCount()==w.editCount(),"v19 migrates with its edits and no invented estate origin");
    // Older saves upgrade only on free land, even when the first parcel was edited.
    World blocked{7262026,true};Player home;home.pose.position={10.5f,24,7.5f};initializeFarm(blocked);initializeHome(blocked);initializeGarden(blocked);
    check(initializeMetropolis(blocked,home),"legacy city initializes");blocked.ensure(chunkAt(o.x+100,o.z+100),0);check(blocked.set({o.x+100,25,o.z+100},Block::Brick),"legacy parcel is actually edited");
    check(initializeEstate(blocked,home) && blocked.estateOrigin->x!=o.x,"new district skips an edited parcel");
    std::cout<<"ESTATE PASS: three mansions, six safe destinations, stairs, yacht boarding, airport, driving road, minimap, persistence and preservation.\n";
  } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}

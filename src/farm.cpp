#include "farm.hpp"
#include "ranch.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bw {
namespace {
constexpr float pi=3.14159265f;
bool overlap(Box a,Box b) {
  return a.max.x>b.min.x+.001f && a.min.x<b.max.x-.001f && a.max.y>b.min.y+.001f && a.min.y<b.max.y-.001f
      && a.max.z>b.min.z+.001f && a.min.z<b.max.z-.001f;
}
bool blocked(const World& world,glm::vec3 at,float scale=1) {
  Box box{at+scale*glm::vec3(-.24f,0,-.24f),at+scale*glm::vec3(.24f,.85f,.24f)};
  for(int z=int(std::floor(box.min.z));z<=int(std::floor(box.max.z));++z)
    for(int x=int(std::floor(box.min.x));x<=int(std::floor(box.max.x));++x) {
      if(!world.chunks.contains(chunkAt(x,z))) return true;
      for(int y=int(std::floor(box.min.y+.001f));y<=int(std::floor(box.max.y-.001f));++y) {
        Cell c{x,y,z}; auto block=world.get(c);
        if(collidable(block) && overlap(box,blockBounds(c,block))) return true;
      }
    }
  return false;
}
std::optional<glm::vec3> standing(const World& world,glm::vec3 at,float scale=1) {
  int top=int(std::floor(at.y+.02f));
  float radius=.24f*scale;
  for(int y=top;y>=top-2;--y) {
    // The leading edge of a foot reaches a step before the body's center does.
    // Sampling only the center traps chickens against the lip of shallow holes.
    bool supported=false;
    for(int z=int(std::floor(at.z-radius+.001f));z<=int(std::floor(at.z+radius-.001f));++z)
      for(int x=int(std::floor(at.x-radius+.001f));x<=int(std::floor(at.x+radius-.001f));++x) {
        auto floor=world.get({x,y,z});
        supported|=opaque(floor) || floor==Block::Glass;
      }
    if(!supported) continue;
    auto p=glm::vec3(at.x,float(y+1),at.z);
    if(p.y>at.y+1.01f || p.y<at.y-1.01f || blocked(world,p,scale)) continue;
    return p;
  }
  return {};
}
std::optional<float> rayBox(glm::vec3 origin,glm::vec3 direction,Box box,float reach) {
  float enter=0,leave=reach;
  for(int axis=0;axis<3;++axis) {
    if(std::abs(direction[axis])<1e-7f) {
      if(origin[axis]<box.min[axis] || origin[axis]>box.max[axis]) return {};
    } else {
      float a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];
      enter=std::max(enter,std::min(a,b)); leave=std::min(leave,std::max(a,b));
    }
  }
  return enter<=leave ? std::optional<float>(enter) : std::nullopt;
}
bool penPieceDone(const World& world,const BlueprintPiece& piece) {
  auto b=world.get(piece.cell);
  return piece.block==Block::Fence ? b==Block::Fence : isGate(b) && !gateAlongX(b);
}
std::optional<BlueprintPiece> nextPenPiece(const World& world,const Player& player) {
  std::optional<BlueprintPiece> next; float distance=std::numeric_limits<float>::infinity();
  for(const auto& piece : penBlueprint(world)) if(!penPieceDone(world,piece)) {
    auto delta=glm::vec3(piece.cell.x+.5f,piece.cell.y+.5f,piece.cell.z+.5f)-player.eye();
    if(glm::length(delta)<distance) { distance=glm::length(delta); next=piece; }
  }
  return next;
}
}
float chickenScale(const Chicken& c) { return .55f+.45f*std::clamp(c.growth/chickGrowSeconds,0.f,1.f); }
Box chickenBounds(const Chicken& c) { auto s=chickenScale(c); return {c.position+s*glm::vec3(-.24f,0,-.24f),c.position+s*glm::vec3(.24f,.85f,.24f)}; }
bool chickensOverlap(const World& world,Box box) {
  return ranchOverlap(world,box) || std::ranges::any_of(world.farm.chickens,[&](const auto& c){return overlap(box,chickenBounds(c));});
}
void initializeFarm(World& world) {
  auto& farm=world.farm;
  for(std::size_t i=0;i<farm.chickens.size();++i) if(farm.chickens[i].name.empty()) farm.chickens[i].name=animalName(farm.chickens[i],i);
  if(farm.initialized) return;
  if(!world.terrain.adventure()) farm.home.y=float(world.terrain.height(3,8)+1);
  auto center=chunkAt(int(farm.home.x),int(farm.home.z)); world.ensure(center,1);
  // Search free surfaces, preserving any structures in worlds made before animals existed.
  for(int radius=0;radius<=8 && farm.chickens.size()<starterFlock;++radius)
    for(int dz=-radius;dz<=radius && farm.chickens.size()<starterFlock;++dz)
      for(int dx=-radius;dx<=radius && farm.chickens.size()<starterFlock;++dx) {
        if(std::max(std::abs(dx),std::abs(dz))!=radius) continue;
        int x=int(std::floor(farm.home.x))+dx,z=int(std::floor(farm.home.z))+dz;
        for(int y=worldHeight-2;y>=1;--y) {
          auto b=world.get({x,y,z}); if(!opaque(b) || b==Block::Leaves) continue;
          Chicken c; c.position={x+.5f,float(y+1),z+.5f}; c.yaw=float(farm.chickens.size())*1.7f; c.name=animalName(c,farm.chickens.size());
          if(!blocked(world,c.position) && !chickensOverlap(world,chickenBounds(c))) farm.chickens.push_back(c);
          break;
        }
      }
  farm.initialized=farm.chickens.size()>=starterFlock;
}
std::string animalName(const Chicken& c,std::size_t index) {
  static constexpr std::array names{"Clover","Poppy","Hazel","Sunny","Pip","Peep","Daisy","Buttercup","Dot","Biscuit","Pebble","Maple"};
  return c.name.empty() ? names[index%names.size()] : c.name;
}
bool validAnimalName(std::string_view name) {
  if(name.empty() || name.size()>animalNameLimit || name.front()==' ' || name.back()==' ') return false;
  bool letter=false;
  for(char c : name) {
    bool alnum=(c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9');
    if(!alnum && c!=' ' && c!='-' && c!='.' && c!='\'') return false;
    letter|=alnum;
  }
  return letter;
}
bool renameChicken(World& world,std::size_t index,std::string_view name) {
  while(!name.empty() && name.front()==' ') name.remove_prefix(1);
  while(!name.empty() && name.back()==' ') name.remove_suffix(1);
  if(index>=world.farm.chickens.size() || !validAnimalName(name)) return false;
  world.farm.chickens[index].name=name; world.farm.flags|=AnimalNamed; return true;
}
std::string hatchProblem(const World& world,std::size_t mother) {
  const auto& flock=world.farm.chickens;
  if(mother>=flock.size()) return "CHOOSE A HEN FIRST";
  if(isChick(flock[mother])) return "CHICKS NEED TO GROW UP FIRST";
  if(flock[mother].hatchTimer>=0) return "ALREADY KEEPING AN EGG WARM";
  auto waiting=std::ranges::count_if(flock,[](const auto& c){return c.hatchTimer>=0;});
  if(flock.size()+waiting>=flockLimit) return "FLOCK FULL / 12 INCLUDING HATCHING EGGS";
  if(world.farm.eggs<1) return "COLLECT AN EGG FROM A FED HEN";
  return {};
}
bool incubateEgg(World& world,std::size_t mother) {
  if(!hatchProblem(world,mother).empty()) return false;
  --world.farm.eggs; world.farm.chickens[mother].hatchTimer=eggHatchSeconds;
  world.farm.flags|=EggIncubated; return true;
}
void growFarm(World& world,float seconds) {
  if(!std::isfinite(seconds) || seconds<=0) return;
  world.growCrops(seconds);
  for(auto& animal : world.farm.livestock) animal.milkTimer=std::max(0.f,animal.milkTimer-seconds);
  auto& flock=world.farm.chickens;
  const auto before=flock.size();
  for(std::size_t i=0;i<before;++i) {
    auto& c=flock[i];
    c.growth=std::min(chickGrowSeconds,c.growth+seconds);
    if(c.eggTimer>=0 && !c.eggReady) {
      c.eggTimer=std::max(0.f,c.eggTimer-seconds);
      if(c.eggTimer==0) { c.eggReady=true; c.eggTimer=-1; }
    }
    if(c.hatchTimer<0) continue;
    float oldTimer=c.hatchTimer;
    c.hatchTimer=std::max(0.f,c.hatchTimer-seconds);
    if(c.hatchTimer>0 || flock.size()>=flockLimit) continue;
    // Wait for a loaded, clear patch beside the mother. Never overwrite a build.
    Chicken baby; baby.mother=int(i); baby.growth=std::min(chickGrowSeconds,seconds-oldTimer);
    baby.name=animalName(baby,flock.size()); baby.yaw=c.yaw; baby.happy=2;
    std::optional<glm::vec3> spot;
    for(float radius : {.75f,1.2f,1.8f}) {
      for(int direction=0;direction<8 && !spot;++direction) {
        float angle=float(direction)*pi*.25f;
        auto candidate=standing(world,c.position+glm::vec3(std::sin(angle)*radius,0,std::cos(angle)*radius),chickenScale(baby));
        if(!candidate) continue;
        auto eye=c.position+glm::vec3(0,.4f,0),delta=*candidate+glm::vec3(0,.3f,0)-eye;
        if(world.raycast(eye,delta,glm::length(delta))) continue;
        baby.position=*candidate;
        if(!chickensOverlap(world,chickenBounds(baby))) spot=candidate;
      }
      if(spot) break;
    }
    if(!spot) continue;
    c.hatchTimer=-1; c.happy=2; world.farm.flags|=ChickHatched;
    flock.push_back(std::move(baby)); // Do not use c after this potentially reallocates.
  }
}
void tickFarm(World& world,const Player& player,float dt,bool offeringWheat) {
  if(!std::isfinite(dt) || dt<=0) return;
  dt=std::min(dt,.1f); growFarm(world,dt);
  if(!(world.farm.flags&PenBuilt) && penComplete(world)) world.farm.flags|=PenBuilt;
  auto& flock=world.farm.chickens;
  for(std::size_t i=0;i<flock.size();++i) {
    auto& c=flock[i]; float scale=chickenScale(c); c.happy=std::max(0.f,c.happy-dt);
    c.sleeping=world.clock.sky().daylight<.12f; c.moving=false;
    if(!world.chunks.contains(chunkAt(int(std::floor(c.position.x)),int(std::floor(c.position.z))))) continue;
    // Gravity also handles a player removing the floor underneath a chicken.
    c.fallSpeed=std::max(-12.f,c.fallSpeed-18.f*dt);
    for(int sub=0;sub<6;++sub) {
      auto next=c.position; next.y+=c.fallSpeed*dt/6;
      if(blocked(world,next,scale)) {
        float lo=0,hi=1;
        for(int j=0;j<10;++j) { float mid=(lo+hi)*.5f; auto p=glm::mix(c.position,next,mid); if(blocked(world,p,scale)) hi=mid; else lo=mid; }
        c.position=glm::mix(c.position,next,lo); c.fallSpeed=0; break;
      }
      c.position=next;
    }
    if(c.sleeping || c.hatchTimer>=0) continue;
    auto delta=player.pose.position-c.position; delta.y=0;
    float distance=glm::length(delta);
    bool baby=isChick(c),hasMother=baby && c.mother>=0 && std::size_t(c.mother)<flock.size();
    if(hasMother) { delta=flock[c.mother].position-c.position; delta.y=0; distance=glm::length(delta); }
    bool follow=hasMother ? distance>.85f : offeringWheat && world.farm.wheat>0 && distance<8 && distance>1.05f;
    c.think-=dt;
    if(c.think<=0) {
      float choice=std::sin(c.yaw*17.3f+world.clock.phase*813.f+float(i)*3.1f);
      c.heading=std::remainder(c.yaw+choice*2.1f,2*pi); c.think=1.5f+std::abs(choice)*2;
    }
    glm::vec3 wanted{std::sin(c.heading),0,-std::cos(c.heading)};
    if(follow) wanted=delta/distance;
    else {
      auto home=(hasMother ? flock[c.mother].position : world.farm.home)-c.position; home.y=0;
      if(glm::length(home)>(hasMother ? 1.2f : 9.f)) wanted=glm::normalize(home);
      else if(c.think<.8f) continue; // Pause to peck between short walks.
    }
    float speed=follow ? (baby ? 2.25f : 2.f) : (baby ? .4f : .65f);
    std::optional<glm::vec3> step;
    // Small local detours let the flock negotiate corners without a per-frame path search.
    for(float angle : {0.f,.65f,-.65f,1.2f,-1.2f}) {
      glm::vec3 direction{wanted.x*std::cos(angle)-wanted.z*std::sin(angle),0,wanted.x*std::sin(angle)+wanted.z*std::cos(angle)};
      auto candidate=standing(world,c.position+direction*speed*dt,scale);
      if(!candidate) continue;
      bool crowd=false;
      for(std::size_t j=0;j<flock.size();++j) if(i!=j && glm::length(*candidate-flock[j].position)<.24f*(scale+chickenScale(flock[j]))+.02f
          && glm::length(*candidate-flock[j].position)<glm::length(c.position-flock[j].position)) crowd=true;
      Chicken candidateBird=c; candidateBird.position=*candidate;
      if(crowd || ranchOverlap(world,chickenBounds(candidateBird))) continue;
      float turn=std::remainder(std::atan2(direction.x,-direction.z)-c.yaw,2*pi);
      c.yaw=std::remainder(c.yaw+std::clamp(turn,-3.f*dt,3.f*dt),2*pi);
      step=candidate; break;
    }
    if(step) { c.position=*step; c.walk+=speed*dt*8; c.moving=true; }
    else c.think=.5f; // Rest before choosing another route; never spin when blocked.
  }
}
std::optional<std::size_t> targetChicken(const World& world,const Player& player,float reach) {
  float nearest=reach;
  if(auto hit=world.raycast(player.eye(),player.direction(),reach)) nearest=hit->distance;
  std::optional<std::size_t> result;
  for(std::size_t i=0;i<world.farm.chickens.size();++i) {
    const auto& c=world.farm.chickens[i];
    if(!world.chunks.contains(chunkAt(int(std::floor(c.position.x)),int(std::floor(c.position.z))))) continue;
    auto box=chickenBounds(c); box.min.x-=.12f; box.max.x+=.12f; box.min.z-=.12f; box.max.z+=.12f;
    if(auto distance=rayBox(player.eye(),player.direction(),box,nearest)) { nearest=*distance; result=i; }
  }
  return result;
}
std::string chickenPrompt(const World& world,std::size_t index) {
  if(index>=world.farm.chickens.size()) return {};
  const auto& c=world.farm.chickens[index];
  if(isChick(c)) return "CHICK / V PET / P FARM";
  if(c.eggReady) return world.farm.eggs>=9999 ? "EGG BASKET FULL" : "CHICKEN / V COLLECT EGG";
  if(c.hatchTimer>=0) return c.hatchTimer>0 ? "EGG HATCHES IN "+std::to_string(int(std::ceil(c.hatchTimer)))+"S / P FARM" : "EGG READY / NEEDS CLEAR SPACE";
  if(c.eggTimer>=0) return "HAPPY CHICKEN / EGG IN "+std::to_string(int(std::ceil(c.eggTimer)))+"S";
  if(world.clock.sky().daylight<.12f) return "CHICKEN / SLEEPING UNTIL MORNING";
  return world.farm.wheat>0 ? "CHICKEN / V FEED WHEAT" : "CHICKEN / GROW WHEAT TO FEED ME";
}
FarmUse useChicken(World& world,const Player& player,std::size_t index) {
  if(targetChicken(world,player)!=index) return FarmUse::None;
  auto& c=world.farm.chickens[index];
  if(isChick(c)) { c.happy=2; return FarmUse::Petted; }
  if(c.eggReady && world.farm.eggs<9999) {
    c.eggReady=false; ++world.farm.eggs; world.farm.flags|=EggCollected; c.happy=2; return FarmUse::Collected;
  }
  if(c.hatchTimer>=0) return FarmUse::None;
  if(!c.eggReady && c.eggTimer<0 && world.clock.sky().daylight>=.12f && world.farm.wheat>0) {
    --world.farm.wheat; c.eggTimer=eggLaySeconds; c.happy=2; world.farm.flags|=ChickenFed; return FarmUse::Fed;
  }
  return FarmUse::None;
}
FarmUse harvestWheat(World& world,const Player& player,Cell cell) {
  return isWheat(world.get(cell)) ? harvestCrop(world,player,cell) : FarmUse::None;
}
FarmUse harvestCrop(World& world,const Player& player,Cell cell) {
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  auto block=world.get(cell);
  if(!hit || hit->block!=cell || !cropRipe(block)) return FarmUse::None;
  auto kind=cropKind(block);
  auto crop=world.cropAt(cell);
  int amount=cropYield(kind)+(crop && crop->composted ? 2 : 0);
  // Berry bushes stay rooted and flower again; annual crops leave a prepared bed.
  auto next=kind==CropKind::Strawberry ? Block::StrawberryGrowing : Block::Air;
  if(world.farm.harvest(kind)>9999-amount || !world.set(cell,next)) return FarmUse::None;
  world.farm.harvest(kind)+=amount;
  world.farm.garden.flags|=PickedCrop;
  if(kind==CropKind::Wheat) world.farm.flags|=WheatHarvested;
  return FarmUse::Harvested;
}
bool waterCrop(World& world,const Player& player,Cell cell) {
  if(world.inventory.held()!=Item::WateringCan) return false;
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  return hit && hit->block==cell && world.waterCrop(cell);
}
std::string cropPrompt(const World& world,Cell cell,bool wateringCan) {
  const auto* crop=world.cropAt(cell);
  if(!crop) return "HOE SOIL / THEN PLANT SEEDS";
  std::string name(cropName(crop->kind));
  if(crop->age>=cropGrowSeconds(crop->kind)) return name+" RIPE / V PICK +"+std::to_string(cropYield(crop->kind)+(crop->composted ? 2 : 0));
  int percent=int(crop->age/cropGrowSeconds(crop->kind)*100.f);
  return name+" / GROWTH "+std::to_string(percent)+" OF 100"+(crop->water>0 ? " / WATERED" : wateringCan ? " / V WATER" : " / P FOR TOOLS")+(crop->composted ? " / COMPOST +2" : "");
}
bool toggleGate(World& world,const Player& player,Cell cell) {
  auto b=world.get(cell); if(!isGate(b)) return false;
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  if(!hit || hit->block!=cell) return false;
  auto next=gateVariant(gateAlongX(b),!gateOpen(b));
  if(player.overlaps(cell,next) || (!gateOpen(next) && chickensOverlap(world,blockBounds(cell,next)))) return false;
  return world.set(cell,next);
}
std::vector<BlueprintPiece> penBlueprint(const World& world) {
  std::vector<BlueprintPiece> pieces;
  int x=int(std::floor(world.farm.home.x)),y=int(std::floor(world.farm.home.y)),z=int(std::floor(world.farm.home.z));
  for(int dz=-3;dz<=3;++dz) for(int dx=-3;dx<=3;++dx) if(std::abs(dx)==3 || std::abs(dz)==3)
    pieces.push_back({{x+dx,y,z+dz},dx==0 && dz==3 ? Block::GateZ : Block::Fence,CabinPart::Frame});
  return pieces;
}
bool penComplete(const World& world) {
  auto pieces=penBlueprint(world);
  return std::ranges::all_of(pieces,[&](const auto& piece){return penPieceDone(world,piece);});
}
bool buildPenNext(World& world,const Player& player) {
  auto piece=nextPenPiece(world,player);
  if(!piece || glm::length(glm::vec3(piece->cell.x+.5f,piece->cell.y+.5f,piece->cell.z+.5f)-player.eye())>7) return false;
  return placeBlock(world,player,piece->cell,piece->block);
}
GuideView farmGuide(const World& world,const Player& player) {
  GuideView view; view.enabled=true; view.farm=true;
  constexpr std::array milestones{PenBuilt,WheatPlanted,WheatHarvested,ChickenFed,EggCollected,EggIncubated,ChickHatched,AnimalNamed};
  while(view.stage<int(milestones.size()) && (world.farm.flags&milestones[view.stage])) ++view.stage;
  view.destination=world.farm.home; view.destinationName="CHICKEN PEN";
  switch(view.stage) {
    case 0: {
      view.title="01 / MAKE A CHICKEN PEN";
      view.lines={"Hold G to build the glowing fence.","Walk around to reach every piece.","E / Build has fences and gates."};
      if(auto next=nextPenPiece(world,player)) {
        view.preview=next->cell; view.previewBlock=next->block;
        view.destination=glm::vec3(next->cell.x+.5f,next->cell.y+.5f,next->cell.z+.5f);
        auto status=placementStatus(world,player,next->cell,next->block);
        if(status!=PlacementStatus::Ready) view.lines[0]=placementMessage(status);
      }
      break;
    }
    case 1: view.title="02 / PLANT SOME WHEAT";
      view.lines={"Use the hoe on grass to prepare soil.","Choose wheat, then click or V.","Seeds are unlimited. Try a small patch."}; break;
    case 2: view.title="03 / WATCH YOUR GARDEN GROW";
      view.lines={"P has a watering can to help it grow.","Click or V on golden wheat.","Plant new seeds after the harvest."}; break;
    case 3: view.title="04 / FEED YOUR CHICKENS";
      view.lines={"Hold wheat. Nearby chickens follow.","V opens the gate.","Click or V feeds one wheat."}; break;
    case 4: view.title="05 / YOUR FIRST EGG";
      view.lines={"A fed chicken lays an egg in 30 seconds.","V collects its egg.","V closes the gate."}; break;
    case 5: view.title="06 / HATCH A BABY CHICK";
      view.lines={"Press P to open your Farm page.","Choose a hen and click HATCH ONE EGG.","She keeps it warm for 60 play seconds."}; break;
    case 6: view.title="07 / A NEW LITTLE FRIEND";
      view.lines={"Return to the world while the egg warms.","Keep a clear patch beside the mother.","Your chick will hatch and follow her."}; break;
    case 7: view.title="08 / GIVE SOMEONE A NAME";
      view.lines={"Press P and choose your new chick.","Click its name. Type a name, then Enter.","Chicks grow up in three play minutes."}; break;
    default: view.title="YOUR LITTLE FARM";
      view.lines={"P has carrots, berries, and pumpkins.","Water plants to help them grow faster.","Your crops stay safe if you forget."}; view.destination.reset(); break;
  }
  return view;
}
FarmView farmView(const World& world) {
  FarmView view; view.wheat=world.farm.wheat; view.eggs=world.farm.eggs; view.plants=int(world.farm.crops.size());
  view.garden=world.farm.garden;
  view.milk=world.farm.milk; view.basketValue=view.milk*5; view.carOwned=world.farm.car.owned;
  for(const auto& animal : world.farm.livestock) {
    if(animal.kind==LivestockKind::Cow) ++view.cows;
    else if(animal.kind==LivestockKind::Horse) ++view.horses;
    else if(animal.kind==LivestockKind::Sheep) ++view.sheep;
    else if(animal.kind==LivestockKind::Fox) ++view.foxes;
  }
  for(auto kind : cropKinds) { view.harvest[int(kind)]=world.farm.harvest(kind); view.basketValue+=world.farm.harvest(kind)*cropPrice(kind); }
  for(const auto& crop : world.farm.crops) {
    if(crop.age>=cropGrowSeconds(crop.kind)) ++view.ripe;
    if(crop.water>0) ++view.watered;
  }
  for(std::size_t i=0;i<world.farm.chickens.size();++i) {
    const auto& c=world.farm.chickens[i]; AnimalView animal;
    animal.name=animalName(c,i); animal.baby=isChick(c); animal.growth=c.growth/chickGrowSeconds; animal.hatching=c.hatchTimer;
    if(c.mother>=0 && std::size_t(c.mother)<world.farm.chickens.size()) animal.mother=animalName(world.farm.chickens[c.mother],std::size_t(c.mother));
    animal.hatchProblem=hatchProblem(world,i);
    if(animal.baby) animal.status="GROWN IN "+std::to_string(int(std::ceil(chickGrowSeconds-c.growth)))+"S";
    else if(c.hatchTimer>=0) animal.status=c.hatchTimer>0 ? "HATCHING IN "+std::to_string(int(std::ceil(c.hatchTimer)))+"S" : "READY / NEEDS CLEAR GROUND NEARBY";
    else if(c.eggReady) animal.status="EGG READY TO COLLECT";
    else if(c.eggTimer>=0) animal.status="LAYING IN "+std::to_string(int(std::ceil(c.eggTimer)))+"S";
    else animal.status="FEED WHEAT TO GET AN EGG";
    view.animals.push_back(std::move(animal));
  }
  return view;
}
std::vector<Vertex> chickenMesh(const World& world) {
  std::vector<Vertex> vertices; vertices.reserve(chickenVertexLimit);
  for(std::size_t i=0;i<world.farm.chickens.size();++i) {
    const auto& c=world.farm.chickens[i];
    if(!world.chunks.contains(chunkAt(int(std::floor(c.position.x)),int(std::floor(c.position.z))))) continue;
    bool baby=isChick(c); float scale=chickenScale(c);
    float sway=c.moving ? std::sin(c.walk)*.04f : 0,body=c.sleeping || c.hatchTimer>=0 ? -.10f : 0;
    float peck=!c.moving && !c.sleeping ? std::max(0.f,std::sin(c.think*8))*.11f : 0;
    auto box=[&](glm::vec3 lo,glm::vec3 hi,float material) {
      auto first=vertices.size(); appendBox(vertices,{lo,hi},{-100002,0,int(i)},material,1);
      constexpr std::array shade{.82f,.68f,1.f,.60f,.87f,.76f};
      for(auto j=first;j<vertices.size();++j) {
        auto p=vertices[j].position*scale;
        vertices[j].position=c.position+glm::vec3(p.x*std::cos(c.yaw)-p.z*std::sin(c.yaw),p.y,p.x*std::sin(c.yaw)+p.z*std::cos(c.yaw));
        vertices[j].light=shade[(j-first)/6];
      }
    };
    float feather=baby ? 25.f : i==2 ? 30.f : 26.f;
    box({-.22f,.22f+body,-.23f},{.22f,.61f+body,.26f},feather);
    box({-.14f,.53f+body-peck,-.38f},{.14f,.86f+body-peck,-.12f},feather);
    box({-.11f,.57f+body-peck,-.49f},{.11f,.66f+body-peck,-.36f},27);
    if(!baby) {
      box({-.045f,.83f+body-peck,-.34f},{.045f,.94f+body-peck,-.15f},28);
      box({-.07f,.47f+body-peck,-.39f},{.07f,.57f+body-peck,-.34f},28);
    }
    float eyeHeight=c.sleeping ? .012f : .047f;
    box({-.147f,.73f+body-peck,-.32f},{-.139f,.73f+body-peck+eyeHeight,-.263f},29);
    box({.139f,.73f+body-peck,-.32f},{.147f,.73f+body-peck+eyeHeight,-.263f},29);
    box({-.27f,.30f+body+sway,-.05f},{-.215f,.53f+body+sway,.20f},feather);
    box({.215f,.30f+body-sway,-.05f},{.27f,.53f+body-sway,.20f},feather);
    box({-.12f,.40f+body,.24f},{.12f,.66f+body,.38f},feather);
    for(float x : {-.11f,.11f}) {
      float stride=c.moving ? std::sin(c.walk)*(x<0 ? .06f : -.06f) : 0;
      box({x-.025f,.04f,-.03f+stride},{x+.025f,.26f,.025f+stride},27);
      box({x-.055f,.02f,-.14f+stride},{x+.055f,.055f,.03f+stride},27);
    }
    if(c.hatchTimer>=0) {
      box({-.37f,.01f,-.30f},{.37f,.08f,.31f},25);
      box({-.34f,.08f,.16f},{-.19f,.25f,.30f},31);
    }
    if(c.eggReady) box({.28f,.015f,-.05f},{.43f,.21f,.11f},31);
    if(c.happy>0) {
      float y=1.08f+(2-c.happy)*.16f;
      box({-.12f,y,0},{-.01f,y+.10f,.04f},32); box({.01f,y,0},{.12f,y+.10f,.04f},32);
      box({-.07f,y-.09f,0},{.07f,y+.025f,.04f},32);
    }
  }
  return vertices;
}
} // namespace bw

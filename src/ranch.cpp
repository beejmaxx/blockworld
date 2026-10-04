#include "ranch.hpp"
#include "city_life.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr float pi=3.14159265f;
constexpr float carHalfWidth=.95f,carHalfLength=2.23f;
float animalRadius(LivestockKind kind) { return kind==LivestockKind::Fox ? .64f : kind==LivestockKind::Sheep ? .85f : 1.12f; }
float animalHeight(LivestockKind kind) { return kind==LivestockKind::Fox ? .95f : kind==LivestockKind::Sheep ? 1.45f : kind==LivestockKind::Horse ? 2.25f : 1.8f; }
glm::vec3 carOffset(float yaw,float x,float z) {
  return {x*std::cos(yaw)-z*std::sin(yaw),0,x*std::sin(yaw)+z*std::cos(yaw)};
}
bool overlaps(Box a,Box b) {
  return a.max.x>b.min.x+.001f && a.min.x<b.max.x-.001f && a.max.y>b.min.y+.001f && a.min.y<b.max.y-.001f
    && a.max.z>b.min.z+.001f && a.min.z<b.max.z-.001f;
}
Box body(glm::vec3 p,float radius,float height) { return {p+glm::vec3(-radius,.02f,-radius),p+glm::vec3(radius,height,radius)}; }
bool carOverlaps(const FarmCar& car,Box box) {
  if(car.position.y+1.5f<=box.min.y+.001f || car.position.y+.02f>=box.max.y-.001f) return false;
  glm::vec2 center{car.position.x,car.position.z};
  glm::vec2 other{(box.min.x+box.max.x)*.5f,(box.min.z+box.max.z)*.5f};
  glm::vec2 half{(box.max.x-box.min.x)*.5f,(box.max.z-box.min.z)*.5f};
  glm::vec2 right{std::cos(car.yaw),std::sin(car.yaw)},forward{-right.y,right.x};
  for(auto axis : {glm::vec2(1,0),glm::vec2(0,1),right,forward}) {
    float reach=carHalfWidth*std::abs(glm::dot(right,axis))+carHalfLength*std::abs(glm::dot(forward,axis))
      +half.x*std::abs(axis.x)+half.y*std::abs(axis.y);
    if(std::abs(glm::dot(other-center,axis))>=reach-.001f) return false;
  }
  return true;
}
bool blocked(const World& world,Box box) {
  for(int z=int(std::floor(box.min.z));z<=int(std::floor(box.max.z));++z)
    for(int x=int(std::floor(box.min.x));x<=int(std::floor(box.max.x));++x) {
      if(std::abs(x)>coordinateLimit-3 || std::abs(z)>coordinateLimit-3 || !world.chunks.contains(chunkAt(x,z))) return true;
      for(int y=int(std::floor(box.min.y));y<=int(std::floor(box.max.y-.001f));++y) {
        Cell c{x,y,z}; auto b=world.get(c);
        if(collidable(b) && overlaps(box,blockBounds(c,b))) return true;
      }
    }
  return false;
}
bool occupied(const World& world,Box box,int skip=-1,bool skipCar=false) {
  for(const auto& c : world.farm.chickens) if(overlaps(box,chickenBounds(c))) return true;
  for(std::size_t i=0;i<world.farm.livestock.size();++i)
    if(int(i)!=skip && overlaps(box,livestockBounds(world.farm.livestock[i]))) return true;
  return !skipCar && world.farm.car.owned && carOverlaps(world.farm.car,box);
}
std::optional<glm::vec3> ground(const World& world,glm::vec3 at,float radius,float height,int skip=-1,bool skipCar=false,int drop=2) {
  int top=std::min(worldHeight-2,int(std::floor(at.y+.05f)));
  auto floorAt=[&](float x,int y,float z) {
    auto b=world.get({int(std::floor(x)),y,int(std::floor(z))});
    return opaque(b) && b!=Block::Leaves && b!=Block::Fence && !isGate(b);
  };
  for(int y=top;y>=std::max(0,top-drop);--y) {
    bool floor=floorAt(at.x,y,at.z);
    if(!floor) continue;
    glm::vec3 p{at.x,float(y+1),at.z}; auto box=body(p,radius,height);
    if(blocked(world,box) || occupied(world,box,skip,skipCar)) continue;
    // Support every corner so a parked car or animal cannot bridge a deep hole.
    bool supported=true;
    float supportRadius=radius*.8f;
    for(float x : {-supportRadius,supportRadius}) for(float z : {-supportRadius,supportRadius}) {
      if(!floorAt(p.x+x,y,p.z+z)) supported=false;
    }
    if(supported) return p;
  }
  return {};
}
bool carClear(const World& world,const FarmCar& car,bool driven) {
  auto bounds=carBounds(car);
  for(int z=int(std::floor(bounds.min.z));z<=int(std::floor(bounds.max.z));++z)
    for(int x=int(std::floor(bounds.min.x));x<=int(std::floor(bounds.max.x));++x) {
      if(std::abs(x)>coordinateLimit-3 || std::abs(z)>coordinateLimit-3 || !world.chunks.contains(chunkAt(x,z))) return false;
      for(int y=int(std::floor(bounds.min.y));y<=int(std::floor(bounds.max.y));++y) {
        Cell c{x,y,z}; auto b=world.get(c);
        if(collidable(b) && carOverlaps(car,blockBounds(c,b))) return false;
      }
    }
  for(const auto& c : world.farm.chickens) if(carOverlaps(car,chickenBounds(c))) return false;
  for(const auto& a : world.farm.livestock) if(carOverlaps(car,livestockBounds(a))) return false;
  if(cityPeopleOverlap(world,bounds))return false;
  Player occupant;
  return !driven || !occupant.collides(world,car.position+glm::vec3(0,.32f,0));
}
std::optional<glm::vec3> carGround(const World& world,glm::vec3 at,float yaw,bool driven,int drop=2) {
  auto floorAt=[&](glm::vec3 p,int y) {
    auto b=world.get({int(std::floor(p.x)),y,int(std::floor(p.z))});
    return opaque(b) && b!=Block::Leaves && b!=Block::Fence && !isGate(b);
  };
  int top=std::min(worldHeight-2,int(std::floor(at.y+.05f)));
  for(int y=top;y>=std::max(0,top-drop);--y) {
    bool floor=floorAt(at,y);
    // Test the entire oriented footprint: a diagonal bumper can hit the corner
    // of a road step between any fixed set of point samples.
    FarmCar footprint{true,{at.x,float(y),at.z},yaw,0}; auto bounds=carBounds(footprint);
    for(int z=int(std::floor(bounds.min.z));!floor && z<=int(std::floor(bounds.max.z));++z)
      for(int x=int(std::floor(bounds.min.x));!floor && x<=int(std::floor(bounds.max.x));++x) {
        auto b=world.get({x,y,z});
        floor=opaque(b) && b!=Block::Leaves && carOverlaps(footprint,blockBounds({x,y,z},b));
      }
    if(!floor) continue;
    bool supported=true;
    for(float x : {-.8f,.8f}) for(float z : {-1.3f,1.3f}) {
      auto wheel=at+carOffset(yaw,x,z);
      if(!floorAt(wheel,y) && !floorAt(wheel,y-1)) supported=false;
    }
    if(!supported) continue;
    FarmCar candidate{true,{at.x,float(y+1),at.z},yaw,0};
    if(carClear(world,candidate,driven)) return candidate.position;
  }
  return {};
}
std::optional<glm::vec3> delivery(const World& world,const Player& player,bool car,LivestockKind kind=LivestockKind::Cow) {
  for(float radius : {4.f,6.f,8.f,10.f,12.f}) for(int direction=0;direction<16;++direction) {
    float angle=player.pose.yaw+direction*pi/8;
    auto at=player.pose.position+glm::vec3(std::sin(angle)*radius,1,-std::cos(angle)*radius);
    auto spot=car ? carGround(world,at,player.pose.yaw,true,worldHeight) : ground(world,at,animalRadius(kind),animalHeight(kind),-1,false,worldHeight);
    if(spot) {
      bool overlap=car ? carOverlaps(FarmCar{true,*spot,player.pose.yaw,0},body(player.pose.position,.3f,1.8f))
        : overlaps(body(player.pose.position,.3f,1.8f),body(*spot,animalRadius(kind),animalHeight(kind)));
      if(!player.collides(world,*spot) && !overlap) return spot;
    }
  }
  return {};
}
std::optional<float> rayBox(glm::vec3 origin,glm::vec3 direction,Box box,float reach) {
  float enter=0,leave=reach;
  for(int axis=0;axis<3;++axis) {
    if(std::abs(direction[axis])<1e-7f) { if(origin[axis]<box.min[axis] || origin[axis]>box.max[axis]) return {}; }
    else {
      float a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];
      enter=std::max(enter,std::min(a,b)); leave=std::min(leave,std::max(a,b));
    }
  }
  return enter<=leave ? std::optional<float>(enter) : std::nullopt;
}
glm::vec3 seat(const World& world,const RideState& ride) {
  return ride.car ? world.farm.car.position+glm::vec3(0,.32f,0) : world.farm.livestock[ride.index].position+glm::vec3(0,.9f,0);
}
}
Box livestockBounds(const Livestock& animal) {
  if(animal.kind==LivestockKind::Fox) {
    // Include the white tail tip and ears when picking or protecting the fox.
    auto center=animal.position+carOffset(animal.yaw,0,.165f);
    float c=std::abs(std::cos(animal.yaw)),s=std::abs(std::sin(animal.yaw));
    glm::vec3 half{c*.3f+s*1.065f,0,s*.3f+c*1.065f};
    return {center-half+glm::vec3(0,.02f,0),center+half+glm::vec3(0,1.01f,0)};
  }
  return body(animal.position,animalRadius(animal.kind),animalHeight(animal.kind));
}
Box carBounds(const FarmCar& car) {
  float c=std::abs(std::cos(car.yaw)),s=std::abs(std::sin(car.yaw));
  glm::vec3 half{c*carHalfWidth+s*carHalfLength,0,s*carHalfWidth+c*carHalfLength};
  return {car.position-half+glm::vec3(0,.02f,0),car.position+half+glm::vec3(0,1.5f,0)};
}
bool carFits(const World& w,const FarmCar& car) {return carClear(w,car,true);}
bool ranchOverlap(const World& world,Box box) {
  if(cityPeopleOverlap(world,box))return true;
  for(const auto& animal : world.farm.livestock) if(overlaps(box,livestockBounds(animal))) return true;
  return world.farm.car.owned && carOverlaps(world.farm.car,box);
}
std::optional<RanchTarget> targetRanch(const World& world,const Player& player,float reach) {
  float nearest=reach;
  if(auto hit=world.raycast(player.eye(),player.direction(),reach)) nearest=hit->distance;
  for(const auto& chicken : world.farm.chickens) if(auto distance=rayBox(player.eye(),player.direction(),chickenBounds(chicken),nearest)) nearest=*distance;
  std::optional<RanchTarget> result;
  for(std::size_t i=0;i<world.farm.livestock.size();++i) {
    const auto& animal=world.farm.livestock[i];
    if(!world.chunks.contains(chunkAt(int(std::floor(animal.position.x)),int(std::floor(animal.position.z))))) continue;
    if(auto distance=rayBox(player.eye(),player.direction(),livestockBounds(animal),nearest)) { nearest=*distance; result=RanchTarget{false,i,nearest}; }
  }
  if(world.farm.car.owned) {
    const auto& car=world.farm.car;
    auto local=[&](glm::vec3 p) { auto xz=carOffset(-car.yaw,p.x,p.z); xz.y=p.y; return xz; };
    if(auto distance=rayBox(local(player.eye()-car.position),local(player.direction()),{{-carHalfWidth,.02f,-carHalfLength},{carHalfWidth,1.5f,carHalfLength}},nearest))
      result=RanchTarget{true,0,*distance};
  }
  return result;
}
std::string buyLivestock(World& world,const Player& player,LivestockKind kind) {
  if(kind>LivestockKind::Fox) return "Unknown animal";
  if(world.farm.livestock.size()>=livestockLimit) return "Your pasture is full (12 animals)";
  if(world.farm.garden.coins<livestockPrice(kind)) return "Sell crops in the shop to earn more coins";
  auto spot=delivery(world,player,false,kind);
  if(!spot) return "Step outside near a clear, level patch first";
  Livestock animal; animal.kind=kind; animal.position=animal.home=*spot; animal.yaw=player.pose.yaw;
  world.farm.livestock.push_back(animal); world.farm.garden.coins-=livestockPrice(kind); return {};
}
void initializeCastlePets(World& world) {
  if(!world.castleOrigin) return;
  auto o=*world.castleOrigin;
  world.ensure(chunkAt(o.x+15,o.z+19),2);
  Player scout; scout.pose.position={o.x+15.5f,float(o.y),o.z+21.5f}; scout.pose.yaw=0;
  for(auto kind : {LivestockKind::Sheep,LivestockKind::Fox}) {
    if(world.farm.livestock.size()>=livestockLimit || std::ranges::any_of(world.farm.livestock,[&](const auto& a){return a.kind==kind;})) continue;
    if(auto spot=delivery(world,scout,false,kind)) {
      Livestock animal; animal.kind=kind; animal.position=animal.home=*spot; animal.yaw=pi;
      world.farm.livestock.push_back(animal);
    }
  }
}
std::string bringCar(World& world,const Player& player) {
  auto spot=delivery(world,player,true);
  if(!spot) return "Step outside near a clear, level patch for your car";
  world.farm.car={true,*spot,player.pose.yaw,0}; return {};
}
std::string recoverCar(World& world,Player& player,const RideState& ride) {
  if(!ride.active || !ride.car) return "Get into your car first";
  auto problem=bringCar(world,player);
  if(problem.empty()) { player.pose.position=seat(world,ride); player.stopFlying(); }
  return problem;
}
bool mountRanch(World& world,Player& player,RideState& ride,RanchTarget target) {
  if(ride.active) return false;
  auto actual=targetRanch(world,player);
  if(!actual || actual->car!=target.car || actual->index!=target.index) return false;
  if(!target.car && world.farm.livestock[target.index].kind!=LivestockKind::Horse) return false;
  RideState candidate{true,target.car,target.index};
  if(player.collides(world,seat(world,candidate))) return false;
  ride=candidate; player.stopFlying(); player.sneaking=false; player.pose.position=seat(world,ride);
  player.pose.yaw=ride.car ? world.farm.car.yaw : world.farm.livestock[ride.index].yaw; player.pose.pitch=-.12f;
  world.farm.car.speed=0; return true;
}
bool leaveRide(World& world,Player& player,RideState& ride) {
  if(!ride.active) return true;
  auto origin=ride.car ? world.farm.car.position : world.farm.livestock[ride.index].position;
  for(float radius : {2.2f,3.2f,4.2f}) for(int direction=0;direction<12;++direction) {
    float angle=player.pose.yaw+pi*.5f+direction*pi/6;
    auto at=origin+glm::vec3(std::sin(angle)*radius,0,-std::cos(angle)*radius);
    if(auto spot=ground(world,at,.31f,1.8f)) {
      player.pose.position=*spot; player.stopFlying(); ride.active=false; world.farm.car.speed=0; return true;
    }
  }
  world.farm.car.speed=0; return false;
}
bool collectMilk(World& world,const Player& player,std::size_t index) {
  auto target=targetRanch(world,player);
  if(!target || target->car || target->index!=index || index>=world.farm.livestock.size()) return false;
  auto& animal=world.farm.livestock[index];
  if(animal.kind!=LivestockKind::Cow || animal.milkTimer>0 || world.farm.milk>=9999) return false;
  ++world.farm.milk; animal.milkTimer=milkSeconds; return true;
}
bool petLivestock(World& world,const Player& player,std::size_t index) {
  auto target=targetRanch(world,player);
  if(!target || target->car || target->index!=index || index>=world.farm.livestock.size()) return false;
  auto& a=world.farm.livestock[index];
  if(a.kind!=LivestockKind::Sheep && a.kind!=LivestockKind::Fox) return false;
  a.happy=2.5f; a.think=1.2f;
  auto delta=player.pose.position-a.position; a.yaw=std::atan2(delta.x,-delta.z); return true;
}
std::string ranchPrompt(const World& world,RanchTarget target) {
  if(target.car) return std::string(garageCars()[world.cityLife.activeCar].name)+" / V to drive";
  if(target.index>=world.farm.livestock.size()) return {};
  const auto& animal=world.farm.livestock[target.index];
  if(animal.kind==LivestockKind::Horse) return "Horse / V to ride";
  if(animal.kind==LivestockKind::Sheep) return "Sheep / V to pet";
  if(animal.kind==LivestockKind::Fox) return "Friendly fox / V to pet";
  if(world.farm.milk>=9999) return "Milk basket full / P / Shop / Sell basket";
  if(animal.milkTimer>0) return "Cow / Milk ready in "+std::to_string(int(std::ceil(animal.milkTimer)))+" seconds";
  return "Cow / V to collect milk";
}
void tickRanch(World& world,Player& player,RideState& ride,Movement movement,float dt) {
  if(!std::isfinite(dt) || dt<=0) return;
  dt=std::min(dt,.1f);
  if(ride.active) {
    auto& position=ride.car ? world.farm.car.position : world.farm.livestock[ride.index].position;
    auto& yaw=ride.car ? world.farm.car.yaw : world.farm.livestock[ride.index].yaw;
    float throttle=std::clamp(movement.forward,-1.f,1.f);
    float turnInput=std::clamp(movement.right,-1.f,1.f);
    float speed=throttle*6.f;
    if(ride.car) {
      speed=world.farm.car.speed;
      world.farm.car.boosting=movement.boost && throttle>0 && !movement.jump;
      float top=world.farm.car.boosting ? carBoostSpeed : carTopSpeed;
      float target=throttle>=0 ? throttle*top : throttle*8.f;
      // Lift slightly in tight turns. Steering is gentler at high speed, and
      // S first brakes forward motion before engaging reverse.
      if(target>0) target=std::min(target,std::lerp(top,18.f,std::abs(turnInput)));
      float rate=throttle==0 ? 14.f : speed*throttle<0 || std::abs(target)<std::abs(speed) ? 32.f : world.farm.car.boosting ? 24.f : 12.f;
      speed+=std::clamp(target-speed,-rate*dt,rate*dt);
    } else if(throttle<0) speed*=.45f;
    if(movement.jump) speed=0;
    float turnRate=ride.car ? std::lerp(1.7f,.65f,std::clamp(std::abs(speed)/carTopSpeed,0.f,1.f)) : 1.6f;
    float steering=turnInput*turnRate*dt;
    const int turns=std::max(1,int(std::ceil(std::abs(steering)/.025f)));
    for(int turn=0;turn<turns;++turn) {
      float candidate=std::remainder(yaw+steering/turns,2*pi);
      if(ride.car) {
        auto floor=carGround(world,position,candidate,true);
        if(!floor || std::abs(floor->y-position.y)>1.01f) break;
        position=*floor;
      }
      yaw=candidate; player.pose.yaw=std::remainder(player.pose.yaw+steering/turns,2*pi);
    }
    glm::vec3 direction{std::sin(yaw),0,-std::cos(yaw)};
    const int steps=std::max(1,int(std::ceil(std::abs(speed)*dt/.08f)));
    float travelled=0;
    for(int step=0;step<steps;++step) {
      auto delta=direction*speed*dt/float(steps);
      auto next=ride.car ? carGround(world,position+delta,yaw,true)
        : ground(world,position+delta,1.12f,2.8f,int(ride.index));
      // Grazing a trunk should slide along its side, not freeze all movement.
      if(ride.car && !next) for(int axis : {std::abs(delta.x)>std::abs(delta.z) ? 0 : 2,std::abs(delta.x)>std::abs(delta.z) ? 2 : 0}) {
        if(std::abs(delta[axis])<.00001f) continue;
        auto at=position; at[axis]+=delta[axis]; next=carGround(world,at,yaw,true);
        if(next) break;
      }
      if(!next || std::abs(next->y-position.y)>1.01f) { speed=0; break; }
      travelled+=glm::length(glm::vec2(next->x-position.x,next->z-position.z));
      position=*next;
    }
    if(ride.car) world.farm.car.speed=std::copysign(travelled/dt,speed);
    else { auto& horse=world.farm.livestock[ride.index]; horse.home=position; horse.moving=speed!=0; horse.walk+=std::abs(speed)*dt*4; }
    player.stopFlying(); player.grounded=false; player.pose.position=seat(world,ride);
  }
  if(world.farm.car.owned && !(ride.active && ride.car)) {
    auto& car=world.farm.car; car.speed=0; car.boosting=false;
    if(auto floor=carGround(world,car.position,car.yaw,false,worldHeight); floor && floor->y<car.position.y)
      car.position.y=std::max(floor->y,car.position.y-5.f*dt);
  }
  for(std::size_t i=0;i<world.farm.livestock.size();++i) {
    if(ride.active && !ride.car && ride.index==i) continue;
    auto& animal=world.farm.livestock[i]; animal.moving=false;
    animal.happy=std::max(0.f,animal.happy-dt);
    float radius=animalRadius(animal.kind),height=animalHeight(animal.kind);
    if(!world.chunks.contains(chunkAt(int(std::floor(animal.position.x)),int(std::floor(animal.position.z))))) continue;
    // If terrain was dug away, settle onto a clear floor instead of hovering.
    if(auto floor=ground(world,animal.position,radius,height,int(i),false,worldHeight); floor && floor->y<animal.position.y)
      animal.position.y=std::max(floor->y,animal.position.y-5.f*dt);
    if(world.clock.sky().daylight<.12f || animal.happy>0) continue;
    animal.think-=dt;
    if(animal.think<=0) {
      animal.heading=animal.yaw+std::sin(float(i)*3.1f+float(world.clock.phase)*900)*1.8f;
      animal.think=3.f;
    }
    if(animal.think<1.3f) continue;
    auto home=animal.home-animal.position; home.y=0;
    float angle=glm::length(home)>5.f ? std::atan2(home.x,-home.z) : animal.heading;
    float turn=std::clamp(std::remainder(angle-animal.yaw,2*pi),-dt,dt);
    float yaw=std::remainder(animal.yaw+turn,2*pi);
    auto wanted=animal.position+glm::vec3(std::sin(yaw),0,-std::cos(yaw))*(animal.kind==LivestockKind::Fox ? .8f : .45f)*dt;
    if(auto next=ground(world,wanted,radius,height,int(i)); next && std::abs(next->y-animal.position.y)<1.01f
        && !overlaps(body(*next,radius,height),body(player.pose.position,.3f,1.8f))) {
      animal.position=*next; animal.yaw=yaw; animal.walk+=dt*3; animal.moving=true;
    } else animal.think=1.f;
  }
}
std::vector<Vertex> ranchMesh(const World& world) {
  std::vector<Vertex> vertices;
  auto shape=[&](glm::vec3 position,float yaw,int id,glm::vec3 lo,glm::vec3 hi,float material) {
    auto first=vertices.size(); appendBox(vertices,{lo,hi},{-100003,0,id},material,1);
    constexpr std::array shade{.82f,.68f,1.f,.60f,.87f,.76f};
    for(auto j=first;j<vertices.size();++j) {
      auto p=vertices[j].position;
      vertices[j].position=position+glm::vec3(p.x*std::cos(yaw)-p.z*std::sin(yaw),p.y,p.x*std::sin(yaw)+p.z*std::cos(yaw));
      vertices[j].light=shade[(j-first)/6];
    }
  };
  for(std::size_t i=0;i<world.farm.livestock.size();++i) {
    const auto& a=world.farm.livestock[i];
    if(!world.chunks.contains(chunkAt(int(std::floor(a.position.x)),int(std::floor(a.position.z))))) continue;
    auto box=[&](glm::vec3 lo,glm::vec3 hi,float m){shape(a.position,a.yaw,int(i),lo,hi,m);};
    if(a.kind==LivestockKind::Sheep || a.kind==LivestockKind::Fox) {
      bool fox=a.kind==LivestockKind::Fox; float coat=fox ? 27.f : 26.f;
      float hip=fox ? .48f : .8f;
      box({fox ? -.24f : -.43f,fox ? .3f : .48f,-.46f},{fox ? .24f : .43f,fox ? .7f : 1.16f,.5f},coat);
      for(float x : {-.22f,.22f}) for(float z : {-.32f,.34f}) {
        float step=a.moving ? std::sin(a.walk+(x*z>0 ? 0 : pi))*.1f : 0;
        box({x-.065f,.08f,z-.065f+step},{x+.065f,hip,z+.065f+step},fox ? 27 : 33);
        box({x-.07f,0,z-.07f+step},{x+.07f,.18f,z+.07f+step},29);
      }
      if(fox) {
        box({-.22f,.48f,-.66f},{.22f,.83f,-.28f},27);
        box({-.18f,.44f,-.73f},{.18f,.64f,-.48f},26);
        box({-.10f,.48f,-.85f},{.10f,.60f,-.69f},26);
        box({-.065f,.52f,-.89f},{.065f,.60f,-.82f},29);
        box({-.18f,.3f,-.44f},{.18f,.53f,-.25f},26);
        float wag=a.happy>0 ? std::sin(a.happy*18)*.14f : 0;
        box({-.14f+wag,.38f,.43f},{.14f+wag,.66f,.98f},27);
        box({-.13f+wag,.40f,.96f},{.13f+wag,.64f,1.22f},26);
        for(float x : {-.15f,.15f}) {
          box({x-.075f,.78f,-.57f},{x+.075f,1.f,-.39f},27);
          box({x-.045f,.85f,-.58f},{x+.045f,.97f,-.56f},29);
          box({x-.035f,.68f,-.672f},{x+.035f,.745f,-.66f},29);
        }
      } else {
        for(float x : {-.27f,0.f,.27f}) for(float z : {-.30f,0.f,.30f})
          box({x-.16f,1.05f,z-.16f},{x+.16f,1.27f,z+.16f},26);
        box({-.24f,.82f,-.76f},{.24f,1.23f,-.4f},33);
        box({-.25f,1.16f,-.68f},{.25f,1.4f,-.38f},26);
        for(float x : {-.18f,.18f}) {
          box({x-.035f,1.01f,-.775f},{x+.035f,1.085f,-.758f},29);
          float ear=x<0 ? -.34f : .24f;
          box({ear,1.02f,-.56f},{ear+.1f,1.12f,-.37f},33);
        }
        box({-.10f,.65f,.48f},{.10f,.92f,.72f},26);
      }
      continue;
    }
    bool horse=a.kind==LivestockKind::Horse; float fur=horse ? 30.f : 26.f;
    box({-.42f,.7f,-.72f},{.42f,1.5f,.72f},fur);
    for(float x : {-.29f,.29f}) for(float z : {-.53f,.53f}) {
      float stride=a.moving ? std::sin(a.walk+(x*z>0 ? 0 : pi))*.13f : 0;
      box({x-.095f,.12f,z-.1f+stride},{x+.095f,.85f,z+.1f+stride},fur);
      box({x-.11f,0,z-.12f+stride},{x+.11f,.18f,z+.12f+stride},29);
    }
    if(horse) {
      box({-.22f,1.2f,-.82f},{.22f,2.f,-.47f},fur);
      box({-.23f,1.75f,-1.1f},{.23f,2.12f,-.6f},fur);
      box({-.24f,1.65f,-1.13f},{.24f,1.87f,-.91f},33);
      box({-.075f,1.45f,-.58f},{.075f,2.11f,-.42f},29);
      box({-.1f,.42f,.7f},{.1f,1.45f,.94f},29);
      box({-.35f,1.5f,-.18f},{.35f,1.58f,.35f},33);
      box({-.08f,1.56f,-.23f},{.08f,1.72f,-.10f},33);
    } else {
      box({-.32f,1.05f,-1.02f},{.32f,1.7f,-.55f},26);
      box({-.33f,1.04f,-1.12f},{.33f,1.27f,-.98f},32);
      box({-.43f,.87f,-.45f},{-.419f,1.4f,.05f},29);
      box({.419f,.95f,.1f},{.43f,1.45f,.54f},29);
      box({-.12f,1.5f,.1f},{.38f,1.52f,.48f},29);
      box({-.16f,.55f,.1f},{.16f,.74f,.4f},32);
      box({-.04f,.55f,.71f},{.04f,1.37f,.81f},26);
      box({-.065f,.46f,.73f},{.065f,.65f,.86f},29);
    }
    float eyeY=horse ? 1.99f : 1.5f,earY=horse ? 2.1f : 1.62f;
    for(float side : {-1.f,1.f}) {
      float x=side*(horse ? .235f : .325f);
      box({x-.012f,eyeY,-.9f},{x+.012f,eyeY+.07f,-.80f},29);
      float ear=side*.22f;
      box({ear-.06f,earY,-.72f},{ear+.06f,earY+(horse ? .16f : .12f),-.58f},fur);
    }
  }
  std::vector<std::pair<FarmCar,int>> vehicles;
  if(world.farm.car.owned)vehicles.push_back({world.farm.car,world.cityLife.activeCar});
  if(world.metroOrigin)for(int i=0;i<garageSize;++i)if(i!=world.cityLife.activeCar) {
    auto car=parkedCar(world,i);
    if(world.chunks.contains(chunkAt(int(car.position.x),int(car.position.z))))vehicles.push_back({car,i});
  }
  for(const auto& [c,model]:vehicles) {
    auto box=[&](glm::vec3 lo,glm::vec3 hi,float m){shape(c.position,c.yaw,99,lo,hi,m);};
    constexpr float paint=74,carbon=75,glass=76,alloy=77,headlamp=78,tail=79;
    auto quad=[&](glm::vec3 a,glm::vec3 b,glm::vec3 d,glm::vec3 e,float material,float light) {
      std::array<glm::vec3,4> ps{a,b,d,e};
      constexpr std::array<glm::vec2,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
      for(int i:{0,1,2,0,2,3}) {
        auto p=ps[i]; auto offset=carOffset(c.yaw,p.x,p.z); offset.y=p.y;
        vertices.push_back({c.position+offset,uv[i],material,light,{-100003,0,99}});
      }
    };
    if(model>0) {
      int body=garageCars()[model].body;float color=80.f+model;
      box({-.76f,.28f,-2.12f},{.76f,.42f,2.10f},carbon);
      box({-.85f,.4f,-1.92f},{.85f,.76f,1.98f},color);
      if(body==4) {
        quad({-.82f,.5f,-2.16f},{-.83f,.81f,-.65f},{.83f,.81f,-.65f},{.82f,.5f,-2.16f},color,1);
        box({-.82f,.42f,-2.16f},{.82f,.52f,-1.9f},color);
      } else box({-.76f,.5f,-2.12f},{.76f,.71f,-1.86f},color);
      float front=body==3 ? -1.03f : -.55f,back=body==2 || body==3 ? 1.34f : .64f;
      quad({-.70f,.78f,front-.38f},{-.58f,1.36f,front},{.58f,1.36f,front},{.70f,.78f,front-.38f},glass,1);
      if(body==1) {
        box({-.61f,.77f,-.25f},{.61f,.82f,.91f},carbon);
        for(float x:{-.31f,.31f})box({x-.19f,.82f,.34f},{x+.19f,1.17f,.54f},carbon);
        box({-.63f,1.1f,.68f},{.63f,1.23f,.76f},alloy);
      } else {
        box({-.59f,1.34f,front},{.59f,1.43f,back},color);
        box({-.66f,.8f,front},{.66f,1.34f,back},glass);
        for(float x:{-.69f,.61f})box({x,.8f,front},{x+.08f,1.4f,front+.10f},color);
        quad({.66f,.81f,back+.34f},{.59f,1.34f,back},{-.59f,1.34f,back},{-.66f,.81f,back+.34f},glass,.8f);
      }
      for(float x:{-.79f,.69f})box({x,.46f,-1.30f},{x+.10f,.72f,1.25f},color);
      for(float x:{-.92f,.80f})box({x,.93f,front-.10f},{x+.12f,1.05f,front+.14f},color);
      for(float x:{-.65f,.38f}) {
        box({x,.55f,-2.14f},{x+.27f,.65f,-2.10f},headlamp);
        box({x,.55f,1.98f},{x+.27f,.68f,2.03f},tail);
      }
      box({-.35f,.42f,-2.145f},{.35f,.51f,-2.11f},carbon);
      if(body==5) {
        box({-.83f,.36f,-2.20f},{.83f,.44f,-2.10f},alloy);
        box({-.83f,.36f,2.05f},{.83f,.44f,2.17f},alloy);
      }
      if(body==3 || body==4) {
        for(float x:{-.53f,.46f})box({x,.76f,1.55f},{x+.07f,1.18f,1.67f},carbon);
        box({-.89f,1.15f,1.44f},{.89f,1.24f,1.86f},body==3 ? color : carbon);
        for(float x:{-.09f,.08f})box({x,.77f,-1.8f},{x+.07f,.79f,-.95f},alloy);
      }
      for(float x:{-.81f,.81f})for(float z:{-1.32f,1.32f}) {
        float outer=x<0 ? -.945f : .945f,inner=x<0 ? -.70f : .70f;
        for(int i=0;i<12;++i) {
          float a=2*pi*i/12,b=2*pi*(i+1)/12;
          auto point=[&](float xx,float r,float t){return glm::vec3(xx,.38f+r*std::cos(t),z+r*std::sin(t));};
          quad(point(inner,.36f,a),point(outer,.36f,a),point(outer,.36f,b),point(inner,.36f,b),carbon,.8f);
          auto face=[&](float r,float m){glm::vec3 center{outer,.38f,z};if(x<0)quad(center,point(outer,r,b),point(outer,r,a),center,m,.9f);else quad(center,point(outer,r,a),point(outer,r,b),center,m,.9f);};
          face(.35f,carbon);face(.24f,body==3 ? 71.f : alloy);
        }
      }
      continue;
    }
    // Low, wide 911 body: tapered nose, raised front wings, sloping roof and
    // broad rear haunches. Sections keep a faceted voxel-game silhouette.
    struct Section {float z,width,top;};
    constexpr std::array<Section,8> body{{
      {-2.18f,.71f,.60f},{-1.88f,.84f,.78f},{-1.35f,.88f,.91f},
      {-.68f,.83f,.83f},{.58f,.86f,.87f},{1.32f,.91f,.96f},{1.92f,.87f,.84f},{2.12f,.75f,.72f}}};
    box({-.72f,.26f,-2.16f},{.72f,.42f,2.12f},carbon);
    for(std::size_t i=1;i<body.size();++i) {
      auto a=body[i-1],b=body[i];
      quad({-a.width,.40f,a.z},{-b.width,.40f,b.z},{-b.width,b.top,b.z},{-a.width,a.top,a.z},paint,.82f);
      quad({a.width,a.top,a.z},{b.width,b.top,b.z},{b.width,.40f,b.z},{a.width,.40f,a.z},paint,.75f);
      float ay=a.top-(a.z<-.68f ? .10f : 0),by=b.top-(b.z<-.68f ? .10f : 0);
      quad({-.49f,ay,a.z},{-.49f,by,b.z},{.49f,by,b.z},{.49f,ay,a.z},paint,1);
      quad({-a.width,a.top,a.z},{-b.width,b.top,b.z},{-.49f,by,b.z},{-.49f,ay,a.z},paint,.96f);
      quad({.49f,ay,a.z},{.49f,by,b.z},{b.width,b.top,b.z},{a.width,a.top,a.z},paint,.94f);
    }
    quad({-.71f,.4f,-2.18f},{-.71f,.60f,-2.18f},{.71f,.60f,-2.18f},{.71f,.4f,-2.18f},paint,.9f);
    quad({.75f,.4f,2.12f},{.75f,.72f,2.12f},{-.75f,.72f,2.12f},{-.75f,.4f,2.12f},paint,.74f);
    box({-.84f,.24f,-2.23f},{.84f,.31f,-1.88f},carbon); // Front splitter.
    for(float side:{-1.f,1.f}) {
      float lo=side<0 ? -.91f : .78f,hi=side<0 ? -.78f : .91f;
      box({lo,.26f,-1.35f},{hi,.37f,1.4f},carbon);
      float x=side*.81f;
      box({x-.06f,.61f,.57f},{x+.06f,.79f,1.02f},carbon); // Rear brake intakes.
      box({side<0 ? -.82f : .70f,.96f,-.57f},{side<0 ? -.70f : .82f,1.03f,-.48f},carbon);
      box({side<0 ? -.95f : .79f,1.02f,-.67f},{side<0 ? -.79f : .95f,1.12f,-.44f},paint);
    }
    // Dark windscreen and side glass, with a red roof and C-pillars.
    quad({-.66f,.86f,-.94f},{-.55f,1.36f,-.27f},{.55f,1.36f,-.27f},{.66f,.86f,-.94f},glass,.95f);
    quad({-.55f,1.36f,-.27f},{-.57f,1.43f,.02f},{.57f,1.43f,.02f},{.55f,1.36f,-.27f},paint,1);
    quad({-.57f,1.43f,.02f},{-.57f,1.40f,.37f},{.57f,1.40f,.37f},{.57f,1.43f,.02f},paint,1);
    quad({-.57f,1.40f,.37f},{-.64f,1.16f,.87f},{.64f,1.16f,.87f},{.57f,1.40f,.37f},glass,.93f);
    quad({-.64f,1.16f,.87f},{-.73f,.91f,1.28f},{.73f,.91f,1.28f},{.64f,1.16f,.87f},paint,1);
    for(float side:{-1.f,1.f}) {
      auto sideQuad=[&](glm::vec3 a,glm::vec3 b,glm::vec3 d,glm::vec3 e,float m) {
        a.x*=side;b.x*=side;d.x*=side;e.x*=side;
        if(side<0)quad(e,d,b,a,m,.82f);else quad(a,b,d,e,m,.82f);
      };
      sideQuad({.67f,.87f,-.9f},{.57f,1.33f,-.25f},{.59f,1.35f,.35f},{.74f,.91f,.86f},glass);
      sideQuad({.59f,1.35f,.35f},{.64f,1.16f,.87f},{.73f,.91f,1.28f},{.74f,.91f,.86f},paint);
      sideQuad({.68f,.88f,-.98f},{.56f,1.38f,-.3f},{.57f,1.33f,-.20f},{.67f,.87f,-.83f},paint);
      sideQuad({.57f,1.33f,-.25f},{.57f,1.43f,.02f},{.59f,1.35f,.35f},{.59f,1.31f,.35f},paint);
      box({side<0 ? -.84f : .81f,.72f,-.12f},{side<0 ? -.81f : .84f,.77f,.10f},carbon);
    }
    // Faceted tyres, rotating five-spoke alloys and yellow brake calipers.
    for(float x:{-.81f,.81f}) for(float z:{-1.35f,1.34f}) {
      float outside=x<0 ? -.947f : .947f,inside=x<0 ? -.69f : .69f;
      float spin=(c.position.x*std::sin(c.yaw)-c.position.z*std::cos(c.yaw))/.36f;
      for(int i=0;i<12;++i) {
        float a=2*pi*i/12,b=2*pi*(i+1)/12;
        auto rim=[&](float xx,float r,float angle){return glm::vec3(xx,.38f+r*std::cos(angle),z+r*std::sin(angle));};
        quad(rim(inside,.36f,a),rim(outside,.36f,a),rim(outside,.36f,b),rim(inside,.36f,b),carbon,.8f);
        // Both windings cover left and right wheels without backface holes.
        auto disc=[&](float r,float mat){auto center=glm::vec3(outside,.38f,z);
          if(x<0)quad(center,rim(outside,r,b),rim(outside,r,a),center,mat,.9f);
          else quad(center,rim(outside,r,a),rim(outside,r,b),center,mat,.9f);};
        disc(.35f,carbon); // Visible sidewall.
      }
      float xx=outside+(x<0 ? -.001f : .001f);
      auto wheelQuad=[&](glm::vec3 a,glm::vec3 b,glm::vec3 d,glm::vec3 e,float material) {
        if(x<0)quad(e,d,b,a,material,.98f);else quad(a,b,d,e,material,.98f);
      };
      for(int i=0;i<12;++i) {
        float a=2*pi*i/12,b=2*pi*(i+1)/12;
        auto p=[&](float r,float angle){return glm::vec3(xx,.38f+r*std::cos(angle),z+r*std::sin(angle));};
        wheelQuad(p(.28f,a),p(.30f,a),p(.30f,b),p(.28f,b),alloy);
      }
      wheelQuad({xx,.25f,z+.11f},{xx,.25f,z+.22f},{xx,.5f,z+.22f},{xx,.5f,z+.11f},71);
      for(int i=0;i<5;++i) {
        float a=spin+2*pi*i/5;
        auto p=[&](float r,float angle){return glm::vec3(xx,.38f+r*std::cos(angle),z+r*std::sin(angle));};
        wheelQuad(p(.03f,a-.5f),p(.28f,a-.15f),p(.28f,a+.15f),p(.03f,a+.5f),alloy);
      }
    }
    // Oval 911 headlights lie flush along the swept front wings.
    for(float x:{-.61f,.61f}) {
      for(int ring=0;ring<2;++ring) {
        float radius=ring ? .155f : .185f;
        glm::vec3 center{x,.86f+ring*.003f,-1.76f-ring*.003f};
        auto point=[&](float angle){return center+glm::vec3(radius*std::cos(angle),radius*.76f*std::sin(angle),radius*.85f*std::sin(angle));};
        for(int i=0;i<12;++i)quad(center,point(2*pi*(i+1)/12),point(2*pi*i/12),center,ring ? headlamp : carbon,1);
      }
      box({x-.18f,.63f,2.095f},{x+.18f,.74f,2.135f},tail);
      box({x-.21f,.40f,-2.185f},{x+.20f,.52f,-2.16f},carbon);
    }
    box({-.31f,.37f,-2.19f},{.31f,.50f,-2.16f},carbon);
    box({-.24f,.46f,2.125f},{.24f,.59f,2.14f},alloy);
    for(float x:{-.48f,.32f})box({x,.29f,2.06f},{x+.16f,.43f,2.22f},alloy);
    // Tall fixed GT2 rear wing, uprights and endplates.
    for(float x:{-.55f,.47f})box({x,.88f,1.47f},{x+.08f,1.31f,1.68f},carbon);
    box({-.93f,1.27f,1.40f},{.93f,1.37f,1.91f},carbon);
    for(float x:{-.94f,.88f})box({x,1.23f,1.39f},{x+.06f,1.46f,1.92f},paint);
    for(int i=0;i<5;++i)box({-.42f,.975f,1.12f+i*.09f},{.42f,.998f,1.15f+i*.09f},carbon);
  }
  return vertices;
}
} // namespace bw

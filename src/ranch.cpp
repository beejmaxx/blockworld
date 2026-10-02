#include "ranch.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr float pi=3.14159265f;
bool overlaps(Box a,Box b) {
  return a.max.x>b.min.x+.001f && a.min.x<b.max.x-.001f && a.max.y>b.min.y+.001f && a.min.y<b.max.y-.001f
    && a.max.z>b.min.z+.001f && a.min.z<b.max.z-.001f;
}
Box body(glm::vec3 p,float radius,float height) { return {p+glm::vec3(-radius,.02f,-radius),p+glm::vec3(radius,height,radius)}; }
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
  return !skipCar && world.farm.car.owned && overlaps(box,carBounds(world.farm.car));
}
std::optional<glm::vec3> ground(const World& world,glm::vec3 at,float radius,float height,int skip=-1,bool skipCar=false,int drop=2) {
  int top=std::min(worldHeight-2,int(std::floor(at.y+.05f)));
  for(int y=top;y>=std::max(0,top-drop);--y) {
    auto floor=world.get({int(std::floor(at.x)),y,int(std::floor(at.z))});
    if(!opaque(floor) || floor==Block::Leaves || floor==Block::Fence || isGate(floor)) continue;
    glm::vec3 p{at.x,float(y+1),at.z}; auto box=body(p,radius,height);
    if(blocked(world,box) || occupied(world,box,skip,skipCar)) continue;
    // Support every corner so a parked car or animal cannot bridge a deep hole.
    bool supported=true;
    for(float x : {-radius*.8f,radius*.8f}) for(float z : {-radius*.8f,radius*.8f}) {
      auto b=world.get({int(std::floor(p.x+x)),y,int(std::floor(p.z+z))});
      if(!opaque(b) || b==Block::Leaves || b==Block::Fence || isGate(b)) supported=false;
    }
    if(supported) return p;
  }
  return {};
}
std::optional<glm::vec3> delivery(const World& world,const Player& player,bool car) {
  for(float radius : {4.f,6.f,8.f,10.f,12.f}) for(int direction=0;direction<16;++direction) {
    float angle=player.pose.yaw+direction*pi/8;
    auto at=player.pose.position+glm::vec3(std::sin(angle)*radius,1,-std::cos(angle)*radius);
    if(auto spot=ground(world,at,car ? 1.7f : 1.12f,car ? 1.5f : 2.25f,-1,car,worldHeight)) {
      if(!player.collides(world,*spot) && !overlaps(body(player.pose.position,.3f,1.8f),body(*spot,car ? 1.7f : 1.12f,2.25f))) return spot;
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
Box livestockBounds(const Livestock& animal) { return body(animal.position,1.12f,animal.kind==LivestockKind::Horse ? 2.25f : 1.8f); }
Box carBounds(const FarmCar& car) { return body(car.position,1.7f,1.5f); }
bool ranchOverlap(const World& world,Box box) {
  for(const auto& animal : world.farm.livestock) if(overlaps(box,livestockBounds(animal))) return true;
  return world.farm.car.owned && overlaps(box,carBounds(world.farm.car));
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
  if(world.farm.car.owned) if(auto distance=rayBox(player.eye(),player.direction(),carBounds(world.farm.car),nearest)) result=RanchTarget{true,0,*distance};
  return result;
}
std::string buyLivestock(World& world,const Player& player,LivestockKind kind) {
  if(kind!=LivestockKind::Cow && kind!=LivestockKind::Horse) return "Unknown animal";
  if(world.farm.livestock.size()>=livestockLimit) return "Your pasture is full (8 cows and horses)";
  if(world.farm.garden.coins<livestockPrice(kind)) return "Sell crops in the shop to earn more coins";
  auto spot=delivery(world,player,false);
  if(!spot) return "Step outside near a clear, level patch first";
  Livestock animal; animal.kind=kind; animal.position=animal.home=*spot; animal.yaw=player.pose.yaw;
  world.farm.livestock.push_back(animal); world.farm.garden.coins-=livestockPrice(kind); return {};
}
std::string bringCar(World& world,const Player& player) {
  auto spot=delivery(world,player,true);
  if(!spot) return "Step outside near a clear, level patch for your car";
  world.farm.car={true,*spot,player.pose.yaw,0}; return {};
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
std::string ranchPrompt(const World& world,RanchTarget target) {
  if(target.car) return "Farm car / V or right-click to drive";
  if(target.index>=world.farm.livestock.size()) return {};
  const auto& animal=world.farm.livestock[target.index];
  if(animal.kind==LivestockKind::Horse) return "Horse / V or right-click to ride";
  if(world.farm.milk>=9999) return "Milk basket full / P / Shop / Sell basket";
  if(animal.milkTimer>0) return "Cow / Milk ready in "+std::to_string(int(std::ceil(animal.milkTimer)))+" seconds";
  return "Cow / V or right-click to collect milk";
}
void tickRanch(World& world,Player& player,RideState& ride,Movement movement,float dt) {
  if(!std::isfinite(dt) || dt<=0) return;
  dt=std::min(dt,.1f);
  if(ride.active) {
    auto& position=ride.car ? world.farm.car.position : world.farm.livestock[ride.index].position;
    auto& yaw=ride.car ? world.farm.car.yaw : world.farm.livestock[ride.index].yaw;
    float speed=movement.forward*(ride.car ? 9.f : 6.f);
    if(movement.forward<0) speed*=.45f;
    if(movement.jump) speed=0;
    float steering=movement.right*1.6f*dt;
    yaw=std::remainder(yaw+steering,2*pi);
    player.pose.yaw=std::remainder(player.pose.yaw+steering,2*pi);
    glm::vec3 direction{std::sin(yaw),0,-std::cos(yaw)};
    const int steps=std::max(1,int(std::ceil(std::abs(speed)*dt/.08f)));
    for(int step=0;step<steps;++step) {
      auto next=ground(world,position+direction*speed*dt/float(steps),ride.car ? 1.7f : 1.12f,ride.car ? 2.2f : 2.8f,
        ride.car ? -1 : int(ride.index),ride.car);
      if(!next || std::abs(next->y-position.y)>1.01f) { speed=0; break; }
      position=*next;
    }
    if(ride.car) world.farm.car.speed=speed;
    else { auto& horse=world.farm.livestock[ride.index]; horse.home=position; horse.moving=speed!=0; horse.walk+=std::abs(speed)*dt*4; }
    player.stopFlying(); player.grounded=false; player.pose.position=seat(world,ride);
  }
  if(world.farm.car.owned && !(ride.active && ride.car)) {
    auto& car=world.farm.car; car.speed=0;
    if(auto floor=ground(world,car.position,1.7f,1.5f,-1,true,worldHeight); floor && floor->y<car.position.y)
      car.position.y=std::max(floor->y,car.position.y-5.f*dt);
  }
  for(std::size_t i=0;i<world.farm.livestock.size();++i) {
    if(ride.active && !ride.car && ride.index==i) continue;
    auto& animal=world.farm.livestock[i]; animal.moving=false;
    if(!world.chunks.contains(chunkAt(int(std::floor(animal.position.x)),int(std::floor(animal.position.z))))) continue;
    // If terrain was dug away, settle onto a clear floor instead of hovering.
    if(auto floor=ground(world,animal.position,1.12f,2.25f,int(i),false,worldHeight); floor && floor->y<animal.position.y)
      animal.position.y=std::max(floor->y,animal.position.y-5.f*dt);
    if(world.clock.sky().daylight<.12f) continue;
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
    auto wanted=animal.position+glm::vec3(std::sin(yaw),0,-std::cos(yaw))*.45f*dt;
    if(auto next=ground(world,wanted,1.12f,2.25f,int(i)); next && std::abs(next->y-animal.position.y)<1.01f
        && !overlaps(body(*next,1.12f,2.25f),body(player.pose.position,.3f,1.8f))) {
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
  if(world.farm.car.owned) {
    const auto& c=world.farm.car;
    auto box=[&](glm::vec3 lo,glm::vec3 hi,float m){shape(c.position,c.yaw,99,lo,hi,m);};
    box({-.78f,.36f,-1.25f},{.78f,.72f,1.25f},42);
    box({-.73f,.7f,-1.23f},{.73f,1.05f,-.48f},42);
    box({-.78f,.72f,.62f},{.78f,1.f,1.22f},42);
    for(float x : {-.78f,.65f}) box({x,.7f,-.4f},{x+.13f,1.f,.65f},42);
    for(float x : {-.46f,.2f}) {
      box({x,.73f,.0f},{x+.27f,.86f,.54f},33);
      box({x,.82f,.44f},{x+.27f,1.39f,.6f},33);
    }
    box({-.3f,1.12f,-.46f},{.3f,1.18f,-.38f},29);
    for(float x : {-.94f,.7f}) for(float z : {-.93f,.63f}) {
      box({x,.02f,z-.23f},{x+.24f,.57f,z+.23f},29);
      box({x-.005f,.19f,z-.1f},{x+.245f,.4f,z+.1f},3);
    }
    for(float x : {-.61f,.38f}) {
      box({x,.72f,-1.27f},{x+.23f,.9f,-1.24f},31);
      box({x,.73f,1.245f},{x+.23f,.87f,1.27f},28);
    }
    box({-.8f,.38f,-1.36f},{.8f,.51f,-1.25f},3);
    box({-.8f,.38f,1.25f},{.8f,.51f,1.36f},3);
  }
  return vertices;
}
} // namespace bw

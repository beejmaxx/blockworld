#include "player.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
constexpr float halfWidth=.3f, height=1.8f, epsilon=.0001f;
bool intersects(glm::vec3 p,Box box) {
  return p.x+halfWidth>box.min.x+epsilon && p.x-halfWidth<box.max.x-epsilon &&
    p.y+height>box.min.y+epsilon && p.y<box.max.y-epsilon && p.z+halfWidth>box.min.z+epsilon && p.z-halfWidth<box.max.z-epsilon;
}
}
glm::vec3 Player::direction() const {
  return {std::sin(pose.yaw)*std::cos(pose.pitch),std::sin(pose.pitch),-std::cos(pose.yaw)*std::cos(pose.pitch)};
}
void Player::look(float dx,float dy) {
  pose.yaw = std::remainder(pose.yaw+dx*.0024f, 6.2831853f);
  pose.pitch = std::clamp(pose.pitch-dy*.0024f,-1.55f,1.55f);
}
bool Player::overlaps(Cell c,Block block) const {
  return collidable(block) && intersects(pose.position,blockBounds(c,block));
}
bool Player::collides(const World& world,glm::vec3 p) const {
  for (int y=int(std::floor(p.y+epsilon));y<=int(std::floor(p.y+height-epsilon));++y)
    for (int z=int(std::floor(p.z-halfWidth+epsilon));z<=int(std::floor(p.z+halfWidth-epsilon));++z)
      for (int x=int(std::floor(p.x-halfWidth+epsilon));x<=int(std::floor(p.x+halfWidth-epsilon));++x)
        if (auto b=world.get({x,y,z}); collidable(b) && intersects(p,blockBounds({x,y,z},b))) return true;
  return false;
}
void Player::tick(const World& world,Movement move,float dt) {
  dt=std::clamp(dt,0.f,.1f);
  sneaking=move.sneak && !pose.flying;
  // Bounded substeps prevent walking or falling through a one-block wall.
  // A counted loop avoids a tiny floating-point remainder that could clear grounded.
  if(dt==0.f) return;
  const int steps=std::max(1,int(std::ceil(dt*120.f)));
  for(int i=0;i<steps;++i) { step(world,move,dt/float(steps)); move.jump=false; }
}
void Player::step(const World& world,Movement move,float dt) {
  glm::vec3 forward{std::sin(pose.yaw),0,-std::cos(pose.yaw)}, right{std::cos(pose.yaw),0,std::sin(pose.yaw)};
  glm::vec3 wanted=forward*move.forward+right*move.right;
  if (glm::length(wanted)>1.f) wanted=glm::normalize(wanted);
  Cell water{int(std::floor(pose.position.x)),int(std::floor(pose.position.y+.6f)),int(std::floor(pose.position.z))};
  bool swimming=!pose.flying && world.get(water)==Block::Water;
  float speed=pose.flying ? (move.sprint ? 18.f : 10.f) : sneaking ? 1.4f : (move.sprint ? 7.8f : 4.6f);
  if(swimming) speed=3.2f;
  velocity.x=wanted.x*speed; velocity.z=wanted.z*speed;
  if (pose.flying) velocity.y=move.vertical*speed;
  else if(swimming) {
    while(water.y<worldHeight && world.get(water)==Block::Water) ++water.y;
    velocity.y=move.jump ? 4.f : std::clamp((float(water.y)-.9f-pose.position.y)*4.f,-3.f,3.f);
  }
  else {
    if (move.jump && grounded) velocity.y=8.f;
    velocity.y=std::max(velocity.y-24.f*dt,-35.f);
  }
  bool protectEdge=sneaking && grounded && velocity.y<=0;
  bool canStep=(grounded || swimming) && !pose.flying && velocity.y<=.1f;
  grounded=false;
  for (int axis : {0,2,1}) {
    float amount=velocity[axis]*dt;
    if (amount==0.f) continue;
    glm::vec3 next=pose.position; next[axis]+=amount;
    auto blocked=[&](glm::vec3 p) {
      return collides(world,p) || (protectEdge && axis!=1 && !collides(world,p-glm::vec3(0,.5f,0)));
    };
    if (blocked(next)) {
      // A half-block stair can be walked up, but full blocks still need a jump.
      // Check headroom at both ends before raising the player's collision box.
      if(axis!=1 && canStep && collides(world,next)) {
        constexpr float stepHeight=.501f;
        auto raised=next+glm::vec3(0,stepHeight,0);
        if(!collides(world,pose.position+glm::vec3(0,stepHeight,0)) && !collides(world,raised)
            && collides(world,raised-glm::vec3(0,stepHeight+.01f,0))) {
          float lo=0,hi=stepHeight;
          for(int i=0;i<12;++i) {
            float mid=(lo+hi)*.5f;
            if(collides(world,next+glm::vec3(0,mid,0))) lo=mid; else hi=mid;
          }
          pose.position=next+glm::vec3(0,hi,0);
          continue;
        }
      }
      float lo=0.f,hi=1.f;
      for (int i=0;i<12;++i) {
        float mid=(lo+hi)*.5f; next=pose.position; next[axis]+=amount*mid;
        if (blocked(next)) hi=mid; else lo=mid;
      }
      pose.position[axis]+=amount*lo;
      if (axis==1 && amount<0.f) grounded=true;
      velocity[axis]=0;
    } else pose.position=next;
  }
  pose.position.x=std::clamp(pose.position.x,-float(coordinateLimit)+2,float(coordinateLimit)-2);
  pose.position.z=std::clamp(pose.position.z,-float(coordinateLimit)+2,float(coordinateLimit)-2);
  pose.position.y=std::min(pose.position.y,256.f);
  if(pose.flying && grounded) pose.flying=false;
}
} // namespace bw

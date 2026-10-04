#pragma once
#include "world.hpp"

namespace bw {
struct Movement {
  float forward{}, right{}, vertical{};
  bool jump{}, sprint{}, sneak{},boost{};
};
class Player {
public:
  PlayerPose pose;
  glm::vec3 velocity{};
  bool grounded = false;
  bool sneaking = false;
  glm::vec3 eye() const { return pose.position + glm::vec3(0,sneaking ? 1.42f : 1.62f,0); }
  glm::vec3 direction() const;
  void look(float dx, float dy);
  void stopFlying() { noclip_=false; pose.flying=false; velocity={}; }
  void toggleFlying() { if(!noclip_)pose.flying=!pose.flying; velocity={}; }
  bool noclip() const { return noclip_; }
  bool toggleNoclip(const World& world);
  PlayerPose savePose(const World& world) const;
  void tick(const World& world, Movement movement, float dt);
  bool overlaps(Cell c, Block block=Block::Stone) const;
  bool collides(const World& world, glm::vec3 at) const;
private:
  bool noclip_=false;
  std::optional<PlayerPose> lastClear_;
  void step(const World& world, Movement movement, float dt);
};
} // namespace bw

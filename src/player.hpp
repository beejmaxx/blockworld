#pragma once
#include "world.hpp"

namespace bw {
struct Movement {
  float forward{}, right{}, vertical{};
  bool jump{}, sprint{}, sneak{};
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
  void stopFlying() { pose.flying=false; velocity={}; }
  void toggleFlying() { pose.flying=!pose.flying; velocity={}; }
  void tick(const World& world, Movement movement, float dt);
  bool overlaps(Cell c, Block block=Block::Stone) const;
  bool collides(const World& world, glm::vec3 at) const;
private:
  void step(const World& world, Movement movement, float dt);
};
} // namespace bw

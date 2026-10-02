#pragma once
#include "adventure.hpp"

namespace bw {
struct PlacementPreview { Cell cell; Block block; PlacementStatus status; std::optional<Box> volume={}; };
std::optional<PlacementPreview> placementPreview(const World& world,const Player& player,const std::optional<RayHit>& hit,Block block);
std::optional<RayHit> miningTarget(const World& world,const Player& player);
struct BreakEvent { Cell cell; Block block; };
class BuildRepeater {
public:
  void press() { held_=true; queued_=true; delay_=.35f; }
  void release() { held_=false; }
  void cancel() { held_=false; queued_=false; delay_=0; }
  bool tick(float dt,bool active);
private:
  bool held_=false,queued_=false;
  float delay_=0;
};
class Mining {
public:
  std::optional<BreakEvent> tick(World& world,const std::optional<RayHit>& hit,bool held,float dt,Tool equipped=Tool::Hands);
  void reset() { cell_.reset(); elapsed_=0; progress_=0; }
  std::optional<Cell> target() const { return cell_; }
  float progress() const { return progress_; }
private:
  std::optional<Cell> cell_;
  Block block_=Block::Air;
  float elapsed_=0,progress_=0;
};
class DebrisCloud {
public:
  static constexpr std::size_t capacity=160;
  void emit(Cell cell,Block block);
  void water(Cell cell);
  void rain(glm::vec3 center);
  void tick(float dt);
  std::vector<Vertex> mesh(glm::vec3 right,glm::vec3 up) const;
  std::size_t size() const { return particles_.size(); }
private:
  struct Particle { glm::vec3 position,velocity; float age,life,size,material; };
  std::vector<Particle> particles_;
  std::uint32_t seed_=0x127ad39;
  float random();
};
} // namespace bw

#include "building.hpp"
#include "farm.hpp"
#include "ranch.hpp"
#include "inventory.hpp"
#include "city_life.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
bool BuildRepeater::tick(float dt,bool active) {
  if(!active) { cancel(); return false; }
  if(queued_) { queued_=false; return true; }
  if(!held_ || !std::isfinite(dt)) return false;
  delay_-=std::clamp(dt,0.f,.1f);
  if(delay_>0) return false;
  delay_=.18f; return true;
}
std::optional<PlacementPreview> placementPreview(const World& world,const Player& player,const std::optional<RayHit>& hit,Block block) {
  if(!hit || !std::isfinite(hit->distance) || hit->distance<0 || hit->distance>7.f) return {};
  bool alongX=std::abs(player.direction().x)>std::abs(player.direction().z);
  auto status=placementStatus(world,player,hit->adjacent,block,alongX);
  if(isDoor(block)) block=doorVariant(alongX,false,false);
  if(isGate(block)) block=gateVariant(alongX,false);
  if(isBed(block)) block=bedVariant(alongX,false);
  return PlacementPreview{hit->adjacent,block,status};
}
std::optional<RayHit> miningTarget(const World& world,const Player& player) {
  // Animals intercept the click: never dig the floor or wall through a chicken.
  if(targetChicken(world,player,7.f) || targetRanch(world,player,7.f) || targetCity(world,player,7.f)) return {};
  return world.raycast(player.eye(),player.direction(),7.f);
}
std::optional<BreakEvent> removeSelectedBlock(World& world,const Player& player,const ToolSelection& tools) {
  if(!tools.removesBlocks()) return {};
  auto hit=miningTarget(world,player);
  if(!hit) return {};
  BreakEvent event{hit->block,world.get(hit->block)};
  if(!breakBlock(world,event.cell)) return {};
  collectMaterial(world,event.block); world.guideFlags|=Broke;
  return event;
}
std::optional<BreakEvent> Mining::tick(World& world,const std::optional<RayHit>& hit,bool held,float dt,Tool equipped) {
  if(!held || !hit || !std::isfinite(hit->distance) || hit->distance>7.f || hit->distance<0) { reset(); return {}; }
  auto block=world.get(hit->block);
  float duration=breakSeconds(world.crafting,block,equipped);
  if(duration<=0) { reset(); return {}; }
  if(!cell_ || *cell_!=hit->block || block_!=block) { reset(); cell_=hit->block; block_=block; }
  if(!std::isfinite(dt)) return {};
  elapsed_+=std::clamp(dt,0.f,.1f); progress_=std::clamp(elapsed_/duration,0.f,1.f);
  if(progress_<1.f) return {};
  auto event=BreakEvent{*cell_,block_}; reset();
  if(!breakBlock(world,event.cell)) return {};
  collectMaterial(world,event.block); world.guideFlags|=Broke;
  return event;
}
float DebrisCloud::random() {
  seed_^=seed_<<13; seed_^=seed_>>17; seed_^=seed_<<5;
  return float(seed_&0xffffu)/65535.f;
}
void DebrisCloud::emit(Cell c,Block block) {
  if(block==Block::Air || block==Block::Bedrock) return;
  constexpr std::size_t count=18;
  if(particles_.size()+count>capacity) particles_.erase(particles_.begin(),particles_.begin()+std::ptrdiff_t(particles_.size()+count-capacity));
  auto box=blockBounds(c,block);
  float material=block==Block::Glass ? 12.f : isDoor(block) ? 15.f : isBed(block) ? 17.f
                 : block==Block::Workbench ? 19.f : block==Block::Torch ? 13.f : isGate(block) || block==Block::Fence ? 7.f
                 : isWheat(block) ? (block==Block::WheatRipe ? 25.f : 24.f)
                 : isCrop(block) ? (cropRipe(block) ? (cropKind(block)==CropKind::Strawberry ? 35.f : 34.f) : 24.f)
                 : block==Block::Farmland ? 40.f : block==Block::Sprinkler ? 42.f : block==Block::StoneSlab ? 3.f
                 : isCityMaterial(block) ? cityMaterial(block) : isFurniture(block) ? 72.f : float(block);
  for(std::size_t i=0;i<count;++i) {
    glm::vec3 offset(random(),random(),random());
    glm::vec3 velocity{(random()-.5f)*3.4f,.9f+random()*2.2f,(random()-.5f)*3.4f};
    particles_.push_back({glm::mix(box.min,box.max,offset),velocity,0,.4f+random()*.5f,.045f+random()*.035f,material});
  }
}
void DebrisCloud::water(Cell cell) {
  constexpr std::size_t count=24;
  if(particles_.size()+count>capacity) particles_.erase(particles_.begin(),particles_.begin()+std::ptrdiff_t(particles_.size()+count-capacity));
  for(std::size_t i=0;i<count;++i) {
    glm::vec3 p{cell.x+.12f+random()*.76f,cell.y+.70f+random()*.35f,cell.z+.12f+random()*.76f};
    particles_.push_back({p,{(random()-.5f)*.3f,-.6f-random()*.4f,(random()-.5f)*.3f},0,.22f+random()*.16f,.025f+random()*.015f,37});
  }
}
void DebrisCloud::rain(glm::vec3 center) {
  constexpr std::size_t count=8;
  if(particles_.size()+count>capacity) particles_.erase(particles_.begin(),particles_.begin()+std::ptrdiff_t(particles_.size()+count-capacity));
  for(std::size_t i=0;i<count;++i) {
    glm::vec3 p=center+glm::vec3((random()-.5f)*12.f,2+random()*4.f,(random()-.5f)*12.f);
    particles_.push_back({p,{-.3f,-7.f,.1f},0,.45f+random()*.15f,.023f,37});
  }
}
void DebrisCloud::tick(float dt) {
  if(!std::isfinite(dt)) return;
  dt=std::clamp(dt,0.f,.1f);
  for(auto& p : particles_) { p.age+=dt; p.velocity.y-=9.f*dt; p.position+=p.velocity*dt; }
  std::erase_if(particles_,[](const auto& p){return p.age>=p.life;});
}
std::vector<Vertex> DebrisCloud::mesh(glm::vec3 right,glm::vec3 up) const {
  std::vector<Vertex> out; out.reserve(particles_.size()*6);
  constexpr std::array<glm::vec2,4> uv{{{0,0},{1,0},{1,1},{0,1}}};
  for(const auto& p : particles_) {
    float size=p.size*(1.f-.75f*p.age/p.life);
    for(int i : {0,1,2,0,2,3}) {
      auto local=uv[i]*2.f-1.f;
      out.push_back({p.position+size*(right*local.x+up*local.y),uv[i],p.material,.8f,glm::vec3(-100001)});
    }
  }
  return out;
}
} // namespace bw

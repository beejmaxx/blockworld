#pragma once
#include "coast.hpp"
#include <span>

namespace bw {
inline constexpr int harborGround=23,harborFloorHeight=5;
struct HarborBuilding {
  int x,z,width,depth,floors,style,turn;
  Block wall,trim;
  std::string_view name;
};
std::span<const HarborBuilding> harborBuildings();
glm::vec3 harborPosition(Cell coast,const HarborBuilding&,glm::vec3 local);
void generateHarbor(Chunk&,Cell coast,std::uint32_t lots);
bool harborGraded(Cell coast,std::uint32_t lots,int x,int z);
bool initializeHarbor(World&,const Player&);
bool visitHarbor(World&,Player&,bool penthouse=false);
} // namespace bw

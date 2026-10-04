#pragma once
#include "player.hpp"
#include <span>

namespace bw {
inline constexpr int citySize=128;
inline constexpr int cityFloorHeight=4;
struct CityBuilding {
  int x,z,width,depth,floors;
  Block wall,trim;
};
std::span<const CityBuilding> cityBuildings();
// Generated before player edits, including when the chunk worker reloads a district.
void generateCity(Chunk& chunk,Cell origin);
bool initializeCity(World& world,const Player& player);
bool visitCity(World& world,Player& player);
bool atCity(const World& world,const Player& player);
} // namespace bw

#pragma once
#include "world.hpp"

namespace bw {
inline constexpr int mapCells=32;
struct MapMarker {glm::vec2 position{};std::string name;glm::vec3 color{};};
struct MiniMap {
  bool enabled=false;
  glm::vec2 center{},player{};
  float radius=96,yaw=0;
  std::array<glm::vec3,mapCells*mapCells> colors{};
  std::vector<std::array<glm::vec2,2>> roads;
  std::vector<MapMarker> markers;
};
glm::vec2 mapPoint(const MiniMap&,glm::vec2 world);
std::optional<std::array<glm::vec2,2>> mapLine(const MiniMap&,glm::vec2 a,glm::vec2 b);
MiniMap buildMiniMap(const World&,glm::vec2 player,bool overview,bool driving);
} // namespace bw

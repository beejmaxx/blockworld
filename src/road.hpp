#pragma once
#include "player.hpp"

namespace bw {
inline constexpr float roadHalfWidth=6.f,roadClearance=11.f;
inline constexpr std::size_t roadPointLimit=1024;
struct RoadSample {
  float distance=1e9f,along=0,length=0,height=0;
  glm::vec2 center{},direction{};
};
RoadSample sampleRoad(const std::vector<glm::vec3>&,float x,float z);
void generateRoad(Chunk&,const std::vector<glm::vec3>&);
bool initializeRoad(World&,const Player&);
bool visitRoad(World&,Player&);
} // namespace bw

#pragma once
#include "player.hpp"
#include <span>

namespace bw {
inline constexpr int estateWidth=640,estateDepth=416;
struct EstatePlace { std::string_view name,description; glm::vec3 arrival,target; };
std::span<const EstatePlace> estatePlaces();
bool estateContains(Cell,float x,float z,float margin=0);
std::vector<glm::vec3> estateRoad(const World&);
bool initializeEstate(World&,const Player&);
void generateEstate(Chunk&,const World&);
bool visitEstate(World&,Player&,int destination);
}

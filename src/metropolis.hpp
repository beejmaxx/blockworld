#pragma once
#include "harbor.hpp"

namespace bw {
inline constexpr int metroColumns=7,metroRows=6,metroPitch=64;
inline constexpr int metroWidth=metroColumns*metroPitch,metroDepth=metroRows*metroPitch;
std::span<const HarborBuilding> metroBuildings();
constexpr bool metroOffice(std::size_t index){return index>=residentCount && index%3==1;}
bool metroContains(Cell,float x,float z,float margin=0);
void generateMetropolis(Chunk&,const World&);
std::vector<glm::vec3> metroLink(const World&);
bool initializeMetropolis(World&,const Player&);
bool visitMetropolis(World&,Player&,bool roof=false);
bool visitMetroProperty(World&,Player&,int index);
bool visitGarage(World&,Player&);
bool visitBank(World&,Player&);
bool visitDataCenter(World&,Player&,int index=0);
glm::vec3 garagePosition(Cell,int car);
glm::vec3 bankTerminal(Cell);
glm::vec3 metroResidentHome(Cell,int resident);
std::vector<Vertex> metropolisSkyline(const World&,glm::vec3 eye);
inline constexpr std::size_t skylineVertexLimit=180000;
} // namespace bw

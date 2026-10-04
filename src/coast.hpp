#pragma once
#include "player.hpp"

namespace bw {
inline constexpr int coastSize=384;
inline constexpr int coastSeaLevel=18; // Height of the water surface, not its bottom.
inline constexpr int coastViewRadius=12;
struct CoastColumn {
  int ground{};
  bool water=false,road=false,pavement=false,beach=false;
};
// All samples use world coordinates, so adjacent streamed chunks agree.
CoastColumn coastColumn(const Terrain&,Cell origin,int x,int z);
bool coastContains(Cell origin,float x,float z,int margin=0);
void generateCoast(Chunk&,const Terrain&,Cell origin);
bool initializeCoast(World&,const Player&);
bool visitCoast(World&,Player&);
bool atCoast(const World&,const Player&);
} // namespace bw

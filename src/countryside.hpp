#pragma once
#include "player.hpp"

namespace bw {
inline constexpr int countrysideWidth=112,countrysideDepth=96;
bool countrysideContains(Cell origin,float x,float z,float margin=0);
void generateCountryside(Chunk&,Cell origin);
bool initializeCountryside(World&,const Player&);
bool atCountryside(const World&,const Player&);
} // namespace bw

#pragma once
#include "player.hpp"

namespace bw {
// The origin is the northwest corner at courtyard floor height.
// Construction only uses a completely untouched parcel; later edits stay edited.
bool initializeCastle(World& world,const Player& player);
bool visitCastle(World& world,Player& player);
bool atCastle(const World& world,const Player& player);
} // namespace bw

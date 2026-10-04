#pragma once
#include "farm.hpp"

namespace bw {
struct RanchTarget { bool car=false; std::size_t index=0; float distance=0; };
struct RideState { bool active=false,car=false; std::size_t index=0; };
Box livestockBounds(const Livestock& animal);
Box carBounds(const FarmCar& car);
bool ranchOverlap(const World& world,Box box);
std::optional<RanchTarget> targetRanch(const World& world,const Player& player,float reach=3.5f);
std::string buyLivestock(World& world,const Player& player,LivestockKind kind);
std::string bringCar(World& world,const Player& player);
std::string recoverCar(World& world,Player& player,const RideState& ride);
bool mountRanch(World& world,Player& player,RideState& ride,RanchTarget target);
bool leaveRide(World& world,Player& player,RideState& ride);
void tickRanch(World& world,Player& player,RideState& ride,Movement movement,float dt);
bool collectMilk(World& world,const Player& player,std::size_t index);
bool petLivestock(World& world,const Player& player,std::size_t index);
void initializeCastlePets(World& world);
std::string ranchPrompt(const World& world,RanchTarget target);
std::vector<Vertex> ranchMesh(const World& world);
inline constexpr std::size_t ranchVertexLimit=(livestockLimit*32+180)*36;
} // namespace bw

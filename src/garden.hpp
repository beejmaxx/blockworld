#pragma once
#include "farm.hpp"

namespace bw {
enum class GardenPurchase { Compost,Sprinkler,Greenhouse };
constexpr int gardenPrice(GardenPurchase item) {
  return item==GardenPurchase::Compost ? 3 : item==GardenPurchase::Sprinkler ? 12 : 40;
}
bool initializeGarden(World& world);
bool visitGarden(World& world,Player& player);
bool tillSoil(World& world,const Player& player,Cell soil);
bool applyCompost(World& world,const Player& player,Cell plant);
int basketValue(const FarmState& farm);
int sellBasket(World& world);
bool buyGardenSupply(World& world,GardenPurchase item);
std::string greenhouseProblem(const World& world,const Player& player,Cell door);
bool placeGreenhouse(World& world,const Player& player,Cell door);
GuideView gardenGuide(const World& world,const Player& player);
} // namespace bw

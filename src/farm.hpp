#pragma once
#include "adventure.hpp"

namespace bw {
void initializeFarm(World& world);
void growFarm(World& world,float seconds);
void tickFarm(World& world,const Player& player,float dt,bool offeringWheat);
float chickenScale(const Chicken& chicken);
std::string animalName(const Chicken& chicken,std::size_t index);
bool validAnimalName(std::string_view name);
bool renameChicken(World& world,std::size_t index,std::string_view name);
std::string hatchProblem(const World& world,std::size_t mother);
bool incubateEgg(World& world,std::size_t mother);
Box chickenBounds(const Chicken& chicken);
bool chickensOverlap(const World& world,Box box);
std::optional<std::size_t> targetChicken(const World& world,const Player& player,float reach=3.5f);
std::string chickenPrompt(const World& world,std::size_t index);
enum class FarmUse { None,Fed,Collected,Harvested,Petted };
FarmUse useChicken(World& world,const Player& player,std::size_t index);
FarmUse harvestWheat(World& world,const Player& player,Cell cell);
FarmUse harvestCrop(World& world,const Player& player,Cell cell);
bool waterCrop(World& world,const Player& player,Cell cell);
std::string cropPrompt(const World& world,Cell cell,bool wateringCan);
bool toggleGate(World& world,const Player& player,Cell cell);
std::vector<BlueprintPiece> penBlueprint(const World& world);
bool penComplete(const World& world);
bool buildPenNext(World& world,const Player& player);
GuideView farmGuide(const World& world,const Player& player);
std::vector<Vertex> chickenMesh(const World& world);
struct AnimalView {
  std::string name,status,mother,hatchProblem;
  bool baby=false;
  float growth=1,hatching=-1;
};
struct FarmView {
  std::vector<AnimalView> animals;
  int wheat=0,eggs=0,plants=0,ripe=0,watered=0;
  std::array<int,4> harvest{};
  GardenState garden;
  int basketValue=0;
  int cows=0,horses=0,sheep=0,foxes=0,milk=0;
  bool carOwned=false;
};
FarmView farmView(const World& world);
inline constexpr std::size_t chickenVertexLimit=flockLimit*22*36;
} // namespace bw

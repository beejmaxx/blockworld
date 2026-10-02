#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace bw {
inline constexpr float wheatGrowSeconds=90.f,eggLaySeconds=30.f;
inline constexpr float eggHatchSeconds=60.f,chickGrowSeconds=180.f;
inline constexpr float cropWaterSeconds=45.f;
inline constexpr int coinLimit=999999, gardenSupplyLimit=999, sprinklerLimit=64;
inline constexpr float showerPeriod=240.f, showerStart=150.f, showerEnd=195.f;
enum GardenFlag : std::uint32_t { PreparedBed=1,SowedCrop=2,WateredCrop=4,PickedCrop=8,SoldHarvest=16,ImprovedGarden=32 };
struct GardenState {
  bool initialized=false;
  glm::ivec3 origin{16,24,7}; // North-west corner of the starter garden, at plant height.
  int coins=0,compost=3,sprinklers=0,greenhouses=0;
  std::uint32_t flags=0;
  float weather=0;
  bool raining() const { return initialized && weather>=showerStart && weather<showerEnd; }
};
enum class CropKind : std::uint8_t { Wheat,Carrot,Strawberry,Pumpkin,Count };
inline constexpr std::array cropKinds{CropKind::Wheat,CropKind::Carrot,CropKind::Strawberry,CropKind::Pumpkin};
constexpr float cropGrowSeconds(CropKind kind) {
  return kind==CropKind::Carrot ? 75.f : kind==CropKind::Strawberry ? 105.f : kind==CropKind::Pumpkin ? 150.f : wheatGrowSeconds;
}
constexpr int cropYield(CropKind kind) { return kind==CropKind::Pumpkin ? 1 : kind==CropKind::Carrot ? 2 : 3; }
constexpr int cropPrice(CropKind kind) { return kind==CropKind::Carrot ? 2 : kind==CropKind::Strawberry ? 3 : kind==CropKind::Pumpkin ? 8 : 1; }
constexpr std::string_view cropName(CropKind kind) {
  return kind==CropKind::Carrot ? "CARROTS" : kind==CropKind::Strawberry ? "STRAWBERRIES" : kind==CropKind::Pumpkin ? "PUMPKINS" : "WHEAT";
}
inline constexpr std::size_t starterFlock=4,flockLimit=12,cropLimit=256,animalNameLimit=18;
enum FarmFlag : std::uint32_t { PenBuilt=1,WheatPlanted=2,WheatHarvested=4,ChickenFed=8,EggCollected=16,EggIncubated=32,ChickHatched=64,AnimalNamed=128 };
struct Chicken {
  glm::vec3 position{};
  float yaw=0,eggTimer=-1;
  bool eggReady=false;
  // Animation and steering are transient; the flock and egg timers are saved.
  float think=0,walk=0,happy=0,fallSpeed=0,heading=0;
  bool moving=false,sleeping=false;
  std::string name;
  float growth=chickGrowSeconds,hatchTimer=-1;
  int mother=-1; // Stable vector index: animals are never removed from the flock.
};
inline bool isChick(const Chicken& chicken) { return chicken.growth<chickGrowSeconds; }
struct Crop {
  int x{},y{},z{};
  float age=0;
  CropKind kind=CropKind::Wheat;
  float water=0; // Remaining seconds of double growth; drying never harms a crop.
  bool composted=false;
  int shelter=-1; // Derived cache: -1 unknown, 0 outdoors, 1 roof, 2 glass roof.
};
enum class LivestockKind : std::uint8_t { Cow,Horse };
inline constexpr std::size_t livestockLimit=8;
inline constexpr float milkSeconds=60.f;
constexpr int livestockPrice(LivestockKind kind) { return kind==LivestockKind::Cow ? 20 : 35; }
struct Livestock {
  LivestockKind kind=LivestockKind::Cow;
  glm::vec3 position{},home{};
  float yaw=0,milkTimer=0;
  float think=0,heading=0,walk=0; // Transient animation and steering.
  bool moving=false;
};
struct FarmCar {
  bool owned=false;
  glm::vec3 position{};
  float yaw=0,speed=0; // Speed is transient; cars load parked.
};
struct FarmState {
  bool initialized=false;
  int wheat=0,eggs=0;
  int carrots=0,strawberries=0,pumpkins=0;
  std::uint32_t flags=0;
  glm::vec3 home{3.5f,24,8.5f};
  std::vector<Chicken> chickens;
  std::vector<Crop> crops;
  GardenState garden;
  std::vector<Livestock> livestock;
  FarmCar car;
  int milk=0;
  std::vector<glm::ivec3> sprinklers; // Derived from placed blocks, including unloaded chunks.
  int& harvest(CropKind kind) {
    return kind==CropKind::Carrot ? carrots : kind==CropKind::Strawberry ? strawberries : kind==CropKind::Pumpkin ? pumpkins : wheat;
  }
  int harvest(CropKind kind) const {
    return kind==CropKind::Carrot ? carrots : kind==CropKind::Strawberry ? strawberries : kind==CropKind::Pumpkin ? pumpkins : wheat;
  }
};
} // namespace bw

#pragma once
#include "player.hpp"
#include <string>

namespace bw {

// All player edits, including the guide, go through the same placement rules.
enum class PlacementStatus { Ready,OutsideWorld,Occupied,NeedsFloor,NoRoom,PlayerOverlap,WorkbenchLocked,NeedsSoil,ChickenOverlap,CropLimit,PumpkinSpace,NeedSprinkler };
PlacementStatus placementStatus(const World& world,const Player& player,Cell cell,Block block,bool alongX=false);
std::string_view placementMessage(PlacementStatus status);
bool placeBlock(World& world,const Player& player,Cell cell,Block block,bool alongX=false);
bool breakBlock(World& world,Cell cell);
bool toggleDoor(World& world,const Player& player,Cell cell);
enum class SleepResult { Ready,NotABed,TooFar,Daytime,Obstructed };
SleepResult bedSleepStatus(const World& world,const Player& player,Cell cell);
SleepResult sleepInBed(World& world,const Player& player,Cell cell);

enum GuideFlag : std::uint32_t { Looked=1,Walked=2,Broke=4,Placed=8,BuiltCabin=16,VisitedCave=32,Slept=64 };
enum class CabinPart { Frame,Windows,Roof,Door,Lamp,Bed,Count };
struct BlueprintPiece { Cell cell; Block block; CabinPart part; };
inline constexpr BlueprintPiece cabinBed{{11,25,-4},Block::BedZ,CabinPart::Bed};
const std::vector<BlueprintPiece>& cabinBlueprint();
bool pieceComplete(const World& world,const BlueprintPiece& piece);
struct Count { int done{},total{}; };
struct CabinStatus {
  std::array<Count,int(CabinPart::Count)> parts{};
  int done{},total{};
  bool complete() const { return done==total; }
};
CabinStatus inspectCabin(const World& world);
bool initializeHome(World& world);
bool visitHome(World& world,Player& player);

struct GuideView {
  bool enabled=false;
  bool farm=false;
  bool landmark=false;
  int stage=0;
  std::string title;
  std::array<std::string,3> lines;
  CabinStatus cabin;
  std::optional<Cell> preview;
  Block previewBlock=Block::Stone;
  std::optional<glm::vec3> destination;
  std::string destinationName;
};
class Adventure {
public:
  void looked(World& world,float dx,float dy);
  void moved(World& world,glm::vec3 before,glm::vec3 after);
  void update(World& world,const Player& player);
  int stage(const World& world) const;
  GuideView view(const World& world,const Player& player) const;
  std::optional<BlueprintPiece> nextPiece(const World& world,const Player& player) const;
  bool buildNext(World& world,const Player& player) const;
private:
  float lookDistance_=0,walkDistance_=0;
};
} // namespace bw

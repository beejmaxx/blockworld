#pragma once
#include "daylight.hpp"
#include "crafting.hpp"
#include "farm_state.hpp"
#include "inventory_state.hpp"

#include <glm/glm.hpp>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static_assert(__cplusplus > 202302L, "Blockworld requires C++26 mode.");

namespace bw {

inline constexpr int chunkSize = 16;
inline constexpr int worldHeight = 64;
inline constexpr int viewRadius = 6;
inline constexpr int coordinateLimit = 100000;

// Values 0-9 are kept stable for the original saves.
enum class Block : std::uint8_t {
  Air, Grass, Dirt, Stone, Sand, Wood, Leaves, Planks, Brick, Bedrock, Glass, Torch,
  DoorZ, DoorZTop, DoorZOpen, DoorZOpenTop, DoorX, DoorXTop, DoorXOpen, DoorXOpenTop,
  BedZ, BedZHead, BedX, BedXHead, Workbench,
  Fence,GateZ,GateZOpen,GateX,GateXOpen,WheatYoung,WheatGrowing,WheatRipe,
  CarrotYoung,CarrotGrowing,CarrotRipe,StrawberryYoung,StrawberryGrowing,StrawberryRipe,
  PumpkinYoung,PumpkinGrowing,PumpkinRipe,Farmland,Sprinkler,Count
};
std::string_view blockName(Block block);
glm::vec3 blockColor(Block block);
constexpr bool isDoor(Block b) { return b >= Block::DoorZ && b <= Block::DoorXOpenTop; }
constexpr bool doorUpper(Block b) { return isDoor(b) && (int(b)-int(Block::DoorZ))%2 == 1; }
constexpr bool doorOpen(Block b) { return isDoor(b) && ((int(b)-int(Block::DoorZ))%4)>=2; }
constexpr bool doorAlongX(Block b) { return b >= Block::DoorX && b <= Block::DoorXOpenTop; }
constexpr Block doorVariant(bool alongX, bool open, bool upper) {
  return Block(int(Block::DoorZ)+(alongX ? 4 : 0)+(open ? 2 : 0)+(upper ? 1 : 0));
}
constexpr bool isBed(Block b) { return b>=Block::BedZ && b<=Block::BedXHead; }
constexpr bool bedHead(Block b) { return b==Block::BedZHead || b==Block::BedXHead; }
constexpr bool bedAlongX(Block b) { return b==Block::BedX || b==Block::BedXHead; }
constexpr Block bedVariant(bool alongX,bool head) { return Block(int(Block::BedZ)+(alongX ? 2 : 0)+(head ? 1 : 0)); }
constexpr bool isGate(Block b) { return b>=Block::GateZ && b<=Block::GateXOpen; }
constexpr bool gateOpen(Block b) { return b==Block::GateZOpen || b==Block::GateXOpen; }
constexpr bool gateAlongX(Block b) { return b==Block::GateX || b==Block::GateXOpen; }
constexpr Block gateVariant(bool alongX,bool open) { return Block(int(Block::GateZ)+(alongX ? 2 : 0)+(open ? 1 : 0)); }
constexpr bool isWheat(Block b) { return b>=Block::WheatYoung && b<=Block::WheatRipe; }
constexpr bool isCrop(Block b) { return b>=Block::WheatYoung && b<=Block::PumpkinRipe; }
constexpr CropKind cropKind(Block b) { return CropKind((int(b)-int(Block::WheatYoung))/3); }
constexpr bool cropRipe(Block b) { return isCrop(b) && (int(b)-int(Block::WheatYoung))%3==2; }
inline Block cropStage(CropKind kind,float age) {
  auto duration=cropGrowSeconds(kind);
  return Block(int(Block::WheatYoung)+int(kind)*3+(age>=duration ? 2 : age>=duration*.5f ? 1 : 0));
}
inline Block wheatStage(float age) { return age>=wheatGrowSeconds ? Block::WheatRipe : age>=wheatGrowSeconds*.5f ? Block::WheatGrowing : Block::WheatYoung; }
constexpr bool opaque(Block b) { return b != Block::Air && b != Block::Glass && b != Block::Torch && b!=Block::Sprinkler && !isDoor(b) && !isBed(b) && b!=Block::Fence && !isGate(b) && !isCrop(b); }
constexpr bool collidable(Block b) { return b != Block::Air && b != Block::Torch && !isCrop(b) && !gateOpen(b); }

struct ChunkPos {
  int x{}, z{};
  bool operator==(const ChunkPos&) const = default;
};
struct Cell {
  int x{}, y{}, z{};
  bool operator==(const Cell&) const = default;
  Cell operator+(Cell b) const { return {x + b.x, y + b.y, z + b.z}; }
};
constexpr Cell bedOffset(Block b) { return bedAlongX(b) ? Cell{1,0,0} : Cell{0,0,-1}; }
inline Cell bedOther(Cell c,Block b) {
  auto d=bedOffset(b);
  return c+Cell{d.x*(bedHead(b) ? -1 : 1),0,d.z*(bedHead(b) ? -1 : 1)};
}
struct PositionHash {
  std::size_t operator()(ChunkPos p) const noexcept;
  std::size_t operator()(Cell p) const noexcept;
};
constexpr int floorDiv(int n, int d) { return n / d - (n % d < 0); }
constexpr int localCoord(int n) { return n - floorDiv(n, chunkSize) * chunkSize; }
constexpr ChunkPos chunkAt(int x, int z) { return {floorDiv(x, chunkSize), floorDiv(z, chunkSize)}; }
inline bool solid(Block block) { return block != Block::Air; }
struct Box { glm::vec3 min, max; };
Box blockBounds(Cell cell, Block block);

struct Chunk {
  ChunkPos pos;
  std::array<Block, chunkSize * chunkSize * worldHeight> blocks{};
  bool dirty = true;
  static constexpr int index(int x, int y, int z) { return (y * chunkSize + z) * chunkSize + x; }
  Block get(int x, int y, int z) const { return blocks[index(x, y, z)]; }
  void set(int x, int y, int z, Block value) { blocks[index(x, y, z)] = value; }
};

class Terrain {
public:
  explicit Terrain(std::uint32_t seed = 7262026, bool adventure = false) : seed_(seed), adventure_(adventure) {}
  int height(int x, int z) const;
  Chunk generate(ChunkPos pos) const;
  std::uint32_t seed() const { return seed_; }
  bool adventure() const { return adventure_; }
private:
  std::uint32_t seed_;
  bool adventure_;
  float noise(float x, float z) const;
  std::uint32_t hash(int x, int z) const;
};

struct PlayerPose {
  glm::vec3 position{}; // Feet, in world coordinates.
  float yaw = -0.65f;
  float pitch = -0.12f;
  bool flying = false;
};

struct RayHit {
  Cell block;
  Cell adjacent;
  float distance;
};

class World {
public:
  explicit World(std::uint32_t seed = 7262026, bool adventure = false) : terrain(seed,adventure) {}
  Terrain terrain;
  std::uint32_t guideFlags = 0;
  WorldClock clock;
  CraftState crafting;
  Inventory inventory;
  FarmState farm;
  std::unordered_map<ChunkPos, Chunk, PositionHash> chunks;
  Block get(Cell cell) const;
  bool set(Cell cell, Block block);
  const Crop* cropAt(Cell cell) const;
  bool waterCrop(Cell cell);
  bool compostCrop(Cell cell);
  int cropShelter(Cell cell) const;
  void growCrops(float seconds);
  void insert(Chunk chunk);
  void ensure(ChunkPos center, int radius);
  void evict(ChunkPos center, int radius);
  std::optional<RayHit> raycast(glm::vec3 origin, glm::vec3 direction, float reach = 7.f) const;
  void save(const std::filesystem::path& path, const PlayerPose& player) const;
  std::optional<PlayerPose> load(const std::filesystem::path& path);
  std::size_t editCount() const { return edits_.size(); }
  bool edited(Cell cell) const { return edits_.contains(cell); }
private:
  std::unordered_map<Cell, Block, PositionHash> edits_;
  void invalidate(ChunkPos pos);
};

// CPU terrain generation lives on one worker. Only the main thread owns World and GPU resources.
class ChunkWorker {
public:
  explicit ChunkWorker(Terrain terrain);
  ~ChunkWorker();
  void request(ChunkPos center, const World& world);
  void collect(World& world, ChunkPos center);
private:
  Terrain terrain_;
  std::mutex mutex_;
  std::condition_variable ready_;
  bool stopping_ = false;
  std::deque<ChunkPos> pending_;
  std::deque<Chunk> completed_;
  std::unordered_set<ChunkPos, PositionHash> requested_;
  std::thread thread_;
};

struct Vertex {
  glm::vec3 position;
  glm::vec2 uv;
  float material;
  float light;
  glm::vec3 block;
};
std::vector<Vertex> buildMesh(const World& world, const Chunk& chunk);
void appendBox(std::vector<Vertex>& vertices,Box box,Cell cell,float material,float light);
std::vector<Vertex> buildPreview(Cell cell, Block block,float material=20.f,bool exact=false);

} // namespace bw

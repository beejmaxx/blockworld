#include "world.hpp"
#include "estate.hpp"
#include "inventory.hpp"
#include "farm.hpp"
#include "city.hpp"
#include "coast.hpp"
#include "harbor.hpp"
#include "road.hpp"
#include "countryside.hpp"
#include "metropolis.hpp"
#include "city_life.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace bw {
namespace {
std::uint32_t mix(std::uint32_t h) {
  h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
  return h;
}
constexpr std::array<Cell, 6> neighbors{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
bool validCell(Cell c) {
  return c.x >= -coordinateLimit && c.x <= coordinateLimit &&
         c.z >= -coordinateLimit && c.z <= coordinateLimit && c.y > 0 && c.y < worldHeight;
}
}

std::size_t PositionHash::operator()(ChunkPos p) const noexcept {
  return (std::uint64_t(mix(std::uint32_t(p.x))) << 32) | mix(std::uint32_t(p.z));
}
std::size_t PositionHash::operator()(Cell p) const noexcept {
  return (*this)(ChunkPos{p.x, p.z}) ^ (std::uint64_t(mix(std::uint32_t(p.y))) * 0x9e3779b97f4a7c15ull);
}
std::string_view blockName(Block b) {
  if(isFurniture(b)) { constexpr std::array names{"Sofa","Coffee table","Chair","Planter"}; return names[int(b)-int(Block::Sofa)]; }
  if(isCityMaterial(b)) {
    constexpr std::array names{"White concrete","Limestone","Terracotta","Sage concrete","Charcoal","Asphalt","Water","Lantern block","Blue facade glass","Blue roof tile","Red tile","Gold tile"};
    return names[int(b)-int(Block::Concrete)];
  }
  if(isDoor(b)) return "Oak door";
  if(isBed(b)) return "Cozy bed";
  if(b==Block::Workbench) return "Workbench";
  if(b==Block::Fence) return "Oak fence";
  if(b==Block::Farmland) return "Prepared soil";
  if(b==Block::Sprinkler) return "Sprinkler";
  if(b==Block::StoneSlab) return "Stone step";
  if(isGate(b)) return "Garden gate";
  if(isWheat(b)) return "Wheat seeds / food";
  if(isCrop(b)) return cropKind(b)==CropKind::Carrot ? "Carrot seeds" : cropKind(b)==CropKind::Strawberry ? "Strawberry seeds" : "Pumpkin seeds";
  constexpr std::array names{"Air", "Grass", "Dirt", "Stone", "Sand", "Oak", "Leaves", "Planks", "Brick", "Bedrock", "Glass", "Torch"};
  return names[static_cast<unsigned>(b)];
}
glm::vec3 blockColor(Block b) {
  if(isFurniture(b)) return b==Block::Planter ? glm::vec3(.3f,.53f,.28f) : b==Block::Sofa ? glm::vec3(.83f,.81f,.72f) : glm::vec3(.44f,.29f,.18f);
  if(isCityMaterial(b)) {
    constexpr std::array<glm::vec3,12> colors{{{.88f,.89f,.84f},{.78f,.72f,.55f},{.72f,.34f,.22f},{.34f,.53f,.40f},
      {.20f,.24f,.26f},{.16f,.19f,.21f},{.15f,.52f,.63f},{1.f,.80f,.43f},{.27f,.54f,.65f},{.20f,.58f,.69f},{.83f,.22f,.19f},{.96f,.71f,.20f}}};
    return colors[int(b)-int(Block::Concrete)];
  }
  if(isDoor(b)) return {.61f,.40f,.20f};
  if(isBed(b)) return {.72f,.27f,.22f};
  if(b==Block::Workbench) return {.64f,.44f,.24f};
  if(b==Block::Farmland) return {.36f,.23f,.13f};
  if(b==Block::Sprinkler) return {.39f,.72f,.77f};
  if(b==Block::StoneSlab) return blockColor(Block::Stone);
  if(b==Block::Fence || isGate(b)) return {.66f,.46f,.24f};
  if(isWheat(b)) return b==Block::WheatRipe ? glm::vec3(.91f,.72f,.26f) : glm::vec3(.40f,.67f,.22f);
  if(isCrop(b)) return cropKind(b)==CropKind::Strawberry ? glm::vec3(.88f,.19f,.29f) : glm::vec3(.96f,.48f,.12f);
  if(b==Block::Glass) return {.66f,.85f,.88f};
  if(b==Block::Torch) return {1.f,.69f,.24f};
  constexpr std::array<glm::vec3, 10> colors{{{0,0,0},{.40f,.62f,.22f},{.49f,.32f,.20f},
    {.55f,.57f,.57f},{.80f,.74f,.49f},{.46f,.31f,.17f},{.27f,.49f,.18f},
    {.68f,.48f,.26f},{.66f,.31f,.22f},{.21f,.23f,.24f}}};
  return colors[static_cast<unsigned>(b)];
}
Box blockBounds(Cell c, Block block) {
  glm::vec3 origin(c.x,c.y,c.z),low(0),high(1);
  if(block==Block::Torch) { low={.38f,0,.38f}; high={.62f,.78f,.62f}; }
  if(block==Block::Sprinkler) { low={.18f,0,.18f}; high={.82f,.65f,.82f}; }
  if(isBed(block)) high.y=bedHead(block) ? .63f : .55f;
  if(block==Block::StoneSlab) high.y=.5f;
  if(block==Block::Sofa) { low={0,0,.06f}; high={1,.88f,.94f}; }
  if(block==Block::Table) { low={.05f,0,.05f}; high={.95f,.58f,.95f}; }
  if(block==Block::Chair) { low={.16f,0,.12f}; high={.84f,.94f,.88f}; }
  if(block==Block::Planter) { low={.16f,0,.16f}; high={.84f,.98f,.84f}; }
  if(isWheat(block)) { low={.14f,0,.14f}; high={.86f,block==Block::WheatYoung ? .28f : block==Block::WheatGrowing ? .53f : .85f,.86f}; }
  else if(isCrop(block)) {
    int stage=(int(block)-int(Block::WheatYoung))%3;
    low={.12f,0,.12f}; high={.88f,.22f+stage*.19f,.88f};
    if(cropKind(block)==CropKind::Pumpkin && stage==1) high.y=.5f;
    if(cropKind(block)==CropKind::Pumpkin && stage==2) high.y=.88f;
    if(cropKind(block)==CropKind::Strawberry && stage>0) high.y=.6f;
  }
  if(isGate(block)) {
    if(gateOpen(block)) { if(gateAlongX(block)) high.z=.14f; else high.x=.14f; }
    else if(gateAlongX(block)) { low.x=.43f; high.x=.57f; }
    else { low.z=.43f; high.z=.57f; }
  }
  if(isDoor(block)) {
    if(doorAlongX(block) != doorOpen(block)) { low.x=.82f; }
    else { low.z=.82f; }
    // Swing into the cell at its hinge, leaving a real, traversable opening.
    if(doorOpen(block)) {
      low=glm::vec3(0);
      if(doorAlongX(block)) high.z=.18f; else high.x=.18f;
    }
  }
  return {origin+low,origin+high};
}
std::uint32_t Terrain::hash(int x, int z) const {
  return mix(std::uint32_t(x) * 0x1f123bb5u ^ std::uint32_t(z) * 0x5f356495u ^ seed_);
}
float Terrain::noise(float x, float z) const {
  const int ix = int(std::floor(x)), iz = int(std::floor(z));
  float fx = x - float(ix), fz = z - float(iz);
  fx = fx * fx * (3.f - 2.f * fx); fz = fz * fz * (3.f - 2.f * fz);
  auto sample = [&](int a, int b) { return float(hash(a, b) & 65535u) / 65535.f; };
  return std::lerp(std::lerp(sample(ix, iz), sample(ix + 1, iz), fx),
                   std::lerp(sample(ix, iz + 1), sample(ix + 1, iz + 1), fx), fz);
}
int Terrain::height(int x, int z) const {
  const float h = 10.f + noise(float(x) / 90.f, float(z) / 90.f) * 21.f
    + noise(float(x) / 29.f, float(z) / 29.f) * 7.f + noise(float(x) / 12.f, float(z) / 12.f) * 2.f;
  if(!adventure_) return std::clamp(int(h), 6, worldHeight - 12);
  float edge=std::max(std::abs(float(x)-10.f)-14.f,std::abs(float(z)-1.f)-16.f);
  float blend=std::clamp(edge/6.f,0.f,1.f); blend=blend*blend*(3-2*blend);
  float ground=std::lerp(23.f,h,blend);
  float hill=std::max(0.f,1.f-glm::length(glm::vec2(x-34,z+4))/12.f);
  ground=std::max(ground,23.f+hill*15.f);
  return std::clamp(int(ground),6,worldHeight-12);
}
Chunk Terrain::generate(ChunkPos pos) const {
  Chunk chunk{pos};
  const int bx = pos.x * chunkSize, bz = pos.z * chunkSize;
  for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x) {
    int h = height(bx + x, bz + z);
    for (int y = 0; y <= h; ++y) {
      Block block = Block::Stone;
      if (y == 0) block = Block::Bedrock;
      else if (y == h) block = h < 19 ? Block::Sand : Block::Grass;
      else if (y > h - 4) block = h < 19 ? Block::Sand : Block::Dirt;
      chunk.set(x, y, z, block);
    }
  }
  auto put = [&](int x, int y, int z, Block b) {
    x -= bx; z -= bz;
    if (x < 0 || x >= chunkSize || z < 0 || z >= chunkSize || y < 0 || y >= worldHeight) return;
    if (b == Block::Wood || chunk.get(x,y,z) == Block::Air) chunk.set(x,y,z,b);
  };
  // Include neighboring tree anchors so canopies agree across chunk boundaries.
  for (int gz = floorDiv(bz - 3, 8); gz <= floorDiv(bz + chunkSize + 2, 8); ++gz)
    for (int gx = floorDiv(bx - 3, 8); gx <= floorDiv(bx + chunkSize + 2, 8); ++gx) {
      const auto hsh = hash(gx * 17, gz * 17);
      if (hsh % 100 > 42) continue;
      int x = gx * 8 + int((hsh >> 8) % 6) + 1, z = gz * 8 + int((hsh >> 16) % 6) + 1;
      const int ground = height(x,z), trunk = 4 + int(hsh % 3);
      if (ground < 20 || std::abs(height(x+2,z+2) - ground) > 2) continue;
      if(adventure_ && x>=-5 && x<=27 && z>=-18 && z<=20) continue;
      if(adventure_ && x>=23 && x<=41 && z>=-11 && z<=3) continue;
      // An open patch at spawn makes the first view and first steps predictable.
      if (std::abs(x-8) < 5 && std::abs(z-8) < 5) continue;
      for (int y = ground + trunk - 2; y <= ground + trunk + 1; ++y) {
        int radius = y == ground + trunk + 1 ? 1 : 2;
        for (int dz = -radius; dz <= radius; ++dz) for (int dx = -radius; dx <= radius; ++dx) {
          if (std::abs(dx) == radius && std::abs(dz) == radius && ((hsh + y) % 2 || radius == 1)) continue;
          put(x+dx,y,z+dz,Block::Leaves);
        }
      }
      for (int y = ground + 1; y <= ground + trunk; ++y) put(x,y,z,Block::Wood);
    }
  if(adventure_) {
    auto write=[&](int x,int y,int z,Block b) {
      if(x>=bx && x<bx+chunkSize && z>=bz && z<bz+chunkSize && y>=0 && y<worldHeight)
        chunk.set(x-bx,y,z-bz,b);
    };
    // The starter foundation and corner posts are generated, so player edits still override them.
    for(int z=-6;z<=-2;++z) for(int x=8;x<=12;++x) write(x,24,z,Block::Planks);
    for(int y=25;y<=27;++y) for(int x : {8,12}) for(int z : {-6,-2}) write(x,y,z,Block::Wood);
    for(int z=1;z<=10;++z) write(10,23,z,Block::Sand);
    for(int x=13;x<=26;++x) write(x,23,-4,Block::Sand);
    write(7,24,3,Block::Wood); write(7,25,3,Block::Wood);
    // An accessible, level tunnel opens into a small lantern chamber in the hill.
    for(int z=bz;z<bz+chunkSize;++z) for(int x=bx;x<bx+chunkSize;++x) {
      bool tunnel=x>=24 && x<=36 && std::abs(z+4)<=1;
      bool chamber=(x-36)*(x-36)+(z+4)*(z+4)<=16;
      if(tunnel || chamber) {
        write(x,23,z,Block::Stone);
        int ceiling=chamber ? 29 : 27;
        for(int y=24;y<=ceiling;++y) write(x,y,z,Block::Air);
      }
    }
    for(Cell c : {Cell{27,24,-5},Cell{32,24,-5},Cell{38,24,-4},Cell{36,24,-7}}) write(c.x,c.y,c.z,Block::Torch);
  }
  return chunk;
}

Block World::get(Cell c) const {
  if (c.y < 0) return Block::Bedrock;
  if (c.y >= worldHeight) return Block::Air;
  auto it = chunks.find(chunkAt(c.x,c.z));
  return it == chunks.end() ? Block::Air : it->second.get(localCoord(c.x),c.y,localCoord(c.z));
}
void World::invalidate(ChunkPos p) {
  if (auto it = chunks.find(p); it != chunks.end()) it->second.dirty = true;
}
bool World::set(Cell c, Block block) {
  if (!validCell(c) || block == Block::Bedrock || block >= Block::Count) return false;
  const ChunkPos p = chunkAt(c.x,c.z);
  auto it = chunks.find(p);
  if (it == chunks.end() || get(c) == Block::Bedrock || get(c) == block) return false;
  auto previous=get(c);
  if(block==Block::Sprinkler && farm.sprinklers.size()>=sprinklerLimit) return false;
  auto crop=std::ranges::find_if(farm.crops,[&](const auto& v){return v.x==c.x && v.y==c.y && v.z==c.z;});
  if(isCrop(block)) {
    auto kind=cropKind(block);
    float age=float((int(block)-int(Block::WheatYoung))%3)*cropGrowSeconds(kind)*.5f;
    if(crop==farm.crops.end()) {
      if(farm.crops.size()>=cropLimit) return false;
      farm.crops.push_back({c.x,c.y,c.z,age,kind});
    } else if(cropStage(crop->kind,crop->age)!=block) { crop->kind=kind; crop->age=age; crop->water=0; crop->composted=false; }
    if(kind==CropKind::Wheat) farm.flags|=WheatPlanted;
  } else if(crop!=farm.crops.end()) farm.crops.erase(crop);
  if(previous==Block::Sprinkler) std::erase(farm.sprinklers,glm::ivec3(c.x,c.y,c.z));
  if(block==Block::Sprinkler) farm.sprinklers.push_back({c.x,c.y,c.z});
  if(opaque(previous) || opaque(block) || previous==Block::Glass || block==Block::Glass)
    for(auto& plant : farm.crops) plant.shelter=-1;
  it->second.set(localCoord(c.x),c.y,localCoord(c.z),block);
  edits_[c] = block;
  // Ambient occlusion also depends on diagonal neighbors at chunk corners.
  for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) invalidate({p.x+dx,p.z+dz});
  return true;
}
const Crop* World::cropAt(Cell c) const {
  auto it=std::ranges::find_if(farm.crops,[&](const auto& crop){return crop.x==c.x && crop.y==c.y && crop.z==c.z;});
  return it==farm.crops.end() ? nullptr : &*it;
}
bool World::waterCrop(Cell c) {
  if(!isCrop(get(c)) || cropRipe(get(c))) return false;
  auto it=std::ranges::find_if(farm.crops,[&](const auto& crop){return crop.x==c.x && crop.y==c.y && crop.z==c.z;});
  if(it==farm.crops.end() || it->water>0) return false;
  it->water=cropWaterSeconds;
  farm.garden.flags|=WateredCrop;
  invalidate(chunkAt(c.x,c.z));
  return true;
}
bool World::compostCrop(Cell c) {
  if(!isCrop(get(c)) || cropRipe(get(c))) return false;
  auto it=std::ranges::find_if(farm.crops,[&](const auto& crop){return crop.x==c.x && crop.y==c.y && crop.z==c.z;});
  if(it==farm.crops.end() || it->composted) return false;
  it->composted=true; invalidate(chunkAt(c.x,c.z)); return true;
}
int World::cropShelter(Cell c) const {
  const auto pos=chunkAt(c.x,c.z);
  auto chunk=chunks.find(pos);
  std::optional<Chunk> generated;
  if(chunk==chunks.end()) { generated=terrain.generate(pos); generateStructures(*generated); }
  for(int y=c.y+1;y<worldHeight;++y) {
    auto edit=edits_.find({c.x,y,c.z});
    auto b=edit!=edits_.end() ? edit->second
      : chunk!=chunks.end() ? chunk->second.get(localCoord(c.x),y,localCoord(c.z))
      : generated->get(localCoord(c.x),y,localCoord(c.z));
    if(b==Block::Glass) return 2;
    if(opaque(b)) return 1;
  }
  return 0;
}
void World::growCrops(float seconds) {
  if(!std::isfinite(seconds) || seconds<=0) return;
  auto& garden=farm.garden;
  // All crops ripen within a cycle. Skip middle cycles on very long sleeps while
  // still simulating the final cycle so weather and moisture resume accurately.
  if(seconds>showerPeriod*2) {
    growCrops(showerPeriod);
    garden.weather=float(std::fmod(double(garden.weather)+double(seconds)-showerPeriod*2,showerPeriod));
    growCrops(showerPeriod); return;
  }
  while(seconds>0) {
    float boundary=garden.weather<showerStart ? showerStart : garden.weather<showerEnd ? showerEnd : showerPeriod;
    float step=std::min(seconds,boundary-garden.weather);
    for(auto& crop : farm.crops) {
      Cell c{crop.x,crop.y,crop.z};
      if(crop.shelter<0) crop.shelter=cropShelter(c);
      bool automatic=(garden.raining() && crop.shelter==0)
        || std::ranges::any_of(farm.sprinklers,[&](glm::ivec3 p) {
          return p.y==c.y && std::abs(p.x-c.x)<=2 && std::abs(p.z-c.z)<=2;
        });
      bool wasWatered=crop.water>0;
      float bonus=automatic ? step : std::min(step,crop.water);
      crop.age=std::min(cropGrowSeconds(crop.kind),crop.age+(step+bonus)*(crop.shelter==2 ? 1.25f : 1.f));
      crop.water=automatic ? cropWaterSeconds : std::max(0.f,crop.water-step);
      auto stage=cropStage(crop.kind,crop.age);
      if(wasWatered!=(crop.water>0)) invalidate(chunkAt(c.x,c.z));
      auto it=edits_.find(c);
      if(it==edits_.end() || it->second==stage) continue;
      it->second=stage;
      if(auto chunk=chunks.find(chunkAt(c.x,c.z));chunk!=chunks.end()) {
        chunk->second.set(localCoord(c.x),c.y,localCoord(c.z),stage); chunk->second.dirty=true;
      }
    }
    seconds=std::max(0.f,seconds-step);
    garden.weather+=step;
    if(garden.weather>=showerPeriod) garden.weather=0;
  }
}
void World::generateStructures(Chunk& chunk) const {
  if(cityOrigin) generateCity(chunk,*cityOrigin);
  if(coastOrigin) generateCoast(chunk,terrain,*coastOrigin);
  if(coastOrigin && harborLots) generateHarbor(chunk,*coastOrigin,*harborLots);
  generateRoad(chunk,road);
  if(countrysideOrigin)generateCountryside(chunk,*countrysideOrigin);
  generateMetropolis(chunk,*this);
  generateEstate(chunk,*this);
}
void World::insert(Chunk chunk) {
  const auto p = chunk.pos;
  generateStructures(chunk);
  for (const auto& [cell, block] : edits_) if (chunkAt(cell.x,cell.z) == p)
    chunk.set(localCoord(cell.x),cell.y,localCoord(cell.z),block);
  chunks.insert_or_assign(p, std::move(chunk));
  for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) invalidate({p.x+dx,p.z+dz});
}
bool World::editedIn(Cell minimum,Cell maximum) const {
  return std::ranges::any_of(edits_,[&](const auto& edit) {
    auto c=edit.first;
    return c.x>=minimum.x && c.x<=maximum.x && c.y>=minimum.y && c.y<=maximum.y && c.z>=minimum.z && c.z<=maximum.z;
  });
}
void World::ensure(ChunkPos p, int radius) {
  for (int z = p.z-radius; z <= p.z+radius; ++z) for (int x = p.x-radius; x <= p.x+radius; ++x)
    if (!chunks.contains({x,z})) insert(terrain.generate({x,z}));
}
int World::streamingRadius(ChunkPos center) const {
  bool coast=coastOrigin && coastContains(*coastOrigin,float(center.x*chunkSize),float(center.z*chunkSize),32);
  bool country=countrysideOrigin && countrysideContains(*countrysideOrigin,float(center.x*chunkSize),float(center.z*chunkSize),64);
  bool metro=metroOrigin && metroContains(*metroOrigin,float(center.x*chunkSize),float(center.z*chunkSize),64);
  bool estate=estateOrigin && estateContains(*estateOrigin,float(center.x*chunkSize),float(center.z*chunkSize),64);
  return coast || country || metro || estate ? coastViewRadius : viewRadius;
}
void World::evict(ChunkPos center, int radius) {
  std::erase_if(chunks, [&](const auto& pair) {
    return std::abs(pair.first.x-center.x) > radius || std::abs(pair.first.z-center.z) > radius;
  });
}
std::optional<RayHit> World::raycast(glm::vec3 origin, glm::vec3 direction, float reach) const {
  const float length = glm::length(direction);
  if (!(length > 0.f) || !std::isfinite(length) || reach < 0.f) return {};
  direction /= length;
  Cell cell{int(std::floor(origin.x)),int(std::floor(origin.y)),int(std::floor(origin.z))}, previous = cell;
  glm::ivec3 step{};
  glm::vec3 next{}, delta{};
  for (int axis = 0; axis < 3; ++axis) {
    step[axis] = direction[axis] > 0.f ? 1 : -1;
    delta[axis] = direction[axis] == 0.f ? std::numeric_limits<float>::infinity() : std::abs(1.f / direction[axis]);
    float edge = std::floor(origin[axis]) + (step[axis] > 0 ? 1.f : 0.f);
    next[axis] = direction[axis] == 0.f ? std::numeric_limits<float>::infinity() : (edge-origin[axis]) / direction[axis];
  }
  float distance = 0.f;
  while (distance <= reach) {
    const auto block=get(cell);
    if (solid(block)) {
      auto box=blockBounds(cell,block);
      float enter=0.f,leave=reach;
      for(int axis=0;axis<3;++axis) {
        if(direction[axis]==0.f) {
          if(origin[axis]<box.min[axis] || origin[axis]>box.max[axis]) leave=-1;
        } else {
          float a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];
          enter=std::max(enter,std::min(a,b)); leave=std::min(leave,std::max(a,b));
        }
      }
      if(enter<=leave) return RayHit{cell,previous,enter};
    }
    const int axis = next.x < next.y ? (next.x < next.z ? 0 : 2) : (next.y < next.z ? 1 : 2);
    previous = cell; distance = next[axis]; next[axis] += delta[axis];
    if (axis == 0) cell.x += step.x;
    if (axis == 1) cell.y += step.y;
    if (axis == 2) cell.z += step.z;
  }
  return {};
}

void World::save(const std::filesystem::path& path, const PlayerPose& player) const {
  if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
  const auto temporary = std::filesystem::path(path.string() + ".tmp");
  std::ofstream file(temporary, std::ios::trunc);
  if (!file) throw std::runtime_error("Cannot open world save: " + temporary.string());
  file << "BLOCKWORLD 21 " << terrain.seed() << ' ' << terrain.adventure() << '\n' << std::setprecision(9)
       << player.position.x << ' ' << player.position.y << ' ' << player.position.z << ' '
       << player.yaw << ' ' << player.pitch << ' ' << player.flying << '\n' << guideFlags << '\n'
       << std::setprecision(17) << clock.phase << ' ' << clock.day << '\n'
       << crafting.wood << ' ' << crafting.planks << ' ' << crafting.stone << ' ' << crafting.flags << '\n'
       << farm.initialized << ' ' << farm.wheat << ' ' << farm.eggs << ' ' << farm.flags << ' '
       << farm.home.x << ' ' << farm.home.y << ' ' << farm.home.z << ' ' << farm.chickens.size() << ' ' << farm.crops.size() << '\n';
  for(std::size_t i=0;i<farm.chickens.size();++i) {
    const auto& c=farm.chickens[i];
    file<<c.position.x<<' '<<c.position.y<<' '<<c.position.z<<' '<<c.yaw<<' '<<c.eggTimer<<' '<<c.eggReady
        <<' '<<c.growth<<' '<<c.mother<<' '<<c.hatchTimer<<' '<<std::quoted(animalName(c,i))<<'\n';
  }
  for(const auto& c : farm.crops) file<<c.x<<' '<<c.y<<' '<<c.z<<' '<<c.age<<' '<<int(c.kind)<<' '<<c.water<<' '<<c.composted<<'\n';
  file<<farm.carrots<<' '<<farm.strawberries<<' '<<farm.pumpkins<<'\n';
  const auto& g=farm.garden;
  file<<g.initialized<<' '<<g.origin.x<<' '<<g.origin.y<<' '<<g.origin.z<<' '<<g.coins<<' '<<g.compost<<' '
      <<g.sprinklers<<' '<<g.greenhouses<<' '<<g.flags<<' '<<g.weather<<'\n';
  file<<farm.milk<<' '<<farm.livestock.size()<<'\n';
  for(const auto& a : farm.livestock)
    file<<int(a.kind)<<' '<<a.position.x<<' '<<a.position.y<<' '<<a.position.z<<' '<<a.home.x<<' '<<a.home.y<<' '<<a.home.z<<' '<<a.yaw<<' '<<a.milkTimer<<'\n';
  const auto& car=farm.car;
  file<<car.owned<<' '<<car.position.x<<' '<<car.position.y<<' '<<car.position.z<<' '<<car.yaw<<'\n';
  const auto castle=castleOrigin.value_or(Cell{});
  file<<castleOrigin.has_value()<<' '<<castle.x<<' '<<castle.y<<' '<<castle.z<<'\n';
  const auto city=cityOrigin.value_or(Cell{});
  file<<cityOrigin.has_value()<<' '<<city.x<<' '<<city.y<<' '<<city.z<<'\n';
  const auto coast=coastOrigin.value_or(Cell{});
  file<<coastOrigin.has_value()<<' '<<coast.x<<' '<<coast.y<<' '<<coast.z<<'\n';
  file<<harborLots.has_value()<<' '<<harborLots.value_or(0)<<'\n';
  file<<road.size()<<'\n';
  for(auto point:road) file<<point.x<<' '<<point.y<<' '<<point.z<<'\n';
  auto country=countrysideOrigin.value_or(Cell{});
  file<<countrysideOrigin.has_value()<<' '<<country.x<<' '<<country.y<<' '<<country.z<<'\n';
  auto metro=metroOrigin.value_or(Cell{});
  file<<metroOrigin.has_value()<<' '<<metro.x<<' '<<metro.y<<' '<<metro.z<<' '<<cityLife.bank<<' '<<cityLife.activeCar<<' '<<cityLife.rentDay;
  for(auto r:cityLife.residents)file<<' '<<r.conversations<<' '<<r.dating;
  file<<' '<<cityLife.statement.size();
  for(auto e:cityLife.statement)file<<' '<<int(e.kind)<<' '<<e.amount<<' '<<e.day;
  for(auto r:cityLife.residents){file<<' '<<r.pregnancyDue;for(auto birth:r.children)file<<' '<<birth;}
  for(auto r:cityLife.residents)for(auto fed:r.lastFed)file<<' '<<fed;
  auto estate=estateOrigin.value_or(Cell{});file<<' '<<estateOrigin.has_value()<<' '<<estate.x<<' '<<estate.y<<' '<<estate.z;
  file<<' '<<cityLife.home;
  for(auto car:cityLife.homeCars)file<<' '<<car;
  for(auto r:cityLife.residents)file<<' '<<r.home;
  for(auto s:cityLife.dataCenters)file<<' '<<s.racks<<' '<<s.power<<' '<<s.cooling<<' '<<s.contracts;
  file<<'\n';
  file<<inventory.selected;
  for(auto item : inventory.slots) file<<' '<<int(item);
  file<<'\n';
  file<<edits_.size()<<'\n';
  std::vector<std::pair<Cell,Block>> sorted(edits_.begin(), edits_.end());
  std::ranges::sort(sorted, [](const auto& a, const auto& b) {
    return std::tie(a.first.x,a.first.y,a.first.z) < std::tie(b.first.x,b.first.y,b.first.z);
  });
  for (const auto& [c,b] : sorted) file << c.x << ' ' << c.y << ' ' << c.z << ' ' << int(b) << '\n';
  file.flush();
  if (!file) throw std::runtime_error("Could not finish writing world save");
  file.close();
  if (!file) throw std::runtime_error("Could not close world save");
  std::filesystem::rename(temporary,path);
}
std::optional<PlayerPose> World::load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return {};
  std::ifstream file(path);
  std::string magic; int version{}; std::uint32_t seed{};
  PlayerPose pose; std::size_t count{};
  auto corrupt = [] { throw std::runtime_error("World save is invalid; it has been left untouched."); };
  int adventure=0; std::uint32_t flags=0;
  WorldClock savedClock;
  CraftState savedCrafting;
  FarmState savedFarm;
  std::optional<Cell> savedCastle,savedCity,savedCoast,savedCountry,savedMetro,savedEstate;
  CityLifeState savedLife;
  std::optional<std::uint32_t> savedHarbor;
  std::vector<glm::vec3> savedRoad;
  if (!(file >> magic >> version >> seed) || magic != "BLOCKWORLD" || version<1 || version>21) corrupt();
  if(version>=2 && (!(file>>adventure) || adventure<0 || adventure>1)) corrupt();
  if (!(file >> pose.position.x >> pose.position.y >> pose.position.z >> pose.yaw >> pose.pitch >> pose.flying)) corrupt();
  if(version>=2 && (!(file>>flags) || flags>(version==2 ? 63u : 127u))) corrupt();
  std::uint64_t savedDay=1;
  if(version>=3) {
    if(!(file>>savedClock.phase>>savedDay) || !std::isfinite(savedClock.phase) || savedClock.phase<0 || savedClock.phase>=1
       || savedDay==0 || savedDay>std::numeric_limits<std::uint32_t>::max()) corrupt();
    savedClock.day=std::uint32_t(savedDay);
  }
  if(version>=4) {
    if(!(file>>savedCrafting.wood>>savedCrafting.planks>>savedCrafting.stone>>savedCrafting.flags)
       || savedCrafting.flags>127) corrupt();
    for(int amount : {savedCrafting.wood,savedCrafting.planks,savedCrafting.stone})
      if(amount<0 || amount>CraftState::capacity) corrupt();
  }
  if(version>=5) {
    std::size_t chickens=0,crops=0;
    if(!(file>>savedFarm.initialized>>savedFarm.wheat>>savedFarm.eggs>>savedFarm.flags
         >>savedFarm.home.x>>savedFarm.home.y>>savedFarm.home.z>>chickens>>crops)
       || savedFarm.wheat<0 || savedFarm.wheat>9999 || savedFarm.eggs<0 || savedFarm.eggs>9999
       || savedFarm.flags>(version>=7 ? 255u : 31u) || chickens>(version>=7 ? flockLimit : starterFlock) || crops>cropLimit) corrupt();
    auto validPosition=[](glm::vec3 p) {
      return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
        && std::abs(p.x)<=coordinateLimit-2 && std::abs(p.z)<=coordinateLimit-2 && p.y>=0 && p.y<=256;
    };
    if(!validPosition(savedFarm.home) || savedFarm.home.y<1 || savedFarm.home.y>=worldHeight) corrupt();
    for(std::size_t i=0;i<chickens;++i) {
      Chicken c;
      if(!(file>>c.position.x>>c.position.y>>c.position.z>>c.yaw>>c.eggTimer>>c.eggReady)
         || !validPosition(c.position) || !std::isfinite(c.yaw) || std::abs(c.yaw)>6.284f
         || !std::isfinite(c.eggTimer) || c.eggTimer < -1 || c.eggTimer>eggLaySeconds
         || (c.eggReady && c.eggTimer!=-1)) corrupt();
      if(version>=7) {
        if(!(file>>c.growth>>c.mother>>c.hatchTimer>>std::quoted(c.name))
           || !std::isfinite(c.growth) || c.growth<0 || c.growth>chickGrowSeconds
           || !std::isfinite(c.hatchTimer) || (c.hatchTimer<0 && c.hatchTimer!=-1) || c.hatchTimer>eggHatchSeconds
           || c.mother < -1 || (c.mother>=0 && std::size_t(c.mother)>=i) || !validAnimalName(c.name)
           || (isChick(c) && (c.mother<0 || c.eggReady || c.eggTimer>=0 || c.hatchTimer>=0))) corrupt();
        if(c.mother>=0 && isChick(savedFarm.chickens[std::size_t(c.mother)])) corrupt();
      } else c.name=animalName(c,i);
      savedFarm.chickens.push_back(c);
    }
    if(chickens+std::ranges::count_if(savedFarm.chickens,[](const auto& c){return c.hatchTimer>=0;})>flockLimit) corrupt();
    std::unordered_set<Cell,PositionHash> planted;
    for(std::size_t i=0;i<crops;++i) {
      Crop c;
      if(!(file>>c.x>>c.y>>c.z>>c.age)) corrupt();
      if(version>=8) {
        int kind=-1;
        if(!(file>>kind>>c.water) || kind<0 || kind>=int(CropKind::Count)) corrupt();
        c.kind=CropKind(kind);
      }
      if(version>=9 && !(file>>c.composted)) corrupt();
      if(!validCell({c.x,c.y,c.z}) || !std::isfinite(c.age) || c.age<0 || c.age>cropGrowSeconds(c.kind)
         || !std::isfinite(c.water) || c.water<0 || c.water>cropWaterSeconds || !planted.insert({c.x,c.y,c.z}).second) corrupt();
      savedFarm.crops.push_back(c);
    }
  }
  if(version>=8) {
    if(!(file>>savedFarm.carrots>>savedFarm.strawberries>>savedFarm.pumpkins)) corrupt();
    for(int amount : {savedFarm.carrots,savedFarm.strawberries,savedFarm.pumpkins}) if(amount<0 || amount>9999) corrupt();
  }
  if(version>=9) {
    auto& g=savedFarm.garden;
    if(!(file>>g.initialized>>g.origin.x>>g.origin.y>>g.origin.z>>g.coins>>g.compost>>g.sprinklers>>g.greenhouses>>g.flags>>g.weather)
       || !validCell({g.origin.x,g.origin.y,g.origin.z}) || std::abs(g.origin.x)>coordinateLimit-9 || std::abs(g.origin.z)>coordinateLimit-9
       || g.origin.y>worldHeight-5 || g.coins<0 || g.coins>coinLimit || g.flags>63
       || !std::isfinite(g.weather) || g.weather<0 || g.weather>=showerPeriod) corrupt();
    for(int amount : {g.compost,g.sprinklers,g.greenhouses}) if(amount<0 || amount>gardenSupplyLimit) corrupt();
  }
  if(version>=10) {
    std::size_t animals=0;
    auto validPosition=[](glm::vec3 p) {
      return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
        && std::abs(p.x)<=coordinateLimit-2 && std::abs(p.z)<=coordinateLimit-2 && p.y>=0 && p.y<=256;
    };
    if(!(file>>savedFarm.milk>>animals) || savedFarm.milk<0 || savedFarm.milk>9999 || animals>(version>=11 ? livestockLimit : 8)) corrupt();
    for(std::size_t i=0;i<animals;++i) {
      Livestock a; int kind=-1;
      if(!(file>>kind>>a.position.x>>a.position.y>>a.position.z>>a.home.x>>a.home.y>>a.home.z>>a.yaw>>a.milkTimer)
          || kind<0 || kind>(version>=11 ? 3 : 1) || !validPosition(a.position) || !validPosition(a.home)
          || !std::isfinite(a.yaw) || std::abs(a.yaw)>6.284f || !std::isfinite(a.milkTimer) || a.milkTimer<0 || a.milkTimer>milkSeconds) corrupt();
      a.kind=LivestockKind(kind); savedFarm.livestock.push_back(a);
    }
    auto& car=savedFarm.car;
    if(!(file>>car.owned>>car.position.x>>car.position.y>>car.position.z>>car.yaw)
        || !validPosition(car.position) || !std::isfinite(car.yaw) || std::abs(car.yaw)>6.284f) corrupt();
  }
  if(version>=11) {
    bool present=false; Cell c;
    if(!(file>>present>>c.x>>c.y>>c.z)) corrupt();
    if(present) {
      if(!validCell(c) || std::abs(c.x)>coordinateLimit-50 || std::abs(c.z)>coordinateLimit-50 || c.y>worldHeight-18) corrupt();
      savedCastle=c;
    } else if(c!=Cell{}) corrupt();
  }
  if(version>=12) {
    bool present=false; Cell c;
    if(!(file>>present>>c.x>>c.y>>c.z)) corrupt();
    if(present) {
      if(!validCell(c) || c.x%chunkSize!=0 || c.z%chunkSize!=0 || c.y!=16
          || std::abs(c.x)>coordinateLimit-citySize || std::abs(c.z)>coordinateLimit-citySize) corrupt();
      savedCity=c;
    } else if(c!=Cell{}) corrupt();
  }
  if(version>=13) {
    bool present=false; Cell c;
    if(!(file>>present>>c.x>>c.y>>c.z)) corrupt();
    if(present) {
      if(!validCell(c) || c.x%chunkSize!=0 || c.z%chunkSize!=0 || c.y!=coastSeaLevel
          || std::abs(c.x)>coordinateLimit-coastSize || std::abs(c.z)>coordinateLimit-coastSize) corrupt();
      if(savedCity && c.x<savedCity->x+citySize && c.x+coastSize>savedCity->x
          && c.z<savedCity->z+citySize && c.z+coastSize>savedCity->z) corrupt();
      if(savedCastle && c.x<savedCastle->x+50 && c.x+coastSize>savedCastle->x
          && c.z<savedCastle->z+50 && c.z+coastSize>savedCastle->z) corrupt();
      savedCoast=c;
    } else if(c!=Cell{}) corrupt();
  }
  if(version>=14) {
    bool present=false; std::uint32_t mask=0;
    if(!(file>>present>>mask) || mask>=(1u<<harborBuildings().size()) || (present && !savedCoast) || (!present && mask)) corrupt();
    if(present) savedHarbor=mask;
  }
  if(version>=15) {
    std::size_t points=0;
    if(!(file>>points) || points>roadPointLimit || points==1 || (points && !savedCoast)) corrupt();
    float length=0;
    for(std::size_t i=0;i<points;++i) {
      glm::vec3 p;
      if(!(file>>p.x>>p.y>>p.z) || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)
          || std::abs(p.x)>coordinateLimit-16 || std::abs(p.z)>coordinateLimit-16 || p.y<4 || p.y>worldHeight-8) corrupt();
      if(i) {
        auto d=p-savedRoad.back(); float segment=glm::length(glm::vec2(d.x,d.z));
        if(segment<.09f || segment>128 || std::abs(d.y)>segment*.14f) corrupt();
        length+=segment; if(length>8192) corrupt();
      }
      savedRoad.push_back(p);
    }
  }
  if(version>=16) {
    bool present=false;Cell c;
    if(!(file>>present>>c.x>>c.y>>c.z))corrupt();
    if(present) {
      if(!validCell(c) || c.y<4 || c.y>worldHeight-20 || std::abs(c.x)>coordinateLimit-countrysideWidth-4
          || std::abs(c.z)>coordinateLimit-countrysideDepth-24 || savedRoad.empty())corrupt();
      savedCountry=c;
    } else if(c!=Cell{})corrupt();
  }
  if(version>=17) {
    bool present=false;Cell c;
    if(!(file>>present>>c.x>>c.y>>c.z>>savedLife.bank>>savedLife.activeCar>>savedLife.rentDay)
       || savedLife.bank<0 || savedLife.bank>bankLimit || savedLife.activeCar<0 || savedLife.activeCar>=garageSize || savedLife.rentDay>savedClock.day)corrupt();
    if(present) {
      if(!validCell(c) || c.y!=harborGround || std::abs(c.x)>coordinateLimit-metroWidth-16
          || std::abs(c.z)>coordinateLimit-metroDepth-16 || !savedCoast || savedRoad.empty() || savedLife.rentDay<1)corrupt();
      savedMetro=c;
    } else if(c!=Cell{})corrupt();
    for(auto& r:savedLife.residents)
      if(!(file>>r.conversations>>r.dating) || r.conversations<0 || r.conversations>3 || (r.dating && r.conversations<2))corrupt();
    int entries=0;if(!(file>>entries) || entries<0 || entries>8)corrupt();
    std::uint32_t previousDay=0;
    for(int i=0;i<entries;++i) {
      int kind=0;BankEntry e;
      if(!(file>>kind>>e.amount>>e.day) || kind<0 || kind>(version>=21 ? 4 : 3) || e.amount<=0 || e.amount>bankLimit || e.day<1 || e.day>savedClock.day || e.day<previousDay)corrupt();
      e.kind=BankKind(kind);savedLife.statement.push_back(e);previousDay=e.day;
    }
  }
  if(version>=18)for(auto& r:savedLife.residents) {
    if(!(file>>r.pregnancyDue) || std::uint64_t(r.pregnancyDue)>std::uint64_t(savedClock.day)+2 || (r.pregnancyDue && !r.dating))corrupt();
    bool ended=false;std::uint32_t previousBirth=0;int children=0;
    for(auto& birth:r.children) {
      if(!(file>>birth) || birth>savedClock.day || (birth && (!r.dating || ended || birth<previousBirth)))corrupt();
      if(birth){previousBirth=birth;++children;}else ended=true;
    }
    if(r.pregnancyDue && (children==3 || r.pregnancyDue<=previousBirth))corrupt();
  }
  if(version>=19)for(auto& r:savedLife.residents)for(int c=0;c<3;++c) {
    double& fed=r.lastFed[c];double now=(double(savedClock.day)+savedClock.phase)*WorldClock::daySeconds;
    if(!(file>>fed) || !std::isfinite(fed) || fed<0 || fed>now+.000001 || (fed>0 && (!r.children[c] || fed<double(r.children[c])*WorldClock::daySeconds)))corrupt();
  }
  if(version>=20) {
    bool present=false;Cell c{};if(!(file>>present>>c.x>>c.y>>c.z))corrupt();
    if(present) {if(!savedMetro || c.y!=23 || !validCell(c) || std::abs(c.x)>coordinateLimit-estateWidth-32 || std::abs(c.z)>coordinateLimit-estateDepth-32 || c.x<savedMetro->x+metroWidth+32 || c.z!=savedMetro->z)corrupt();savedEstate=c;}
    else if(c!=Cell{})corrupt();
  }
  if(version>=21) {
    auto validHome=[&](int h){return h>=-1 && h<=2 && (h<0 || savedEstate.has_value());};
    if(!(file>>savedLife.home) || !validHome(savedLife.home))corrupt();
    std::array<bool,garageSize> parked{};
    for(auto& car:savedLife.homeCars) {
      if(!(file>>car) || car<-1 || car>=garageSize || (car>=0 && (!savedEstate || parked[car])))corrupt();
      if(car>=0)parked[car]=true;
    }
    std::array<bool,3> occupied{};
    for(auto& r:savedLife.residents) {
      if(!(file>>r.home) || !validHome(r.home) || (r.home>=0 && (!r.dating || occupied[r.home])))corrupt();
      if(r.home>=0)occupied[r.home]=true;
    }
    for(auto& s:savedLife.dataCenters) {
      if(!(file>>s.racks>>s.power>>s.cooling>>s.contracts) || s.racks<2 || s.racks>16 || s.power<1 || s.power>4
        || s.cooling<1 || s.cooling>4 || s.contracts<1 || s.contracts>15 || !(s.contracts&1))corrupt();
      auto r=dataCenterReport(s);if(r.used>s.racks || s.racks>r.capacity)corrupt();
    }
  }
  auto savedInventory=startingInventory(savedCrafting);
  if(version>=6) {
    if(!(file>>savedInventory.selected) || savedInventory.selected<0 || savedInventory.selected>=hotbarSize) corrupt();
    for(auto& item : savedInventory.slots) {
      int value=-1;
      if(!(file>>value) || value<0 || value>=(version>=14 ? int(Item::Count) : version>=12 ? int(Item::BlueGlass) : version==11 ? int(Item::Concrete) : version>=9 ? int(Item::StoneSlab) : version==8 ? int(Item::Hoe) : int(Item::Carrot)) || !itemAvailable(Item(value),savedCrafting)) corrupt();
      item=Item(value);
    }
  }
  if(!(file>>count)) corrupt();
  if (count > 1000000 || !std::isfinite(pose.yaw) || !std::isfinite(pose.pitch) || std::abs(pose.pitch) > 1.56f) corrupt();
  for (int axis = 0; axis < 3; ++axis) if (!std::isfinite(pose.position[axis]) || std::abs(pose.position[axis]) > coordinateLimit) corrupt();
  decltype(edits_) edits;
  for (std::size_t i=0; i<count; ++i) {
    Cell c; int b{};
    if (!(file >> c.x >> c.y >> c.z >> b) || !validCell(c) || b < 0 || b==int(Block::Bedrock)
        || b >= (version==1 ? int(Block::Bedrock) : version==2 ? int(Block::BedZ) : version==3 ? int(Block::Workbench)
                 : version==4 ? int(Block::Fence) : version<8 ? int(Block::CarrotYoung) : version==8 ? int(Block::Farmland) : version<11 ? int(Block::StoneSlab) : version==11 ? int(Block::Concrete) : version<14 ? int(Block::BlueGlass) : int(Block::Count))) corrupt();
    if (!edits.emplace(c,static_cast<Block>(b)).second) corrupt();
    if(Block(b)==Block::Sprinkler) savedFarm.sprinklers.push_back({c.x,c.y,c.z});
  }
  if(savedFarm.sprinklers.size()>sprinklerLimit) corrupt();
  if(version>=5) {
    std::size_t planted=0;
    for(const auto& [cell,block] : edits) if(isCrop(block)) ++planted;
    if(planted!=savedFarm.crops.size()) corrupt();
    for(const auto& c : savedFarm.crops) {
      auto it=edits.find({c.x,c.y,c.z});
      if(it==edits.end() || it->second!=cropStage(c.kind,c.age)) corrupt();
    }
  }
  file >> std::ws;
  if (!file.eof()) corrupt();
  terrain = Terrain(seed,adventure!=0); guideFlags=flags; clock=savedClock; crafting=savedCrafting; inventory=savedInventory;
  farm=std::move(savedFarm); castleOrigin=savedCastle; cityOrigin=savedCity; coastOrigin=savedCoast; harborLots=savedHarbor;
  road=std::move(savedRoad); countrysideOrigin=savedCountry;metroOrigin=savedMetro;estateOrigin=savedEstate;cityLife=std::move(savedLife); edits_ = std::move(edits); chunks.clear();
  return pose;
}

ChunkWorker::ChunkWorker(Terrain terrain) : terrain_(terrain), thread_([this] {
  while (true) {
    ChunkPos pos;
    {
      std::unique_lock lock(mutex_);
      ready_.wait(lock, [&] { return stopping_ || !pending_.empty(); });
      if (stopping_) return;
      pos = pending_.front(); pending_.pop_front();
    }
    auto chunk = terrain_.generate(pos);
    std::lock_guard lock(mutex_);
    completed_.push_back(std::move(chunk));
  }
}) {}
ChunkWorker::~ChunkWorker() {
  { std::lock_guard lock(mutex_); stopping_ = true; }
  ready_.notify_one(); thread_.join();
}
void ChunkWorker::request(ChunkPos center, const World& world) {
  std::lock_guard lock(mutex_);
  for (auto p : pending_) requested_.erase(p);
  pending_.clear();
  std::vector<ChunkPos> wanted;
  int radius=world.streamingRadius(center);
  for (int z=-radius;z<=radius;++z) for (int x=-radius;x<=radius;++x) {
    ChunkPos p{center.x+x,center.z+z};
    if (!world.chunks.contains(p) && !requested_.contains(p)) wanted.push_back(p);
  }
  std::ranges::sort(wanted,[&](ChunkPos a,ChunkPos b) {
    auto distance = [&](ChunkPos p) { return (p.x-center.x)*(p.x-center.x)+(p.z-center.z)*(p.z-center.z); };
    return distance(a)<distance(b);
  });
  for (auto p : wanted) { requested_.insert(p); pending_.push_back(p); }
  ready_.notify_one();
}
void ChunkWorker::collect(World& world, ChunkPos center) {
  std::deque<Chunk> completed;
  { std::lock_guard lock(mutex_); completed.swap(completed_);
    for (const auto& c : completed) requested_.erase(c.pos);
  }
  int radius=world.streamingRadius(center);
  for (auto& c : completed) if (std::abs(c.pos.x-center.x)<=radius+1 && std::abs(c.pos.z-center.z)<=radius+1
    && !world.chunks.contains(c.pos)) world.insert(std::move(c));
}

void appendBox(std::vector<Vertex>& vertices,Box box,Cell cell,float material,float light) {
  constexpr std::array<glm::vec3,6> normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
  constexpr std::array<glm::vec3,6> us{{{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}}};
  constexpr std::array<glm::vec3,6> vs{{{0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0}}};
  constexpr std::array<glm::vec2,4> corners{{{0,0},{1,0},{1,1},{0,1}}};
  glm::vec3 center=(box.min+box.max)*.5f,size=box.max-box.min,origin(cell.x,cell.y,cell.z);
  for(int face=0;face<6;++face) {
    std::array<Vertex,4> quad;
    for(int i=0;i<4;++i) quad[i]={center+size*(normals[face]*.5f+us[face]*(corners[i].x-.5f)+vs[face]*(corners[i].y-.5f)),
                                  corners[i],material,light,origin};
    for(int i : {0,1,2,0,2,3}) vertices.push_back(quad[i]);
  }
}
std::vector<Vertex> buildPreview(Cell cell,Block block,float material,bool exact) {
  std::vector<Vertex> vertices;
  glm::vec3 p(cell.x,cell.y,cell.z);
  Box box{p,p+glm::vec3(1,isDoor(block) ? 2 : 1,1)};
  if(exact) {
    box=blockBounds(cell,block);
    if(isDoor(block)) box.max.y=p.y+2;
  }
  if(isBed(block)) {
    auto d=bedOffset(block); glm::vec3 offset(d.x,d.y,d.z);
    box={glm::min(p,p+offset),glm::max(p,p+offset)+glm::vec3(1,.63f,1)};
  }
  appendBox(vertices,{box.min-glm::vec3(.012f),box.max+glm::vec3(.012f)},cell,material,1.f);
  return vertices;
}
std::vector<Vertex> buildMesh(const World& world, const Chunk& chunk) {
  // Each face uses a right-handed tangent basis: cross(U,V) = outward normal.
  constexpr std::array<Cell,6> us{{{0,0,-1},{0,0,1},{1,0,0},{1,0,0},{1,0,0},{-1,0,0}}};
  constexpr std::array<Cell,6> vs{{{0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0}}};
  constexpr std::array<float,6> shades{.78f,.62f,1.f,.48f,.84f,.70f};
  constexpr std::array<glm::vec2,4> corners{{{0,0},{1,0},{1,1},{0,1}}};
  std::vector<Vertex> vertices;
  vertices.reserve(12000);
  std::array<int,chunkSize*chunkSize> surface{};
  for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x) {
    int wx=chunk.pos.x*chunkSize+x,wz=chunk.pos.z*chunkSize+z;
    auto city=world.cityOrigin;
    bool inCity=city && wx>=city->x && wx<city->x+citySize && wz>=city->z && wz<city->z+citySize;
    // The city is graded below some original hills; its streets aren't caves.
    surface[z*chunkSize+x]=world.coastOrigin && world.harborLots && harborGraded(*world.coastOrigin,*world.harborLots,wx,wz) ? harborGround-1 : world.coastOrigin && coastContains(*world.coastOrigin,float(wx),float(wz))
      ? coastColumn(world.terrain,*world.coastOrigin,wx,wz).ground : inCity ? city->y-1 : world.terrain.height(wx,wz);
    auto road=sampleRoad(world.road,wx+.5f,wz+.5f);
    if(road.distance<=roadHalfWidth+1) surface[z*chunkSize+x]=int(std::round(road.height)) - 1;
    if(world.countrysideOrigin) {
      auto o=*world.countrysideOrigin;bool site=wx>=o.x && wx<o.x+countrysideWidth && wz>=o.z && wz<o.z+countrysideDepth;
      bool approach=wx>=o.x+51 && wx<=o.x+61 && wz>=o.z-24 && wz<o.z;
      if(site || approach)surface[z*chunkSize+x]=o.y-1;
    }
    if(world.estateOrigin && estateContains(*world.estateOrigin,float(wx),float(wz)))surface[z*chunkSize+x]=float(wz-world.estateOrigin->z)>=310 ? 14 : 22;
    if(world.metroOrigin && metroContains(*world.metroOrigin,float(wx),float(wz)))surface[z*chunkSize+x]=harborGround-1;
  }
  for (int y=0;y<worldHeight;++y) for (int z=0;z<chunkSize;++z) for (int x=0;x<chunkSize;++x) {
    Block block = chunk.get(x,y,z);
    if (!solid(block)) continue;
    Cell c{chunk.pos.x*chunkSize+x,y,chunk.pos.z*chunkSize+z};
    float daylight=y<surface[z*chunkSize+x]-2 ? .20f : 1.f;
    if(block==Block::Water) {
      int floor=y-1;
      while(floor>0 && world.get({c.x,floor,c.z})==Block::Water) --floor;
      daylight=float(y-floor);
    }
    if(isFurniture(block)) {
      glm::vec3 p(c.x,c.y,c.z);
      auto part=[&](glm::vec3 lo,glm::vec3 hi,float material) { appendBox(vertices,{p+lo,p+hi},c,material,daylight); };
      if(block==Block::Sofa) {
        part({.04f,.08f,.1f},{.96f,.24f,.9f},64);
        part({.025f,.24f,.08f},{.975f,.48f,.91f},73);
        part({0,.4f,.75f},{1,.88f,.94f},73);
        if(world.get(c+Cell{-1,0,0})!=Block::Sofa) part({0,.28f,.08f},{.16f,.65f,.9f},73);
        if(world.get(c+Cell{1,0,0})!=Block::Sofa) part({.84f,.28f,.08f},{1,.65f,.9f},73);
        part({.25f,.49f,.6f},{.7f,.75f,.78f},63);
      } else if(block==Block::Table) {
        part({.05f,.48f,.05f},{.95f,.58f,.95f},72);
        for(float x : {.15f,.77f}) for(float z : {.15f,.77f}) part({x,0,z},{x+.08f,.48f,z+.08f},64);
      } else if(block==Block::Chair) {
        part({.16f,.4f,.12f},{.84f,.5f,.88f},73);
        part({.16f,.5f,.76f},{.84f,.94f,.88f},72);
        for(float x : {.2f,.72f}) for(float z : {.18f,.76f}) part({x,0,z},{x+.08f,.4f,z+.08f},72);
      } else {
        part({.25f,0,.25f},{.75f,.43f,.75f},62);
        part({.21f,.36f,.21f},{.79f,.47f,.79f},60);
        part({.46f,.43f,.46f},{.54f,.75f,.54f},5);
        part({.16f,.59f,.16f},{.84f,.84f,.84f},6);
        part({.28f,.8f,.28f},{.72f,.92f,.72f},6);
        for(auto blossom : {glm::vec3{.25f,.82f,.25f},glm::vec3{.56f,.88f,.48f},glm::vec3{.3f,.83f,.65f}})
          part(blossom,blossom+glm::vec3(.17f,.09f,.17f),70);
      }
      continue;
    }
    if(block==Block::StoneSlab) {
      auto first=vertices.size();
      appendBox(vertices,blockBounds(c,block),c,3,daylight);
      for(auto i=first;i<vertices.size();++i) vertices[i].light*=shades[(i-first)/6];
      continue;
    }
    if(block==Block::Sprinkler) {
      glm::vec3 p(c.x,c.y,c.z);
      appendBox(vertices,{p+glm::vec3(.28f,0,.28f),p+glm::vec3(.72f,.08f,.72f)},c,3,daylight);
      appendBox(vertices,{p+glm::vec3(.45f,.08f,.45f),p+glm::vec3(.55f,.54f,.55f)},c,42,daylight);
      appendBox(vertices,{p+glm::vec3(.18f,.48f,.44f),p+glm::vec3(.82f,.59f,.56f)},c,42,daylight);
      for(float offset : {.22f,.72f}) appendBox(vertices,{p+glm::vec3(offset,.60f,.48f),p+glm::vec3(offset+.06f,.65f,.54f)},c,37,daylight);
      continue;
    }
    if(block==Block::Farmland) {
      // Thin wooden edges join neighbouring soil tiles into readable garden beds.
      auto p=glm::vec3(c.x,c.y,c.z);
      for(Cell n : {Cell{-1,0,0},Cell{1,0,0},Cell{0,0,-1},Cell{0,0,1}}) if(world.get(c+n)!=Block::Farmland) {
        glm::vec3 lo(0,.93f,0),hi(1,1.045f,1);
        if(n.x<0) hi.x=.065f; if(n.x>0) lo.x=.935f;
        if(n.z<0) hi.z=.065f; if(n.z>0) lo.z=.935f;
        appendBox(vertices,{p+lo,p+hi},c,7,daylight);
      }
    }
    if(block==Block::Fence) {
      glm::vec3 p(c.x,c.y,c.z);
      appendBox(vertices,{p+glm::vec3(.39f,0,.39f),p+glm::vec3(.61f,1,.61f)},c,7,daylight*.85f);
      for(Cell n : {Cell{-1,0,0},Cell{1,0,0},Cell{0,0,-1},Cell{0,0,1}}) {
        auto neighbor=world.get(c+n);
        if(neighbor!=Block::Fence && !isGate(neighbor) && !opaque(neighbor)) continue;
        for(float h : {.32f,.72f}) {
          glm::vec3 lo(.45f,h,.45f),hi(.55f,h+.14f,.55f);
          if(n.x<0) lo.x=0; if(n.x>0) hi.x=1; if(n.z<0) lo.z=0; if(n.z>0) hi.z=1;
          appendBox(vertices,{p+lo,p+hi},c,7,daylight*.85f);
        }
      }
      continue;
    }
    if(isGate(block)) {
      auto box=blockBounds(c,block); bool narrowX=box.max.x-box.min.x<.5f;
      for(float t : {0.f,.84f}) {
        auto lo=box.min,hi=box.max;
        if(narrowX) { lo.z+=t; hi.z=lo.z+.16f; } else { lo.x+=t; hi.x=lo.x+.16f; }
        appendBox(vertices,{lo,hi},c,7,daylight*.85f);
      }
      for(float h : {.32f,.72f}) {
        auto lo=box.min,hi=box.max; lo.y+=h; hi.y=lo.y+.14f;
        appendBox(vertices,{lo,hi},c,7,daylight*.9f);
      }
      auto latch=(box.min+box.max)*.5f; latch.y=float(c.y)+.59f;
      if(narrowX) latch.x=box.max.x; else latch.z=box.max.z;
      appendBox(vertices,{latch-glm::vec3(.045f,.045f,.045f),latch+glm::vec3(.045f,.045f,.045f)},c,27,daylight);
      continue;
    }
    if(isCrop(block)) {
      glm::vec3 p(c.x,c.y,c.z);
      const auto* crop=world.cropAt(c);
      bool wet=crop && crop->water>0;
      if(world.get(c+Cell{0,-1,0})!=Block::Farmland)
        appendBox(vertices,{p+glm::vec3(.07f,.006f,.07f),p+glm::vec3(.93f,.035f,.93f)},c,wet ? 33.f : 40.f,daylight);
      if(crop && crop->composted) for(float x : {.18f,.48f,.78f}) for(float z : {.16f,.82f})
        appendBox(vertices,{p+glm::vec3(x,.013f,z),p+glm::vec3(x+.06f,.045f,z+.045f)},c,25,daylight);
      if(!isWheat(block)) {
        auto kind=cropKind(block); int stage=(int(block)-int(Block::WheatYoung))%3;
        float leaf=wet ? 39.f : 24.f;
        auto box=[&](glm::vec3 lo,glm::vec3 hi,float material){appendBox(vertices,{p+lo,p+hi},c,material,daylight);};
        if(kind==CropKind::Carrot) {
          for(glm::vec2 root : {glm::vec2(.27f,.28f),{.72f,.38f},{.45f,.74f}}) {
            float x=root.x,z=root.y,h=.20f+stage*.13f;
            if(stage>0) box({x-.075f,.035f,z-.075f},{x+.075f,.09f+stage*.08f,z+.075f},34);
            box({x-.025f,.06f,z-.025f},{x+.025f,h,z+.025f},leaf);
            box({x-.13f,h*.6f,z-.035f},{x+.13f,h*.6f+.045f,z+.035f},leaf);
            box({x-.035f,h*.77f,z-.12f},{x+.035f,h*.77f+.04f,z+.12f},leaf);
          }
        } else if(kind==CropKind::Strawberry) {
          float h=stage==0 ? .12f : .36f;
          for(glm::vec2 stem : {glm::vec2(.30f,.30f),{.65f,.32f},{.49f,.66f}}) {
            float x=stem.x,z=stem.y;
            box({x-.027f,.03f,z-.027f},{x+.027f,h+.08f,z+.027f},leaf);
            box({x-.14f,h-.025f,z-.12f},{x+.14f,h+.03f,z+.12f},leaf);
            if(stage==1) box({x-.06f,h+.045f,z-.06f},{x+.06f,h+.08f,z+.06f},38);
            if(stage==2) {
              box({x-.085f,.08f,z+.09f},{x+.085f,.26f,z+.23f},35);
              box({x-.045f,.045f,z+.12f},{x+.045f,.09f,z+.20f},35);
              box({x-.09f,.26f,z+.10f},{x+.09f,.285f,z+.22f},leaf);
            }
          }
        } else {
          box({.18f,.055f,.43f},{.82f,.095f,.51f},leaf);
          box({.28f,.07f,.21f},{.33f,.12f,.71f},leaf);
          box({.12f,.12f,.20f},{.39f,.16f,.42f},leaf);
          box({.64f,.10f,.58f},{.87f,.15f,.80f},leaf);
          if(stage>0) {
            float size=stage==2 ? .66f : .31f,center=.53f;
            box({center-size*.5f,.05f,center-size*.5f},{center+size*.5f,.05f+size,center+size*.5f},stage==2 ? 36.f : 24.f);
            box({center-.045f,size+.045f,center-.045f},{center+.045f,size+.18f,center+.045f},5);
          } else box({.48f,.06f,.48f},{.53f,.20f,.53f},leaf);
        }
        continue;
      }
    }
    if(isWheat(block)) {
      glm::vec3 p(c.x,c.y,c.z); float height=blockBounds(c,block).max.y-p.y;
      auto crop=world.cropAt(c); float leaf=crop && crop->water>0 ? 39.f : 24.f;
      for(glm::vec2 stem : {glm::vec2(.25f,.25f),{.72f,.32f},{.44f,.65f},{.74f,.76f},{.23f,.75f}}) {
        float sx=stem.x,sz=stem.y;
        appendBox(vertices,{p+glm::vec3(sx-.022f,0,sz-.022f),p+glm::vec3(sx+.022f,height,sz+.022f)},c,leaf,daylight);
        appendBox(vertices,{p+glm::vec3(sx-.11f,height*.35f,sz-.027f),p+glm::vec3(sx+.11f,height*.46f,sz+.027f)},c,leaf,daylight);
        if(block!=Block::WheatYoung)
          appendBox(vertices,{p+glm::vec3(sx-.056f,height*.68f,sz-.056f),p+glm::vec3(sx+.056f,height,sz+.056f)},c,block==Block::WheatRipe ? 25 : leaf,daylight);
      }
      continue;
    }
    if(block==Block::Torch) {
      glm::vec3 p(c.x,c.y,c.z);
      appendBox(vertices,{p+glm::vec3(.44f,0,.44f),p+glm::vec3(.56f,.55f,.56f)},c,13.f,daylight);
      appendBox(vertices,{p+glm::vec3(.38f,.49f,.38f),p+glm::vec3(.62f,.78f,.62f)},c,14.f,1.f);
      continue;
    }
    if(isDoor(block)) { appendBox(vertices,blockBounds(c,block),c,doorUpper(block) ? 16.f : 15.f,daylight*.87f); continue; }
    if(isBed(block)) {
      glm::vec3 p(c.x,c.y,c.z);
      appendBox(vertices,{p+glm::vec3(.03f,.17f,.03f),p+glm::vec3(.97f,.30f,.97f)},c,7.f,daylight*.82f);
      appendBox(vertices,{p+glm::vec3(.04f,.30f,.02f),p+glm::vec3(.96f,.55f,.98f)},c,17.f,daylight);
      for(float bx : {.08f,.80f}) for(float bz : {.08f,.80f})
        appendBox(vertices,{p+glm::vec3(bx,0,bz),p+glm::vec3(bx+.12f,.20f,bz+.12f)},c,5.f,daylight*.72f);
      if(bedHead(block)) {
        auto low=bedAlongX(block) ? glm::vec3(.52f,.55f,.13f) : glm::vec3(.13f,.55f,.08f);
        auto high=bedAlongX(block) ? glm::vec3(.92f,.63f,.87f) : glm::vec3(.87f,.63f,.48f);
        appendBox(vertices,{p+low,p+high},c,18.f,daylight);
      }
      continue;
    }
    for (int face=0;face<6;++face) {
      Cell n = neighbors[face], u = us[face], v = vs[face];
      Block neighbor=world.get(c+n);
      if (opaque(neighbor) || ((block==Block::Glass || block==Block::Water) && neighbor==block)) continue;
      float material = float(static_cast<int>(block));
      if(isCityMaterial(block)) material=cityMaterial(block);
      if(block==Block::Glass) material=12.f;
      if(block==Block::Workbench) material=face==2 ? 21.f : 19.f;
      if(block==Block::Farmland) {
        auto crop=world.cropAt(c+Cell{0,1,0});
        material=face==2 ? (crop && crop->water>0 ? 41.f : 40.f) : 2.f;
      }
      if (block == Block::Grass) material = face == 2 ? 1.f : (face == 3 ? 2.f : 10.f);
      if (block == Block::Wood && (face==2 || face==3)) material = 11.f;
      std::array<Vertex,4> quad;
      for (int corner=0;corner<4;++corner) {
        auto uv = corners[corner];
        int su = uv.x == 0 ? -1 : 1, sv = uv.y == 0 ? -1 : 1;
        Cell a{u.x*su,u.y*su,u.z*su}, b{v.x*sv,v.y*sv,v.z*sv};
        int s1=opaque(world.get(c+n+a)), s2=opaque(world.get(c+n+b)), diagonal=opaque(world.get(c+n+a+b));
        int occlusion = s1 && s2 ? 3 : s1+s2+diagonal;
        glm::vec3 normal(n.x,n.y,n.z), tangent(u.x,u.y,u.z), bitangent(v.x,v.y,v.z), origin(c.x,c.y,c.z);
        quad[corner] = {origin+glm::vec3(.5f)+normal*.5f+tangent*(uv.x-.5f)+bitangent*(uv.y-.5f),
                        uv,material,daylight*shades[face]*(1.f-.15f*float(occlusion)),origin};
      }
      // Pick the diagonal that avoids visible AO discontinuities.
      if (quad[0].light+quad[2].light > quad[1].light+quad[3].light)
        for (int i : {0,1,3,1,2,3}) vertices.push_back(quad[i]);
      else for (int i : {0,1,2,0,2,3}) vertices.push_back(quad[i]);
    }
  }
  return vertices;
}
} // namespace bw

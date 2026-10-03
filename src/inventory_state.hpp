#pragma once
#include <array>
#include <cstdint>

namespace bw {
// Stable IDs for the saved nine-slot hotbar. A slot holds exactly one item.
enum class Item : std::uint8_t {
  Empty,Planks,Wood,Stone,Glass,Door,Torch,Grass,Leaves,Brick,Bed,Workbench,Fence,Gate,Wheat,Axe,Pickaxe,Dirt,Sand,
  Carrot,Strawberry,Pumpkin,WateringCan,Hoe,Compost,Sprinkler,Greenhouse,StoneSlab,Count
};
inline constexpr int hotbarSize=9;
inline constexpr std::array itemCatalog{
  Item::Planks,Item::Wood,Item::Stone,Item::Glass,Item::Door,Item::Torch,Item::Grass,Item::Leaves,Item::Brick,
  Item::Bed,Item::Workbench,Item::Fence,Item::Gate,Item::Wheat,Item::Axe,Item::Pickaxe,Item::Dirt,Item::Sand,
  Item::Carrot,Item::Strawberry,Item::Pumpkin,Item::WateringCan,Item::Hoe,Item::Compost,Item::Sprinkler,Item::Greenhouse,Item::StoneSlab,Item::Empty
};
struct Inventory {
  std::array<Item,hotbarSize> slots{Item::Planks,Item::Wood,Item::Stone,Item::Glass,Item::Door,Item::Torch,Item::Empty,Item::Empty,Item::Wheat};
  int selected=0;
  Item held() const { return slots[selected]; }
  bool operator==(const Inventory&) const = default;
};
} // namespace bw

#pragma once
#include "world.hpp"
#include <span>

namespace bw {
class Player;
Block itemBlock(Item item);
Tool itemTool(Item item);
Item itemFromBlock(Block block);
std::string_view itemName(Item item);
bool itemAvailable(Item item,const CraftState& crafting);
bool assignItem(Inventory& inventory,const CraftState& crafting,int slot,Item item);
bool equipItem(Inventory& inventory,const CraftState& crafting,Item item);
Inventory startingInventory(const CraftState& crafting);
enum class PlayMode { Farm,Build,Remove };
std::span<const Item> modeTools(PlayMode mode);
std::string_view modeName(PlayMode mode);
std::string_view toolName(Item item);
// Fixed tool palettes replace slot assignment. Each mode remembers its own tool.
struct ToolSelection {
  PlayMode mode=PlayMode::Farm;
  Item farm=Item::Hoe,build=Item::Planks,remove=Item::Empty;
  Item held() const;
  bool choose(Item item,const CraftState& crafting);
  bool select(int index,const CraftState& crafting);
  void cycle(int direction,const CraftState& crafting);
  bool removesBlocks() const { return mode==PlayMode::Build || mode==PlayMode::Remove; }
};
enum class UseKind { None,Place,Door,Gate,Bed,Workbench,Chicken,Crop,Water,Till,Compost,Greenhouse,Ranch };
struct UseTarget {
  UseKind kind=UseKind::None;
  Cell cell{};
  float distance=0;
  std::size_t chicken=0;
};
UseTarget useTarget(const World& world,const Player& player,Item item,bool sneaking=false);
} // namespace bw

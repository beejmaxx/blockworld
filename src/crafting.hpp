#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace bw {
class World;
class Player;
enum class Block : std::uint8_t;
enum CraftFlag : std::uint32_t {
  GatheredWood=1,MadePlanks=2,MadeWorkbench=4,PlacedWorkbench=8,MadeAxe=16,GatheredStone=32,MadePickaxe=64
};
struct CraftState {
  static constexpr int capacity=9999;
  int wood=0,planks=0,stone=0;
  std::uint32_t flags=0;
  bool has(std::uint32_t flag) const { return (flags&flag)!=0; }
};
enum class Recipe { Planks,Workbench,Axe,Pickaxe,Count };
enum class Tool { Hands,Axe,Pickaxe };
struct RecipeInfo { std::string_view name,cost,benefit; int wood{},planks{},stone{}; std::uint32_t unlock{}; bool bench=false; };
const std::array<RecipeInfo,4>& recipes();
bool nearbyWorkbench(const World& world,const Player& player);
std::string craftProblem(const World& world,const Player& player,Recipe recipe);
bool craft(World& world,const Player& player,Recipe recipe);
void collectMaterial(World& world,Block block);
Tool bestTool(const CraftState& state,Block block);
bool ownsTool(const CraftState& state,Tool tool);
std::string_view toolName(Tool tool);
float breakSeconds(const CraftState& state,Block block,Tool equipped=Tool::Hands);
int craftLesson(const CraftState& state);
struct CraftView {
  CraftState bag;
  int lesson=0;
  bool benchNearby=false;
  std::string title;
  std::array<std::string,2> lines;
  std::array<std::string,4> problems;
};
CraftView craftView(const World& world,const Player& player);
} // namespace bw

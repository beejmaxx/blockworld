#include "crafting.hpp"
#include "player.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
const std::array<RecipeInfo,4>& recipes() {
  static constexpr std::array<RecipeInfo,4> list{{
    {"4 planks","1 log","Prepare wood for your tools.",1,0,0,MadePlanks,false},
    {"Workbench","4 planks","Select it, then use V to place it on the ground.",0,4,0,MadeWorkbench,false},
    {"Wooden axe","3 planks + workbench","Choose it in E / Remove. Click to use it.",0,3,0,MadeAxe,true},
    {"Stone pickaxe","2 planks + 3 stone + bench","Choose it in E / Remove. Click to use it.",0,2,3,MadePickaxe,true}
  }};
  return list;
}
bool nearbyWorkbench(const World& world,const Player& player) {
  auto eye=player.eye();
  int bx=int(std::floor(eye.x)),by=int(std::floor(eye.y)),bz=int(std::floor(eye.z));
  for(int y=by-4;y<=by+3;++y) for(int z=bz-4;z<=bz+4;++z) for(int x=bx-4;x<=bx+4;++x) {
    Cell c{x,y,z};
    if(world.get(c)!=Block::Workbench) continue;
    auto delta=glm::vec3(x+.5f,y+.5f,z+.5f)-eye;
    if(glm::length(delta)>3.5f) continue;
    auto hit=world.raycast(eye,delta,3.5f);
    if(hit && hit->block==c) return true;
  }
  return false;
}
std::string craftProblem(const World& world,const Player& player,Recipe recipe) {
  if(int(recipe)<0 || recipe>=Recipe::Count) return "UNKNOWN RECIPE";
  const auto& r=recipes()[int(recipe)]; const auto& bag=world.crafting;
  if(recipe!=Recipe::Planks && bag.has(r.unlock)) return "Already made";
  if(recipe==Recipe::Planks && bag.planks>CraftState::capacity-4) return "Your plank storage is full.";
  if(bag.wood<r.wood) return "Click tree trunks to collect more logs.";
  if(bag.planks<r.planks) return "Make more planks with recipe 1.";
  if(bag.stone<r.stone) return "Click stone blocks to collect more stone.";
  if(r.bench && !nearbyWorkbench(world,player)) return "Place a workbench, then stand beside it.";
  return {};
}
bool craft(World& world,const Player& player,Recipe recipe) {
  if(!craftProblem(world,player,recipe).empty()) return false;
  const auto& r=recipes()[int(recipe)]; auto& bag=world.crafting;
  bag.wood-=r.wood; bag.planks-=r.planks; bag.stone-=r.stone;
  if(recipe==Recipe::Planks) bag.planks+=4;
  bag.flags|=r.unlock;
  return true;
}
void collectMaterial(World& world,Block block) {
  auto& bag=world.crafting;
  if(block==Block::Wood) { bag.wood=std::min(bag.wood+1,CraftState::capacity); if(bag.wood>=3) bag.flags|=GatheredWood; }
  if(block==Block::Planks) bag.planks=std::min(bag.planks+1,CraftState::capacity);
  if(block==Block::Stone || block==Block::Brick) {
    bag.stone=std::min(bag.stone+1,CraftState::capacity); if(bag.stone>=3) bag.flags|=GatheredStone;
  }
}
Tool bestTool(const CraftState& bag,Block b) {
  bool wood=b==Block::Wood || b==Block::Planks || b==Block::Workbench || isDoor(b) || isBed(b) || b==Block::Fence || isGate(b);
  if(wood && bag.has(MadeAxe)) return Tool::Axe;
  if((b==Block::Stone || b==Block::Brick) && bag.has(MadePickaxe)) return Tool::Pickaxe;
  return Tool::Hands;
}
std::string_view toolName(Tool tool) {
  return tool==Tool::Axe ? "AXE" : tool==Tool::Pickaxe ? "PICKAXE" : "HANDS";
}
bool ownsTool(const CraftState& bag,Tool tool) {
  return tool==Tool::Hands || (tool==Tool::Axe && bag.has(MadeAxe)) || (tool==Tool::Pickaxe && bag.has(MadePickaxe));
}
float breakSeconds(const CraftState& bag,Block b,Tool equipped) {
  if(b==Block::Air || b==Block::Bedrock) return 0;
  // Even the fastest tool requires a deliberate hold; a tap must be harmless.
  if(equipped!=Tool::Hands && bestTool(bag,b)==equipped) return .5f;
  if(b==Block::Stone || b==Block::Brick) return 1.15f;
  if(b==Block::Wood || b==Block::Workbench) return .8f;
  if(b==Block::Planks || isDoor(b) || isBed(b) || b==Block::Fence || isGate(b)) return .65f;
  if(b==Block::Torch || b==Block::Leaves || b==Block::Glass || isCrop(b)) return .5f;
  return .6f;
}
int craftLesson(const CraftState& bag) {
  constexpr std::array steps{GatheredWood,MadePlanks,MadeWorkbench,PlacedWorkbench,MadeAxe,GatheredStone,MadePickaxe};
  for(int i=0;i<int(steps.size());++i) if(!bag.has(steps[i])) return i;
  return 7;
}
CraftView craftView(const World& world,const Player& player) {
  CraftView view; view.bag=world.crafting; view.lesson=craftLesson(view.bag); view.benchNearby=nearbyWorkbench(world,player);
  for(int i=0;i<4;++i) view.problems[i]=craftProblem(world,player,Recipe(i));
  switch(view.lesson) {
    case 0: view.title="1. Collect three logs"; view.lines={"Close inventory with E. Aim at a tree trunk.","Click to break a block and collect the wood."}; break;
    case 1: view.title="2. Turn logs into planks"; view.lines={"Click recipe 1 below. One log makes four planks.","Your building blocks stay unlimited."}; break;
    case 2: view.title="3. Make a workbench"; view.lines={"Use four planks to make your first workbench.","Click recipe 2 below."}; break;
    case 3: view.title="4. Place your workbench"; view.lines={"E / Build / choose the Workbench.","Aim at the floor, then right-click or V."}; break;
    case 4: view.title="5. Make your first axe"; view.lines={"Stay near your workbench and make recipe 3.","Need more planks? Recipe 1 turns a log into four."}; break;
    case 5: view.title="6. Collect three stone"; view.lines={"Choose E / Remove, then click stone.","Collect three stone blocks for your pickaxe."}; break;
    case 6: view.title="7. Make a pickaxe"; view.lines={"Return to your workbench and make recipe 4.","It uses three stone and two planks."}; break;
    default: view.title="Your first tools are ready"; view.lines={"E / Remove / choose your axe or pickaxe.","Left-click to swing. E opens your tools and crafting."}; break;
  }
  return view;
}
} // namespace bw

#include "inventory.hpp"
#include "ranch.hpp"
#include "farm.hpp"
#include <algorithm>

namespace bw {
Block itemBlock(Item item) {
  constexpr std::array blocks{Block::Air,Block::Planks,Block::Wood,Block::Stone,Block::Glass,Block::DoorZ,Block::Torch,
    Block::Grass,Block::Leaves,Block::Brick,Block::BedZ,Block::Workbench,Block::Fence,Block::GateZ,Block::WheatYoung,Block::Air,Block::Air,Block::Dirt,Block::Sand,
    Block::CarrotYoung,Block::StrawberryYoung,Block::PumpkinYoung,Block::Air,Block::Air,Block::Air,Block::Sprinkler,Block::Air};
  return item<Item::Count ? blocks[int(item)] : Block::Air;
}
Tool itemTool(Item item) { return item==Item::Axe ? Tool::Axe : item==Item::Pickaxe ? Tool::Pickaxe : Tool::Hands; }
Item itemFromBlock(Block block) {
  if(block==Block::Air) return Item::Empty;
  if(isDoor(block)) return Item::Door;
  if(isGate(block)) return Item::Gate;
  if(isBed(block)) return Item::Bed;
  if(isWheat(block)) return Item::Wheat;
  if(isCrop(block)) return cropKind(block)==CropKind::Carrot ? Item::Carrot : cropKind(block)==CropKind::Strawberry ? Item::Strawberry : Item::Pumpkin;
  for(auto item : itemCatalog) if(itemBlock(item)==block) return item;
  return Item::Empty;
}
std::string_view itemName(Item item) {
  if(item==Item::Axe) return "WOODEN AXE";
  if(item==Item::Pickaxe) return "STONE PICKAXE";
  if(item==Item::Empty) return "EMPTY HAND";
  if(item==Item::WateringCan) return "WATERING CAN";
  if(item==Item::Hoe) return "GARDEN HOE";
  if(item==Item::Compost) return "COMPOST";
  if(item==Item::Greenhouse) return "GREENHOUSE KIT";
  return blockName(itemBlock(item));
}
bool itemAvailable(Item item,const CraftState& crafting) {
  if(item>=Item::Count) return false;
  if(item==Item::Workbench) return crafting.has(MadeWorkbench);
  return ownsTool(crafting,itemTool(item));
}
bool assignItem(Inventory& inventory,const CraftState& crafting,int slot,Item item) {
  if(slot<0 || slot>=hotbarSize || !itemAvailable(item,crafting)) return false;
  inventory.slots[slot]=item; inventory.selected=slot; return true;
}
bool equipItem(Inventory& inventory,const CraftState& crafting,Item item) {
  if(!itemAvailable(item,crafting)) return false;
  auto slot=std::ranges::find(inventory.slots,item);
  if(slot==inventory.slots.end()) slot=std::ranges::find(inventory.slots,Item::Empty);
  int index=slot==inventory.slots.end() ? inventory.selected : int(slot-inventory.slots.begin());
  return assignItem(inventory,crafting,index,item);
}
Inventory startingInventory(const CraftState& crafting) {
  Inventory result;
  if(crafting.has(MadeAxe)) { result.slots[6]=Item::Axe; result.selected=6; }
  if(crafting.has(MadePickaxe)) { result.slots[7]=Item::Pickaxe; if(result.selected==0) result.selected=7; }
  return result;
}
UseTarget useTarget(const World& world,const Player& player,Item item,bool sneaking) {
  if(auto ranch=targetRanch(world,player)) return {UseKind::Ranch,{},ranch->distance,ranch->index};
  if(!sneaking) if(auto chicken=targetChicken(world,player)) return {UseKind::Chicken,{},0,*chicken};
  auto hit=world.raycast(player.eye(),player.direction());
  if(!hit) return {};
  auto block=world.get(hit->block);
  if(!sneaking) {
    auto kind=isDoor(block) ? UseKind::Door : isGate(block) ? UseKind::Gate : isBed(block) ? UseKind::Bed
      : block==Block::Workbench ? UseKind::Workbench
      : isCrop(block) ? (cropRipe(block) ? UseKind::Crop : item==Item::WateringCan ? UseKind::Water
          : item==Item::Compost ? UseKind::Compost : UseKind::Crop) : UseKind::None;
    if(kind!=UseKind::None) return {kind,hit->block,hit->distance};
  }
  if(item==Item::Hoe) return {UseKind::Till,hit->block,hit->distance};
  if(item==Item::Greenhouse) return {UseKind::Greenhouse,hit->adjacent,hit->distance};
  if(itemBlock(item)==Block::Air || !itemAvailable(item,world.crafting)) return {};
  return {UseKind::Place,hit->adjacent,hit->distance};
}
} // namespace bw

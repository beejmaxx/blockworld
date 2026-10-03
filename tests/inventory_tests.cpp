#include "inventory.hpp"
#include "building.hpp"
#include "adventure.hpp"
#include "farm.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
World emptyWorld() {
  World world;
  for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) world.insert(Chunk{{x,z}});
  return world;
}
void sharedHotbar() {
  CraftState craft; Inventory inventory;
  auto initial=inventory;
  for(auto item : {Item::Axe,Item::Pickaxe,Item::Workbench,Item::Count,Item(255)})
    check(!assignItem(inventory,craft,0,item) && inventory==initial,"uncrafted or invalid items cannot enter the hotbar");
  check(!assignItem(inventory,craft,-1,Item::Stone) && !assignItem(inventory,craft,9,Item::Stone)
        && inventory==initial,"invalid slots do not mutate the inventory");
  check(equipItem(inventory,craft,Item::Glass) && inventory.selected==3 && inventory.slots==initial.slots,
        "picking an existing block selects its slot without duplicating it");
  craft.flags=MadeAxe;
  check(equipItem(inventory,craft,Item::Axe) && inventory.selected==6 && inventory.held()==Item::Axe
        && itemTool(inventory.held())==Tool::Axe && itemBlock(inventory.held())==Block::Air,
        "crafting equips a visible axe in an empty slot, with no hidden placement block");
  check(assignItem(inventory,craft,6,Item::Fence) && itemTool(inventory.held())==Tool::Hands
        && itemBlock(inventory.held())==Block::Fence,"replacing the axe slot changes both the hand and placement item");
  check(assignItem(inventory,craft,6,Item::Empty) && inventory.held()==Item::Empty,"clearing a slot equips an empty hand");
  inventory.slots.fill(Item::Stone); inventory.selected=4;
  check(equipItem(inventory,craft,Item::Wheat) && inventory.selected==4 && inventory.held()==Item::Wheat
        && inventory.slots[3]==Item::Stone,"a full hotbar replaces only its selected slot");
  check(itemFromBlock(Block::Air)==Item::Empty && itemFromBlock(Block::Bedrock)==Item::Empty,
        "air and unavailable blocks never pick an unrelated tool");
  for(auto block : {Block::DoorZ,Block::DoorXOpenTop}) check(itemFromBlock(block)==Item::Door,"door variants pick one door item");
  check(itemFromBlock(Block::BedXHead)==Item::Bed && itemFromBlock(Block::GateXOpen)==Item::Gate
        && itemFromBlock(Block::WheatRipe)==Item::Wheat,"beds, gates, and crops pick their placeable item");
  for(auto item : itemCatalog) if(itemBlock(item)!=Block::Air)
    check(itemFromBlock(itemBlock(item))==item,"all building items round trip through block picking");
}
void modeToolsAndRemoval() {
  auto world=emptyWorld(); world.crafting.flags=127;
  Player player; player.pose.position={.5f,2,3.5f}; player.pose.yaw=0; player.pose.pitch=0;
  Cell wall{0,3,0}; world.set(wall,Block::Wood);
  ToolSelection tools;
  for(auto item : modeTools(PlayMode::Farm)) {
    check(tools.choose(item,world.crafting) && tools.mode==PlayMode::Farm && tools.held()==item,"direct selection equips the chosen farm tool");
    check(!removeSelectedBlock(world,player,tools) && world.get(wall)==Block::Wood && world.crafting.wood==0,
          "farming tools cannot remove a house block or collect its materials");
  }
  for(bool flying : {false,true}) for(auto item : modeTools(PlayMode::Build)) {
    world.set(wall,Block::Wood); world.crafting.wood=0; player.pose.flying=flying;
    check(tools.choose(item,world.crafting) && tools.mode==PlayMode::Build && tools.held()==item,"direct selection equips the chosen building material");
    auto target=useTarget(world,player,tools.held());
    check(target.kind==UseKind::Place && target.cell==Cell{0,3,1},"use places against the targeted block in Build mode");
    auto removed=removeSelectedBlock(world,player,tools);
    check(removed && removed->cell==wall && world.get(wall)==Block::Air && world.crafting.wood==1,
          "Build mode removes and collects a block immediately while walking or flying");
    check(tools.mode==PlayMode::Build && tools.held()==item && !removeSelectedBlock(world,player,tools) && world.crafting.wood==1,
          "removal keeps the chosen building material and cannot collect the same block twice");
  }
  world.set(wall,Block::Wood); world.crafting.wood=0; player.pose.flying=false;
  tools.choose(Item::Pumpkin,world.crafting); tools.choose(Item::Glass,world.crafting); tools.choose(Item::Empty,world.crafting);
  check(tools.removesBlocks() && tools.held()==Item::Empty,"Remove starts with the free hammer");
  auto removed=removeSelectedBlock(world,player,tools);
  check(removed && removed->cell==wall && world.get(wall)==Block::Air && world.crafting.wood==1,
        "one deliberate Remove action breaks and collects a block immediately");
  check(!removeSelectedBlock(world,player,tools) && world.crafting.wood==1,"an empty target cannot duplicate materials");
  tools.mode=PlayMode::Farm;
  check(tools.held()==Item::Pumpkin,"switching back to Farm remembers the chosen seeds");
  tools.mode=PlayMode::Build;
  check(tools.held()==Item::Glass,"switching back to Build remembers the chosen material");
  tools.choose(Item::Sand,world.crafting);
  CraftState locked;
  tools.cycle(1,locked);
  check(tools.held()==Item::Planks,"scrolling wraps and skips the uncrafted workbench");
  check(!tools.choose(Item::Axe,locked) && tools.mode==PlayMode::Build && tools.held()==Item::Planks,
        "a locked tool cannot change the current mode or selection");
  check(!tools.choose(Item::Count,locked) && !tools.select(-1,locked) && !tools.select(99,locked),"invalid tool choices are inert");
  tools.choose(Item::Empty,locked); tools.cycle(1,locked);
  check(tools.held()==Item::Empty,"the hammer works without crafting and scrolling cannot select locked removal tools");
  tools.mode=PlayMode::Farm; tools.choose(Item::Hoe,world.crafting);
  world.set({0,1,0},Block::Farmland); world.set({0,2,0},Block::CarrotRipe);
  player.pose.pitch=std::atan2(2.2f-player.eye().y,3.f);
  check(useTarget(world,player,tools.held()).kind==UseKind::Crop && !removeSelectedBlock(world,player,tools)
        && world.get({0,2,0})==Block::CarrotRipe,"a ripe crop is a harvest target even with the hoe selected, never a remove action");
  world.set({0,2,0},Block::Air);
  for(int z=-3;z<=4;++z) for(int x=-2;x<=2;++x) world.set({x,1,z},Block::Grass);
  Chicken hen; hen.position={.5f,2,.5f}; world.farm.chickens.push_back(hen);
  player.pose.pitch=std::atan2(2.4f-player.eye().y,3.f);
  auto floor=world.raycast(player.eye(),player.direction());
  for(auto mode : {PlayMode::Build,PlayMode::Remove}) {
    tools.mode=mode;
    check(floor && !removeSelectedBlock(world,player,tools) && world.get(floor->block)==Block::Grass,
          "Build and Remove protect animals and the ground behind them");
  }
}
void contextualUse() {
  auto world=emptyWorld(); Player player; player.pose.position={.5f,2,3.5f}; player.pose.yaw=0; player.pose.pitch=0;
  world.crafting.flags=127; world.set({0,3,0},Block::Wood);
  auto target=useTarget(world,player,Item::Planks);
  check(target.kind==UseKind::Place && target.cell==Cell{0,3,1},"holding a block places against the targeted face");
  check(useTarget(world,player,Item::Axe).kind==UseKind::None
        && useTarget(world,player,Item::Pickaxe).kind==UseKind::None
        && useTarget(world,player,Item::Empty).kind==UseKind::None,"tools and empty hands cannot place a previously selected block");
  world.set({0,3,0},Block::Air); world.set({0,1,0},Block::Stone);
  check(placeBlock(world,player,{0,2,0},Block::DoorZ),"door fixture is placed");
  for(auto item : {Item::Planks,Item::Axe,Item::Empty})
    check(useTarget(world,player,item).kind==UseKind::Door,"right-use opens a door with any held item");
  check(useTarget(world,player,Item::Planks,true).kind==UseKind::Place
        && useTarget(world,player,Item::Axe,true).kind==UseKind::None,"sneaking bypasses door use only when holding a placeable block");
  breakBlock(world,{0,2,0}); world.set({0,3,0},Block::Workbench);
  check(useTarget(world,player,Item::Axe).kind==UseKind::Workbench,"tools can open a workbench without changing selection");
  world.set({0,3,0},Block::Air);
  Chicken hen; hen.position={.5f,2,.5f}; world.farm.chickens.push_back(hen);
  player.pose.pitch=std::atan2(2.5f-player.eye().y,3.f);
  check(useTarget(world,player,Item::Wheat).kind==UseKind::Chicken,"right-use reaches the chicken under the crosshair");
  world.set({0,2,1},Block::Stone); world.set({0,3,1},Block::Stone);
  check(useTarget(world,player,Item::Wheat).kind==UseKind::Place
        && useTarget(world,player,Item::Axe).kind==UseKind::None,"a wall blocks animal interactions and retains ordinary held-item behavior");
}
void sneakingAndFlight() {
  auto world=emptyWorld(); world.set({0,1,0},Block::Stone);
  Player sneaking; sneaking.pose.position={.5f,2,.5f}; sneaking.pose.yaw=0;
  sneaking.tick(world,{},1.f/60.f);
  Player walking=sneaking; Movement move; move.forward=1; move.sneak=true;
  for(int i=0;i<120;++i) sneaking.tick(world,move,1.f/60.f);
  check(sneaking.grounded && std::abs(sneaking.pose.position.y-2)<.003f && sneaking.pose.position.z>=-.301f,
        "holding sneak stops at a ledge instead of falling off");
  check(std::abs(sneaking.eye().y-sneaking.pose.position.y-1.42f)<.001f,"sneaking lowers the viewpoint");
  move.sneak=false;
  for(int i=0;i<60;++i) walking.tick(world,move,1.f/60.f);
  check(walking.pose.position.z<-2 && walking.pose.position.y<1.9f,"ordinary movement can walk off the same ledge");
  for(int z=-10;z<=10;++z) for(int x=-10;x<=10;++x) world.set({x,1,z},Block::Stone);
  Player walker,runner,croucher;
  walker.pose.position={.5f,2,.5f}; walker.pose.yaw=0; runner=walker; croucher=walker;
  for(int i=0;i<60;++i) {
    Movement normal; normal.forward=1; walker.tick(world,normal,1.f/60.f);
    auto sprint=normal; sprint.sprint=true; runner.tick(world,sprint,1.f/60.f);
    auto sneak=normal; sneak.sneak=true; sneak.sprint=true; croucher.tick(world,sneak,1.f/60.f);
  }
  check(runner.pose.position.z<walker.pose.position.z-2 && croucher.pose.position.z>walker.pose.position.z+2,
        "sprint is faster, and sneak takes precedence over sprint");
  Player flyer; flyer.pose.position={.5f,5,.5f}; flyer.pose.flying=true;
  Movement descend; descend.vertical=-1; descend.sneak=true;
  for(int i=0;i<60;++i) flyer.tick(world,descend,1.f/60.f);
  check(!flyer.pose.flying && flyer.grounded && std::abs(flyer.pose.position.y-2)<.003f,
        "descending onto the floor exits flight safely");
  flyer.pose.position={.5f,5,.5f}; flyer.pose.flying=true; flyer.velocity={1,10,1};
  flyer.stopFlying();
  check(!flyer.pose.flying && flyer.velocity==glm::vec3(0),"stop-flying shortcut clears flight and upward drift");
  flyer.stopFlying();
  for(int i=0;i<90;++i) flyer.tick(world,{},1.f/60.f);
  check(!flyer.pose.flying && flyer.grounded && std::abs(flyer.pose.position.y-2)<.003f,"repeated stop-flying input stays off and lands safely");
  flyer.toggleFlying();
  flyer.tick(world,{},1.f/60.f);
  check(flyer.pose.flying,"flight can be switched on while standing on the ground");
  Movement ascend; ascend.vertical=1;
  for(int i=0;i<30;++i) flyer.tick(world,ascend,1.f/60.f);
  check(flyer.pose.flying && flyer.pose.position.y>6.9f,"holding ascend after toggling flight lifts the player");
  flyer.toggleFlying();
  check(!flyer.pose.flying && flyer.velocity==glm::vec3(0),"flight toggle also switches off and clears upward drift");
  for(int i=0;i<90;++i) flyer.tick(world,{},1.f/60.f);
  check(flyer.grounded && std::abs(flyer.pose.position.y-2)<.003f,"switching flight off returns to walking on the ground");
}
void inventorySaves() {
  auto directory=std::filesystem::temp_directory_path()/("blockworld-inventory-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);} } cleanup{directory};
  auto path=directory/"world.bw";
  auto world=emptyWorld(); world.crafting={8,164,18,127}; world.guideFlags=127; world.clock.phase=.8; world.clock.day=6;
  world.farm.initialized=true; world.farm.wheat=7; world.farm.eggs=2;
  Chicken hen; hen.position={3.5f,24,8.5f}; hen.eggTimer=12; world.farm.chickens.push_back(hen);
  world.inventory.slots={Item::Axe,Item::Bed,Item::Gate,Item::Wheat,Item::Empty,Item::Glass,Item::Pickaxe,Item::Workbench,Item::Dirt};
  world.inventory.selected=6; world.set({1,25,1},Block::Workbench);
  Player player; player.pose.position={4,25,5}; world.save(path,player.pose);
  World loaded; check(loaded.load(path).has_value(),"current save loads"); loaded.ensure({0,0},1);
  check(loaded.inventory==world.inventory && loaded.crafting.flags==127 && loaded.crafting.planks==164
        && loaded.farm.wheat==7 && loaded.farm.eggs==2 && loaded.farm.chickens[0].eggTimer==12
        && loaded.clock.day==6 && loaded.get({1,25,1})==Block::Workbench,"custom slots, selection, buildings, tools, farm, and time round trip together");
  std::string prefix="BLOCKWORLD 6 123 1\n4 25 5 0 0 0\n127\n0.8 6\n8 164 18 127\n1 7 2 0 3.5 24 8.5 0 0\n";
  for(auto inventory : {"9 1 2 3 4 5 6 0 0 14", "-1 1 2 3 4 5 6 0 0 14", "0 1 2 3 4 5 6 0 0 19",
                         "0 1 2 3 4 5 6 0 0 -1", "0 1 2 3 4 5 6 0 0 256", "0 1 2 3 4 5"}) {
    std::ofstream(path)<<prefix<<inventory<<"\n0\n";
    bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
    check(rejected && loaded.inventory==world.inventory && loaded.farm.chickens.size()==1
          && loaded.get({1,25,1})==Block::Workbench,"malformed hotbars reject atomically without changing the world");
  }
  std::ofstream(path)<<"BLOCKWORLD 6 123 1\n4 25 5 0 0 0\n127\n0.8 6\n0 0 0 0\n0 0 0 0 3.5 24 8.5 0 0\n0 15 2 3 4 5 6 0 0 14\n0\n";
  bool rejected=false; try { loaded.load(path); } catch(const std::exception&) { rejected=true; }
  check(rejected && loaded.inventory==world.inventory,"an unowned saved tool is rejected without mutation");
  std::ofstream(path)<<"BLOCKWORLD 5 123 1\n4 25 5 0 0 0\n127\n0.8 6\n8 164 18 127\n1 7 2 31 3.5 24 8.5 1 0\n3.5 24 8.5 0 12 0\n1\n1 25 1 24\n";
  check(loaded.load(path).has_value(),"existing version five worlds migrate"); loaded.ensure({0,0},1);
  check(loaded.inventory.held()==Item::Axe && loaded.inventory.slots[7]==Item::Pickaxe
        && loaded.crafting.planks==164 && loaded.farm.eggs==2 && loaded.farm.chickens[0].eggTimer==12
        && loaded.get({1,25,1})==Block::Workbench && loaded.guideFlags==127,"migration puts owned tools in the hotbar while retaining all progress");
  loaded.save(path,player.pose); World reloaded;
  check(reloaded.load(path).has_value() && reloaded.inventory==loaded.inventory,"migrated inventory persists on the next save");
}
}
int main() {
  try {
    for(auto [name,test] : {std::pair{"shared hotbar",&sharedHotbar},{"mode tools and removal",modeToolsAndRemoval},{"contextual use",contextualUse},
                           {"sneaking and flight",sneakingAndFlight},{"inventory saves",inventorySaves}}) {
      test(); std::cout<<"PASS "<<name<<'\n';
    }
  } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

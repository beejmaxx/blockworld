#include "garden.hpp"
#include <algorithm>
#include <cmath>

namespace bw {
namespace {
bool reached(const World& world,const Player& player,Cell cell) {
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  return hit && hit->block==cell;
}
std::vector<std::pair<Cell,Block>> greenhousePieces(Cell door) {
  std::vector<std::pair<Cell,Block>> pieces;
  for(int z=-4;z<=0;++z) for(int x=-2;x<=2;++x) {
    if(x>-2 && x<2 && z>-4 && z<0) pieces.push_back({door+Cell{x,-1,z},Block::Farmland});
    for(int y=0;y<=3;++y) {
      if(y<3 && x>-2 && x<2 && z>-4 && z<0) continue;
      auto b=(std::abs(x)==2 && (z==-4 || z==0)) ? Block::Wood : Block::Glass;
      if(x==0 && z==0 && y<2) b=y==0 ? Block::DoorZ : Block::DoorZTop;
      pieces.push_back({door+Cell{x,y,z},b});
    }
  }
  return pieces;
}
}
bool initializeGarden(World& world) {
  if(world.farm.garden.initialized || !world.terrain.adventure()) return false;
  // Only use untouched, level ground. A migrated save never loses a player build.
  for(glm::ivec2 candidate : {glm::ivec2(16,7),{-4,-16},{14,-16}}) {
    int y=world.terrain.height(candidate.x,candidate.y)+1;
    if(y<2 || y>worldHeight-5) continue;
    world.ensure(chunkAt(candidate.x+4,candidate.y+4),1);
    bool clear=true;
    for(int z=0;z<9 && clear;++z) for(int x=0;x<9 && clear;++x) {
      Cell c{candidate.x+x,y-1,candidate.y+z};
      if(world.get(c)!=Block::Grass) { clear=false; break; }
      for(int h=y-1;h<worldHeight;++h)
        if(world.edited({c.x,h,c.z}) || (h>=y && world.get({c.x,h,c.z})!=Block::Air)) { clear=false; break; }
    }
    if(!clear || world.farm.crops.size()+6>cropLimit) continue;
    auto& garden=world.farm.garden;
    garden.origin={candidate.x,y,candidate.y}; garden.initialized=true;
    for(int z=0;z<9;++z) for(int x=0;x<9;++x) {
      bool path=x%4==0 || z%4==0;
      if(path || x<4 || z<4) world.set({candidate.x+x,y-1,candidate.y+z},path ? Block::Sand : Block::Farmland);
    }
    // A few examples make growth readable, while one entire bed is yours to prepare.
    auto plant=[&](int x,int z,Block b) { world.set({candidate.x+x,y,candidate.y+z},b); };
    plant(1,1,Block::CarrotYoung); plant(2,1,Block::CarrotGrowing); plant(3,1,Block::CarrotRipe);
    plant(5,1,Block::StrawberryYoung); plant(7,1,Block::StrawberryGrowing); plant(2,6,Block::PumpkinYoung);
    return true;
  }
  return false;
}
bool visitGarden(World& world,Player& player) {
  if(!world.farm.garden.initialized) return false;
  auto o=world.farm.garden.origin;
  world.ensure(chunkAt(o.x+4,o.z+4),1);
  for(auto offset : {glm::ivec2(4,8),{4,4},{8,4},{0,4},{4,0}}) {
    glm::vec3 position(o.x+offset.x+.5f,o.y,o.z+offset.y+.5f);
    for(int up=0;up<5;++up) {
      auto candidate=position+glm::vec3(0,up,0);
      Cell floor{int(std::floor(candidate.x)),int(candidate.y)-1,int(std::floor(candidate.z))};
      if(!collidable(world.get(floor)) || player.collides(world,candidate)) continue;
      player.pose.position=candidate; player.pose.yaw=0; player.pose.pitch=-.35f;
      player.pose.flying=false; player.velocity={}; return true;
    }
  }
  return false;
}
bool tillSoil(World& world,const Player& player,Cell soil) {
  if(world.inventory.held()!=Item::Hoe || !reached(world,player,soil)) return false;
  auto block=world.get(soil);
  if((block!=Block::Grass && block!=Block::Dirt) || world.get(soil+Cell{0,1,0})!=Block::Air) return false;
  if(!world.set(soil,Block::Farmland)) return false;
  world.farm.garden.flags|=PreparedBed; return true;
}
bool applyCompost(World& world,const Player& player,Cell plant) {
  if(world.inventory.held()!=Item::Compost || world.farm.garden.compost<=0 || !reached(world,player,plant)) return false;
  if(!world.compostCrop(plant)) return false;
  --world.farm.garden.compost; return true;
}
int basketValue(const FarmState& farm) {
  int value=farm.milk*5;
  for(auto kind : cropKinds) value+=farm.harvest(kind)*cropPrice(kind);
  return value;
}
int sellBasket(World& world) {
  int value=basketValue(world.farm);
  auto& garden=world.farm.garden;
  if(value<=0 || value>coinLimit-garden.coins) return 0;
  for(auto kind : cropKinds) world.farm.harvest(kind)=0;
  world.farm.milk=0;
  garden.coins+=value; garden.flags|=SoldHarvest; return value;
}
bool buyGardenSupply(World& world,GardenPurchase item) {
  auto& g=world.farm.garden;
  int& amount=item==GardenPurchase::Compost ? g.compost : item==GardenPurchase::Sprinkler ? g.sprinklers : g.greenhouses;
  int count=item==GardenPurchase::Compost ? 5 : 1,price=gardenPrice(item);
  if(g.coins<price || amount>gardenSupplyLimit-count) return false;
  g.coins-=price; amount+=count; g.flags|=ImprovedGarden; return true;
}
std::string greenhouseProblem(const World& world,const Player& player,Cell door) {
  if(world.farm.garden.greenhouses<=0) return "P / SHOP / BUY A GREENHOUSE KIT";
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  if(!hit || hit->adjacent!=door) return "STEP CLOSER TO PLACE THE GREENHOUSE";
  for(int z=-4;z<=0;++z) for(int x=-2;x<=2;++x) {
    auto c=door+Cell{x,0,z};
    if(c.y<1 || c.y+3>=worldHeight || std::abs(c.x)>coordinateLimit || std::abs(c.z)>coordinateLimit
       || !world.chunks.contains(chunkAt(c.x,c.z))) return "CHOOSE A CLEAR 5 BY 5 PATCH OF LEVEL SOIL";
    auto ground=world.get(c+Cell{0,-1,0});
    if(ground!=Block::Grass && ground!=Block::Dirt && ground!=Block::Farmland) return "CHOOSE A CLEAR 5 BY 5 PATCH OF LEVEL SOIL";
    for(int h=0;h<=3;++h) if(world.get(c+Cell{0,h,0})!=Block::Air) return "GREENHOUSE NEEDS 5 BY 5 CLEAR SPACES / EXTENDS NORTH";
  }
  for(const auto& [cell,block] : greenhousePieces(door)) if(block!=Block::Farmland
      && (player.overlaps(cell,block) || chickensOverlap(world,blockBounds(cell,block)))) return "STEP CLEAR OF THE GREENHOUSE WALLS";
  return {};
}
bool placeGreenhouse(World& world,const Player& player,Cell door) {
  if(world.inventory.held()!=Item::Greenhouse) return false;
  auto hit=world.raycast(player.eye(),player.direction(),3.5f);
  if(!hit || hit->adjacent!=door || !greenhouseProblem(world,player,door).empty()) return false;
  for(const auto& [cell,block] : greenhousePieces(door)) world.set(cell,block);
  --world.farm.garden.greenhouses; return true;
}
GuideView gardenGuide(const World& world,const Player&) {
  GuideView view; view.enabled=true; view.farm=true;
  constexpr std::array milestones{PreparedBed,SowedCrop,WateredCrop,PickedCrop,SoldHarvest,ImprovedGarden};
  while(view.stage<int(milestones.size()) && (world.farm.garden.flags&milestones[view.stage])) ++view.stage;
  if(world.farm.garden.initialized) {
    view.destination=glm::vec3(world.farm.garden.origin)+glm::vec3(4.5f,.5f,4.5f);
    view.destinationName="VEGETABLE GARDEN";
  }
  switch(view.stage) {
    case 0: view.title="01 / PREPARE YOUR SOIL";
      view.lines={"P / GARDEN / take the hoe.","Click or V on grass to till it.","P visits the garden. R goes home."}; break;
    case 1: view.title="02 / PLANT YOUR FIRST ROW";
      view.lines={"P / GARDEN / choose carrot seeds.","Click or V on prepared soil.","Seeds are free. Try a little row."}; break;
    case 2: view.title="03 / WATER YOUR PLANTS";
      view.lines={"P / GARDEN / take the watering can.","Click or V waters a plant.","Wet plants grow twice as fast."}; break;
    case 3: view.title="04 / FILL YOUR HARVEST BASKET";
      view.lines={"Click or V picks a ripe crop.","Carrots leave room for new seeds.","Strawberry bushes fruit again."}; break;
    case 4: view.title="05 / SELL YOUR HARVEST";
      view.lines={"P / SHOP shows your harvest basket.","Sell the basket to earn garden coins.","Save 12 coins for a sprinkler."}; break;
    case 5: view.title="06 / IMPROVE YOUR GARDEN";
      view.lines={"P / SHOP has compost and sprinklers.","Sprinklers water plants nearby.","A greenhouse gives 25% extra growth."}; break;
    default: view.title="YOUR VEGETABLE GARDEN";
      view.lines={"Hoe new beds and try different crops.","Compost gives two extra per harvest.","P / SHOP turns harvests into upgrades."}; view.destination.reset(); break;
  }
  return view;
}
} // namespace bw

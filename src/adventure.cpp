#include "adventure.hpp"
#include "farm.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bw {
namespace {
bool editable(const World& world,Cell c) {
  return c.y>0 && c.y<worldHeight && std::abs(c.x)<=coordinateLimit && std::abs(c.z)<=coordinateLimit
    && world.chunks.contains(chunkAt(c.x,c.z)) && world.get(c)!=Block::Bedrock;
}
bool support(Block b) { return opaque(b) || b==Block::Glass; }
Cell doorBase(Cell cell,Block b) { if(doorUpper(b)) --cell.y; return cell; }
glm::vec3 middle(Cell c) { return glm::vec3(c.x+.5f,c.y+.5f,c.z+.5f); }
bool frameMaterial(Block b) { return b==Block::Planks || b==Block::Wood || b==Block::Stone || b==Block::Brick; }
}
PlacementStatus placementStatus(const World& world,const Player& player,Cell c,Block b,bool alongX) {
  if(!editable(world,c) || b==Block::Air || b==Block::Bedrock || b>=Block::Count) return PlacementStatus::OutsideWorld;
  if(solid(world.get(c))) return PlacementStatus::Occupied;
  if(b==Block::Workbench && !world.crafting.has(MadeWorkbench)) return PlacementStatus::WorkbenchLocked;
  if(isCrop(b)) {
    auto below=world.get(c+Cell{0,-1,0});
    if(below!=Block::Farmland) return PlacementStatus::NeedsSoil;
    if(world.farm.crops.size()>=cropLimit) return PlacementStatus::CropLimit;
    for(int z=-1;z<=1;++z) for(int x=-1;x<=1;++x) if(x || z) {
      auto neighbor=world.get(c+Cell{x,0,z});
      if(isCrop(neighbor) && (cropKind(b)==CropKind::Pumpkin || cropKind(neighbor)==CropKind::Pumpkin)) return PlacementStatus::PumpkinSpace;
    }
  }
  if(b==Block::Sprinkler) {
    if(world.farm.garden.sprinklers<=0 || world.farm.sprinklers.size()>=sprinklerLimit) return PlacementStatus::NeedSprinkler;
    if(!support(world.get(c+Cell{0,-1,0}))) return PlacementStatus::NeedsFloor;
  }
  if((b==Block::Fence || isGate(b)) && !support(world.get(c+Cell{0,-1,0}))) return PlacementStatus::NeedsFloor;
  if(isDoor(b)) {
    Cell top=c+Cell{0,1,0};
    auto lower=doorVariant(alongX,false,false),upper=doorVariant(alongX,false,true);
    if(!editable(world,top) || solid(world.get(top))) return PlacementStatus::NoRoom;
    if(!support(world.get(c+Cell{0,-1,0}))) return PlacementStatus::NeedsFloor;
    if(player.overlaps(c,lower) || player.overlaps(top,upper)) return PlacementStatus::PlayerOverlap;
    if(chickensOverlap(world,blockBounds(c,lower)) || chickensOverlap(world,blockBounds(top,upper))) return PlacementStatus::ChickenOverlap;
    return PlacementStatus::Ready;
  }
  if(isBed(b)) {
    auto foot=bedVariant(alongX,false),head=bedVariant(alongX,true);
    Cell other=bedOther(c,foot);
    if(!editable(world,other) || solid(world.get(other))) return PlacementStatus::NoRoom;
    if(!support(world.get(c+Cell{0,-1,0})) || !support(world.get(other+Cell{0,-1,0}))) return PlacementStatus::NeedsFloor;
    if(player.overlaps(c,foot) || player.overlaps(other,head)) return PlacementStatus::PlayerOverlap;
    if(chickensOverlap(world,blockBounds(c,foot)) || chickensOverlap(world,blockBounds(other,head))) return PlacementStatus::ChickenOverlap;
    return PlacementStatus::Ready;
  }
  if(b==Block::Torch && !support(world.get(c+Cell{0,-1,0}))) return PlacementStatus::NeedsFloor;
  if(isGate(b)) b=gateVariant(alongX,false);
  if(collidable(b) && chickensOverlap(world,blockBounds(c,b))) return PlacementStatus::ChickenOverlap;
  return player.overlaps(c,b) ? PlacementStatus::PlayerOverlap : PlacementStatus::Ready;
}
std::string_view placementMessage(PlacementStatus status) {
  switch(status) {
    case PlacementStatus::Ready: return "RIGHT CLICK / V PLACE / LEFT CLICK TO BREAK";
    case PlacementStatus::Occupied: return "THAT SPACE IS OCCUPIED";
    case PlacementStatus::NeedsFloor: return "PLACE ON A SOLID FLOOR";
    case PlacementStatus::NoRoom: return "CLEAR BOTH SPACES FIRST";
    case PlacementStatus::PlayerOverlap: return "STEP BACK TO MAKE ROOM";
    case PlacementStatus::WorkbenchLocked: return "E / CRAFT YOUR FIRST WORKBENCH";
    case PlacementStatus::NeedsSoil: return "PREPARE SOIL WITH THE HOE / P OPENS YOUR GARDEN";
    case PlacementStatus::PumpkinSpace: return "PUMPKINS NEED A ONE-BLOCK GAP FROM OTHER PLANTS";
    case PlacementStatus::NeedSprinkler: return "P / SHOP / BUY A SPRINKLER / MAX 64 PLACED";
    case PlacementStatus::ChickenOverlap: return "LET THE CHICKEN MOVE FIRST";
    case PlacementStatus::CropLimit: return "GARDEN FULL / REMOVE A PLANT WITH X";
    default: return "BUILD WITHIN THE WORLD BOUNDARY";
  }
}
bool placeBlock(World& world,const Player& player,Cell c,Block b,bool alongX) {
  if(placementStatus(world,player,c,b,alongX)!=PlacementStatus::Ready) return false;
  if(isDoor(b)) {
    world.set(c,doorVariant(alongX,false,false)); world.set(c+Cell{0,1,0},doorVariant(alongX,false,true)); return true;
  }
  if(isBed(b)) {
    auto foot=bedVariant(alongX,false);
    world.set(c,foot); world.set(bedOther(c,foot),bedVariant(alongX,true)); return true;
  }
  if(b==Block::Workbench) world.crafting.flags|=PlacedWorkbench;
  if(isGate(b)) b=gateVariant(alongX,false);
  if(isCrop(b)) b=cropStage(cropKind(b),0);
  if(!world.set(c,b)) return false;
  if(isCrop(b)) world.farm.garden.flags|=SowedCrop;
  if(b==Block::Sprinkler) --world.farm.garden.sprinklers;
  return true;
}
bool breakBlock(World& world,Cell c) {
  if(!editable(world,c) || !solid(world.get(c))) return false;
  auto b=world.get(c);
  if(isDoor(b)) {
    c=doorBase(c,b); Cell top=c+Cell{0,1,0};
    if(isDoor(world.get(top))) world.set(top,Block::Air);
    world.set(c,Block::Air); return true;
  }
  if(isBed(b)) {
    Cell other=bedOther(c,b);
    // Never remove an unrelated block if an edited or imported pair is incomplete.
    if(world.get(other)==bedVariant(bedAlongX(b),!bedHead(b))) world.set(other,Block::Air);
    return world.set(c,Block::Air);
  }
  if(!world.set(c,Block::Air)) return false;
  if(b==Block::Sprinkler) world.farm.garden.sprinklers=std::min(gardenSupplyLimit,world.farm.garden.sprinklers+1);
  Cell above=c+Cell{0,1,0}; auto upper=world.get(above);
  if(upper==Block::Torch || upper==Block::Sprinkler || isBed(upper) || (isDoor(upper) && !doorUpper(upper)) || isCrop(upper) || upper==Block::Fence || isGate(upper)) breakBlock(world,above);
  return true;
}
bool toggleDoor(World& world,const Player& player,Cell c) {
  auto b=world.get(c); if(!isDoor(b)) return false;
  c=doorBase(c,b); Cell top=c+Cell{0,1,0};
  if(world.get(top)!=doorVariant(doorAlongX(b),doorOpen(b),true)) return false;
  auto lower=doorVariant(doorAlongX(b),!doorOpen(b),false),upper=doorVariant(doorAlongX(b),!doorOpen(b),true);
  if(player.overlaps(c,lower) || player.overlaps(top,upper)) return false;
  if(chickensOverlap(world,blockBounds(c,lower)) || chickensOverlap(world,blockBounds(top,upper))) return false;
  world.set(c,lower); world.set(top,upper); return true;
}
SleepResult bedSleepStatus(const World& world,const Player& player,Cell c) {
  auto b=world.get(c);
  if(!isBed(b)) return SleepResult::NotABed;
  Cell other=bedOther(c,b);
  if(world.get(other)!=bedVariant(bedAlongX(b),!bedHead(b))) return SleepResult::NotABed;
  if(glm::length(middle(c)-player.eye())>3.5f) return SleepResult::TooFar;
  for(Cell half : {c,other}) {
    if(!support(world.get(half+Cell{0,-1,0}))) return SleepResult::Obstructed;
    for(int y=1;y<=2;++y) if(collidable(world.get(half+Cell{0,y,0}))) return SleepResult::Obstructed;
  }
  return world.clock.canSleep() ? SleepResult::Ready : SleepResult::Daytime;
}
SleepResult sleepInBed(World& world,const Player& player,Cell cell) {
  auto status=bedSleepStatus(world,player,cell);
  if(status==SleepResult::Ready) {
    world.clock.wakeAtMorning(); world.guideFlags|=Slept;
  }
  return status;
}
const std::vector<BlueprintPiece>& cabinBlueprint() {
  static const auto pieces=[] {
    std::vector<BlueprintPiece> result;
    for(int z=-6;z<=-2;++z) for(int x=8;x<=12;++x) result.push_back({{x,24,z},Block::Planks,CabinPart::Frame});
    for(int y=25;y<=27;++y) for(int z=-6;z<=-2;++z) for(int x=8;x<=12;++x) {
      if(x!=8 && x!=12 && z!=-6 && z!=-2) continue;
      if(z==-2 && x==10 && y<27) continue;
      bool corner=(x==8 || x==12) && (z==-6 || z==-2);
      bool window=y==26 && (((x==8 || x==12) && z==-4) || (z==-6 && x==10) || (z==-2 && (x==9 || x==11)));
      result.push_back({{x,y,z},window ? Block::Glass : corner ? Block::Wood : Block::Planks,window ? CabinPart::Windows : CabinPart::Frame});
    }
    for(int layer=0;layer<3;++layer) for(int z=-6+layer;z<=-2-layer;++z) for(int x=8+layer;x<=12-layer;++x)
      result.push_back({{x,28+layer,z},Block::Wood,CabinPart::Roof});
    result.push_back({{10,25,-2},Block::DoorZ,CabinPart::Door});
    result.push_back({{9,25,-5},Block::Torch,CabinPart::Lamp});
    result.push_back(cabinBed);
    return result;
  }();
  return pieces;
}
bool pieceComplete(const World& world,const BlueprintPiece& p) {
  auto block=world.get(p.cell);
  if(p.part==CabinPart::Frame || p.part==CabinPart::Roof) return frameMaterial(block);
  if(p.part==CabinPart::Door) return isDoor(block) && !doorUpper(block)
    && world.get(p.cell+Cell{0,1,0})==doorVariant(doorAlongX(block),doorOpen(block),true);
  if(p.part==CabinPart::Bed) return isBed(block) && !bedHead(block)
    && world.get(bedOther(p.cell,block))==bedVariant(bedAlongX(block),true);
  return block==p.block;
}
CabinStatus inspectCabin(const World& world) {
  CabinStatus status;
  for(const auto& p : cabinBlueprint()) {
    auto& part=status.parts[int(p.part)]; ++part.total; ++status.total;
    if(pieceComplete(world,p)) { ++part.done; ++status.done; }
  }
  return status;
}
bool initializeHome(World& world) {
  if(!world.terrain.adventure()) return false;
  world.ensure({0,-1},1);
  // Complete only an untouched starter site. Never rebuild a modified home.
  for(int y=24;y<=30;++y) for(int z=-6;z<=-2;++z) for(int x=8;x<=12;++x)
    if(world.edited({x,y,z})) return false;
  if(chickensOverlap(world,{{8,24,-6},{13,31,-1}})) return false;
  for(const auto& piece : cabinBlueprint()) {
    world.set(piece.cell,piece.block);
    if(isDoor(piece.block)) world.set(piece.cell+Cell{0,1,0},Block::DoorZTop);
    if(isBed(piece.block)) world.set(bedOther(piece.cell,piece.block),Block::BedZHead);
  }
  world.guideFlags|=Looked|Walked|Broke|Placed|BuiltCabin;
  return true;
}
bool visitHome(World& world,Player& player) {
  if(!world.terrain.adventure()) return false;
  world.ensure({0,-1},1);
  for(auto spot : {glm::vec3(10.5f,25,-3.5f),{9.5f,25,-3.5f},{10.5f,24,1.5f},{10.5f,24,7.5f}}) {
    for(int up=0;up<5;++up) {
      auto candidate=spot+glm::vec3(0,up,0);
      Cell floor{int(std::floor(candidate.x)),int(candidate.y)-1,int(std::floor(candidate.z))};
      Box body{candidate+glm::vec3(-.3f,0,-.3f),candidate+glm::vec3(.3f,1.8f,.3f)};
      if(!support(world.get(floor)) || player.collides(world,candidate) || chickensOverlap(world,body)) continue;
      player.pose.position=candidate; player.pose.yaw=.7f; player.pose.pitch=-.35f;
      player.stopFlying(); return true;
    }
  }
  return false;
}
void Adventure::looked(World& world,float dx,float dy) {
  lookDistance_+=std::abs(dx)+std::abs(dy);
  if(lookDistance_>=100) world.guideFlags|=Looked;
}
void Adventure::moved(World& world,glm::vec3 before,glm::vec3 after) {
  walkDistance_+=glm::length(glm::vec2(after.x-before.x,after.z-before.z));
  if(walkDistance_>=3.f) world.guideFlags|=Walked;
}
void Adventure::update(World& world,const Player& player) {
  if(!world.terrain.adventure()) return;
  // Count the actual structure, never cumulative placement clicks.
  if(inspectCabin(world).complete()) world.guideFlags|=BuiltCabin;
  if((world.guideFlags&BuiltCabin) && glm::length(player.pose.position-glm::vec3(36.5f,24.f,-3.5f))<3.5f)
    world.guideFlags|=VisitedCave;
}
int Adventure::stage(const World& world) const {
  for(int i=0;i<4;++i) if(!(world.guideFlags&(1u<<i))) return i;
  if(!(world.guideFlags&BuiltCabin)) return 4;
  if(!(world.guideFlags&VisitedCave)) return 5;
  return world.guideFlags&Slept ? 7 : 6;
}
std::optional<BlueprintPiece> Adventure::nextPiece(const World& world,const Player& player) const {
  std::optional<BlueprintPiece> best;
  float closest=std::numeric_limits<float>::infinity();
  // Existing completed cabins can add the new bed without rebuilding old edits.
  if(stage(world)==6) return pieceComplete(world,cabinBed) ? std::nullopt : std::optional(cabinBed);
  // Build the enclosure before the roof, then furnish the cabin.
  for(int part=0;part<int(CabinPart::Count);++part) {
    for(const auto& p : cabinBlueprint()) {
      if(int(p.part)!=part || pieceComplete(world,p)) continue;
      float distance=glm::length(middle(p.cell)-player.eye());
      if(distance<closest) { closest=distance; best=p; }
    }
    if(best) return best;
  }
  return {};
}
bool Adventure::buildNext(World& world,const Player& player) const {
  if(!world.terrain.adventure() || (stage(world)!=4 && stage(world)!=6)) return false;
  auto p=nextPiece(world,player);
  if(!p || glm::length(middle(p->cell)-player.eye())>7.f) return false;
  return placeBlock(world,player,p->cell,p->block,false);
}
GuideView Adventure::view(const World& world,const Player& player) const {
  GuideView out; out.enabled=world.terrain.adventure(); if(!out.enabled) return out;
  out.stage=stage(world); out.cabin=inspectCabin(world);
  out.destination=glm::vec3(10.5f,25.f,-1.5f); out.destinationName="CABIN SITE";
  switch(out.stage) {
    case 0:
      out.title="01 / LOOK AROUND";
      out.lines={"Move your mouse to turn your head.","The wooden frame is your cabin site.","Build at your own pace."}; break;
    case 1:
      out.title="02 / TAKE A FEW STEPS";
      out.lines={"Use ARROWS or W A S D to walk.","Press SPACE to jump onto a block.","The sandy path leads to your cabin."}; break;
    case 2:
      out.title="03 / TRY BREAKING A BLOCK";
      out.lines={"Aim the crosshair at the oak stump.","Left-click once to remove the block.","Hold to remove more blocks."};
      out.destination=glm::vec3(7.5f,25.f,3.5f); out.destinationName="PRACTICE STUMP"; break;
    case 3:
      out.title="04 / PLACE YOUR FIRST BLOCK";
      out.lines={"Choose planks from your hotbar or E.","Aim at the ground. Right-click or V.","The green outline shows what you build."}; break;
    case 4: {
      out.title="05 / BUILD YOUR FIRST CABIN";
      auto next=nextPiece(world,player);
      out.lines={"Hold G to place the glowing pieces.","Or choose blocks with 1-9 and build.","Walk around the frame as you go."};
      if(next) {
        out.preview=next->cell; out.previewBlock=next->block;
        out.destination=middle(next->cell); out.destinationName=std::string(blockName(next->block));
        float distance=glm::length(*out.destination-player.eye());
        if(solid(world.get(next->cell))) out.lines[0]="Remove the block in the glowing spot.";
        else if(isDoor(next->block) && solid(world.get(next->cell+Cell{0,1,0}))) out.lines[0]="Clear two blocks for the doorway.";
        else if(isBed(next->block) && solid(world.get(bedOther(next->cell,next->block)))) out.lines[0]="Clear both glowing spaces for the bed.";
        else if(player.overlaps(next->cell,next->block)
                || (isDoor(next->block) && player.overlaps(next->cell+Cell{0,1,0},Block::DoorZTop))
                || (isBed(next->block) && player.overlaps(bedOther(next->cell,next->block),Block::BedZHead)))
          out.lines[0]="Step out of the glowing building spot.";
        else if(distance>7.f) out.lines[0]="Walk closer to the glowing piece.";
      }
      break;
    }
    case 5:
      out.title="06 / THE LANTERN CAVE";
      out.lines={"Your cabin is ready. Make it yours.","Right-click or V opens the door.","Follow the path east to the cave."};
      out.destination=glm::vec3(27.f,25.f,-4.f); out.destinationName="LANTERN CAVE";
      if(player.pose.position.x>24 && std::abs(player.pose.position.z+4)<8 && player.pose.position.y<30) {
        out.title="06 / FOLLOW THE LANTERNS";
        out.lines={"Step into the lantern-lit chamber.","Torches help you explore underground.","Press R whenever you want to go home."};
        out.destination=glm::vec3(36.5f,25.f,-3.5f); out.destinationName="LANTERN CHAMBER";
      }
      break;
    case 6:
      out.title="07 / YOUR FIRST NIGHT";
      out.destination=middle(cabinBed.cell); out.destinationName="YOUR BED";
      if(!pieceComplete(world,cabinBed)) {
        out.preview=cabinBed.cell; out.previewBlock=cabinBed.block;
        out.lines={"Hold G to add a bed inside the cabin.","A bed needs two clear floor spaces.","Right-click or V opens the door."};
        if(solid(world.get(cabinBed.cell)) || solid(world.get(bedOther(cabinBed.cell,cabinBed.block))))
          out.lines[0]="Clear the two glowing spaces for a bed.";
      } else if(world.clock.canSleep()) {
        out.lines={"Your bed is ready for the night.","Right-click or V on the bed to sleep.","Wake up to birds and morning light."};
      } else {
        out.lines={"Explore until sunset, or keep building.","At dusk, right-click or V on your bed.","Sleep brings you straight to morning."};
      }
      break;
    default:
      out.title="YOUR ADVENTURE HAS BEGUN";
      out.lines={"Your house is ready. Make it your own.","E opens your items and crafting.","Press R anytime to return home."};
      out.destination.reset(); break;
  }
  return out;
}
} // namespace bw

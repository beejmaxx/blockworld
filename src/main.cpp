#include "renderer.hpp"
#include "audio.hpp"
#include "garden.hpp"
#include "ranch.hpp"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace bw {
namespace {
struct Options {
  int frames=0;
  bool smoke=false,save=true,paused=true,classic=false,demoCabin=false,demoCave=false,demoBed=false,demoFarm=false,muted=false;
  std::optional<double> hour;
  std::filesystem::path screenshot,worldDirectory;
};
Options parse(int argc,char** argv) {
  Options result;
  for(int i=1;i<argc;++i) {
    std::string arg=argv[i];
    auto value=[&]() -> std::string { if(i+1>=argc) throw std::runtime_error("Missing value for "+arg); return argv[++i]; };
    if(arg=="--smoke-test") { result.smoke=true; result.save=false; result.paused=false; }
    else if(arg=="--no-save") result.save=false;
    else if(arg=="--play") result.paused=false;
    else if(arg=="--classic") result.classic=true;
    else if(arg=="--mute") result.muted=true;
    else if(arg=="--time") {
      auto text=value(); double hour=0; auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),hour);
      if(error!=std::errc{} || end!=text.data()+text.size() || !std::isfinite(hour) || hour<0 || hour>=24)
        throw std::runtime_error("--time requires an hour from 0 to less than 24");
      result.hour=hour;
    }
    else if(arg=="--demo-cabin" || arg=="--demo-cave" || arg=="--demo-bed" || arg=="--demo-farm") {
      result.demoCabin=true; result.demoCave=arg=="--demo-cave"; result.demoBed=arg=="--demo-bed"; result.demoFarm=arg=="--demo-farm"; result.save=false; result.paused=false;
    }
    else if(arg=="--screenshot") result.screenshot=value();
    else if(arg=="--world-dir") result.worldDirectory=value();
    else if(arg=="--frames") {
      auto text=value(); auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),result.frames);
      if(error!=std::errc{} || end!=text.data()+text.size() || result.frames<1) throw std::runtime_error("--frames requires a positive integer");
    } else if(arg=="--help") {
      std::cout<<"Blockworld\n  --play                 Start without the controls overlay\n"
        <<"  --frames N             Exit after N frames\n  --screenshot FILE.bmp  Capture the last frame (requires --frames)\n"
        <<"  --smoke-test           Exercise Metal, edits and resize without touching saves\n"
        <<"  --classic              Open the original sandbox and world.bw save\n"
        <<"  --demo-cabin           Preview a finished cabin in a temporary world\n"
        <<"  --demo-cave            Preview the lantern cave in a temporary world\n"
        <<"  --demo-bed             Preview the furnished cabin interior\n"
        <<"  --demo-farm            Visit a temporary chicken pen and mixed garden\n"
        <<"  --time HOURS           Set the starting time (0 to less than 24)\n"
        <<"  --mute                 Start with sound muted (M toggles sound)\n"
        <<"  --no-save              Use a temporary world\n  --world-dir DIRECTORY  Override the save directory\n";
      std::exit(0);
    } else throw std::runtime_error("Unknown option: "+arg);
  }
  if(result.smoke && result.frames==0) result.frames=180;
  if(result.smoke && result.frames<180) throw std::runtime_error("Smoke test needs at least 180 frames");
  if(result.classic && result.demoCabin) throw std::runtime_error("Cabin previews use the guided meadow, not --classic");
  if(!result.screenshot.empty() && result.frames==0) throw std::runtime_error("--screenshot requires --frames");
  return result;
}
struct SdlLifetime { ~SdlLifetime() { SDL_Quit(); } };
void demoCabin(World& world) {
  Player builder; builder.pose.position={17,24,4};
  for(const auto& piece : cabinBlueprint()) if(!pieceComplete(world,piece))
    if(!placeBlock(world,builder,piece.cell,piece.block)) throw std::runtime_error("Cabin demo could not place a piece");
  world.guideFlags=Looked|Walked|Broke|Placed|BuiltCabin;
}
void cabinView(Player& player) {
  player.pose.position={16.5f,24,5.5f}; player.pose.yaw=-.60f; player.pose.pitch=.03f;
  player.pose.flying=false; player.velocity={};
}
void bedView(Player& player) {
  player.pose.position={9.7f,25,-3.25f}; player.pose.yaw=.96f; player.pose.pitch=-.44f;
  player.pose.flying=false; player.velocity={};
}
std::string sleepMessage(SleepResult status) {
  switch(status) {
    case SleepResult::Daytime: return "YOU CAN SLEEP AT DUSK / COME BACK AFTER SUNSET";
    case SleepResult::TooFar: return "STEP CLOSER TO THE BED";
    case SleepResult::Obstructed: return "CLEAR TWO BLOCKS ABOVE BOTH HALVES OF THE BED";
    case SleepResult::NotABed: return "THE BED NEEDS BOTH HALVES AND A SOLID FLOOR";
    default: return "REST WELL";
  }
}

int run(const Options& options) {
  const bool interactive=!options.smoke && options.frames==0;
  SDL_SetAppMetadata("Blockworld","0.10.0","dev.bijan.blockworld");
  if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) throw std::runtime_error(SDL_GetError());
  SdlLifetime sdl;
  std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
    SDL_CreateWindow("Blockworld",1280,800,SDL_WINDOW_RESIZABLE),SDL_DestroyWindow);
  if(!window) throw std::runtime_error(SDL_GetError());
  SDL_SetWindowMinimumSize(window.get(),800,600);

  World world(7262026,!options.classic);
  Player player;
  if(options.classic) player.pose.position={8.5f,float(world.terrain.height(8,8)+1),8.5f};
  else { player.pose.position={10.5f,24,7.5f}; player.pose.yaw=0; player.pose.pitch=-.12f; }
  std::filesystem::path savePath;
  bool loaded=false;
  if(options.save) {
    if(options.worldDirectory.empty()) {
      char* path=SDL_GetPrefPath("Bijan","Blockworld");
      if(!path) throw std::runtime_error(SDL_GetError());
      savePath=std::filesystem::path(path)/(options.classic ? "world.bw" : "meadow.bw"); SDL_free(path);
    } else savePath=options.worldDirectory/(options.classic ? "world.bw" : "meadow.bw");
    if(auto pose=world.load(savePath)) { player.pose=*pose; loaded=true; }
    std::cout<<"Save: "<<savePath<<'\n';
  }
  auto center=chunkAt(int(std::floor(player.pose.position.x)),int(std::floor(player.pose.position.z)));
  world.ensure(center,3);
  initializeFarm(world);
  if(!options.smoke && !options.demoCabin) initializeHome(world);
  if(!options.smoke && (!options.demoCabin || options.demoFarm) && initializeGarden(world)) {
    visitGarden(world,player);
    if(!loaded) world.inventory.slots={Item::Hoe,Item::Carrot,Item::WateringCan,Item::Strawberry,Item::Pumpkin,Item::Compost,Item::Wheat,Item::Planks,Item::Glass};
    equipItem(world.inventory,world.crafting,Item::Hoe);
  }
  if(options.demoCabin) {
    demoCabin(world); cabinView(player);
    if(options.demoCave) {
      player.pose.position={32.5f,24,-3.5f}; player.pose.yaw=1.5707963f; player.pose.pitch=-.08f;
    }
    if(options.demoBed) { bedView(player); world.guideFlags|=VisitedCave; }
    if(options.demoFarm) {
      Player builder; builder.pose.position={16,24,15};
      for(const auto& piece : penBlueprint(world)) placeBlock(world,builder,piece.cell,piece.block);
      for(int x=7;x<=8;++x) for(int z=6;z<=9;++z) { world.set({x,23,z},Block::Farmland); placeBlock(world,builder,{x,24,z},Block::WheatYoung); }
      growFarm(world,wheatGrowSeconds); world.farm.wheat=6; world.farm.flags|=PenBuilt|WheatHarvested;
      if(!world.farm.chickens.empty()) world.farm.chickens.front().eggReady=true;
      world.farm.eggs=3;
      if(incubateEgg(world,0)) growFarm(world,eggHatchSeconds+20);
      incubateEgg(world,1);
      for(auto kind : cropKinds) for(int z=6;z<=9;++z) {
        int x=kind==CropKind::Pumpkin ? 11 : 7+int(kind);
        Cell cell{x,24,z};
        if(isCrop(world.get(cell))) world.set(cell,Block::Air);
        world.set(cell+Cell{0,-1,0},Block::Farmland);
        world.set(cell,cropStage(kind,0));
        if(z>=7) world.set(cell,cropStage(kind,cropGrowSeconds(kind)*(z==7 ? .5f : 1.f)));
        if(z==6) world.waterCrop(cell);
      }
      equipItem(world.inventory,world.crafting,Item::WateringCan);
      visitGarden(world,player);
    }
  }
  if(options.hour) world.clock.phase=*options.hour/24.;
  // Recover gracefully if the saved player is inside a newly placed block.
  while(player.collides(world,player.pose.position) && player.pose.position.y<worldHeight+2) player.pose.position.y+=1;
  Renderer renderer(window.get());
  Audio audio;
  renderer.sync(world,center,1000);
  ChunkWorker worker(world.terrain); worker.request(center,world);
  HudState hud; hud.paused=options.paused; hud.muted=options.muted;
  hud.farming=options.demoFarm || (!options.classic && !options.demoCabin && !options.smoke);
  hud.inventory=world.inventory;
  Adventure adventure;
  BuildRepeater removeInput;
  RideState ride;
  BuildRepeater buildInput;
  constexpr unsigned keyEditSource=1,mouseEditSource=2;
  unsigned placeHeld=0,removeHeld=0;
  DebrisCloud debris;
  bool running=true;
  float noticeTime=0,saveTime=0,totalTime=0,guideCooldown=0;
  float stride=0,sleepRemaining=0;
  bool repeatedUse=false;
  double lastJumpPress=-1;
  constexpr float toolSwingSeconds=.32f;
  float toolSwingRemaining=0;
  float cluckTimer=3,rainTimer=0;
  std::size_t cluckBird=0;
  bool leftFoot=false,sleepApplied=false;
  std::optional<Cell> sleepingBed;
  std::optional<std::size_t> trackedAnimal;
  auto knownFlockSize=world.farm.chickens.size();
  int frame=0,fpsFrames=0;
  int previousStage=adventure.stage(world);
  std::optional<ChunkPos> previousCenter;
  double fpsTime=0;
  Uint64 previous=SDL_GetPerformanceCounter();
  const double frequency=double(SDL_GetPerformanceFrequency());
  auto cancelEdits=[&] {
    buildInput.cancel(); removeInput.cancel(); placeHeld=0; removeHeld=0; repeatedUse=false; toolSwingRemaining=0;
    lastJumpPress=-1;
  };
  auto notice=[&](std::string message) { hud.notice=std::move(message); noticeTime=3.f; };
  auto capture=[&](bool active) {
    if(interactive && !SDL_SetWindowRelativeMouseMode(window.get(),active)) throw std::runtime_error(SDL_GetError());
  };
  auto save=[&] {
    if(!options.save) return;
    try { world.save(savePath,player.pose); notice("WORLD SAVED"); }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; notice("SAVE FAILED - SEE LOG"); }
    saveTime=0;
  };
  auto soundAt=[&](Sound sound,Block block,Cell cell) {
    glm::vec3 delta=glm::vec3(cell.x+.5f,cell.y+.5f,cell.z+.5f)-player.eye();
    float distance=glm::length(delta);
    float pan=glm::dot(delta,glm::vec3(std::cos(player.pose.yaw),0,std::sin(player.pose.yaw)))/std::max(distance,1.f);
    audio.play(sound,block,std::clamp(1.f-distance*.07f,.3f,1.f),pan*.65f);
  };
  auto useDoor=[&](Cell cell) {
    if(!toggleDoor(world,player,cell)) return false;
    soundAt(Sound::Door,Block::Wood,cell); return true;
  };
  auto requestSleep=[&](Cell cell) {
    auto status=bedSleepStatus(world,player,cell);
    if(status!=SleepResult::Ready) { notice(sleepMessage(status)); return false; }
    sleepingBed=cell; sleepRemaining=3.2f; sleepApplied=false; player.velocity={};
    audio.play(Sound::Sleep); return true;
  };
  auto broke=[&](BreakEvent event) {
    if(toolSwingRemaining<=0) toolSwingRemaining=toolSwingSeconds;
    soundAt(Sound::Break,event.block,event.cell); debris.emit(event.cell,event.block);
    if(event.block==Block::Wood) notice("LOG COLLECTED / E FOR INVENTORY / CRAFTING");
    else if(event.block==Block::Stone || event.block==Block::Brick) notice("STONE COLLECTED / E FOR INVENTORY / CRAFTING");
  };
  auto endNaming=[&] {
    if(hud.naming) SDL_StopTextInput(window.get());
    hud.naming=false; hud.nameSelectedAll=false; hud.nameDraft.clear();
  };
  auto showMenu=[&](Menu menu) {
    endNaming();
    if(menu==Menu::Farm) {
      hud.farming=true; hud.help=true; hud.farm=farmView(world);
      hud.animalSelected=std::clamp(hud.animalSelected,0,std::max(0,int(world.farm.chickens.size())-1));
      hud.farmPage=hud.animalSelected/6;
    }
    hud.menu=menu; hud.paused=false; hud.carried.reset(); hud.inventoryHover=-1;
    cancelEdits(); player.velocity={}; capture(menu==Menu::None);
  };
  auto beginNaming=[&] {
    if(hud.naming) return;
    if(hud.animalSelected<0 || hud.animalSelected>=int(world.farm.chickens.size())) return;
    hud.nameDraft=animalName(world.farm.chickens[hud.animalSelected],std::size_t(hud.animalSelected));
    if(!SDL_StartTextInput(window.get())) { notice("COULD NOT START NAME INPUT"); return; }
    hud.naming=true; hud.nameSelectedAll=true;
  };
  auto saveName=[&] {
    if(!hud.naming) return;
    if(!renameChicken(world,std::size_t(hud.animalSelected),hud.nameDraft)) {
      notice("CHOOSE A NAME WITH 1-18 LETTERS OR NUMBERS"); return;
    }
    endNaming(); notice("NAME SAVED / LOOK FOR IT ABOVE YOUR ANIMAL");
  };
  auto appendName=[&](std::string_view input) {
    for(unsigned char c : input) {
      bool allowed=(c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c==' ' || c=='-' || c=='.' || c=='\'';
      if(!allowed) { notice("NAMES USE LETTERS, NUMBERS, SPACES, - OR ."); continue; }
      if(hud.nameSelectedAll) { hud.nameDraft.clear(); hud.nameSelectedAll=false; }
      if(hud.nameDraft.size()<animalNameLimit) hud.nameDraft.push_back(char(c));
    }
  };
  auto goHome=[&] {
    if(!visitHome(world,player)) { notice("HOME LANDING IS BLOCKED / CLEAR SOME SPACE"); return; }
    ride.active=false; world.farm.car.speed=0;
    trackedAnimal.reset(); hud.farming=false; hud.help=true;
    if(hud.menuOpen()) showMenu(Menu::None);
    stride=0; cancelEdits();
    notice("HOME / RIGHT CLICK OR V ON YOUR BED TO SLEEP AT NIGHT");
  };
  auto farmAction=[&](FarmAction action) {
    if(action==FarmAction::RanchShop || action==FarmAction::GardenShop) { hud.farmRanch=action==FarmAction::RanchShop; return; }
    if(action==FarmAction::BuyCow || action==FarmAction::BuyHorse || action==FarmAction::Car) {
      if(ride.active && !leaveRide(world,player,ride)) { notice("Move to open ground before getting out"); return; }
      auto problem=action==FarmAction::Car ? bringCar(world,player) : buyLivestock(world,player,action==FarmAction::BuyCow ? LivestockKind::Cow : LivestockKind::Horse);
      if(!problem.empty()) { notice(problem); return; }
      auto destination=action==FarmAction::Car ? world.farm.car.position : world.farm.livestock.back().position;
      auto delta=glm::normalize(destination+glm::vec3(0,1,0)-player.eye());
      player.pose.yaw=std::atan2(delta.x,-delta.z); player.pose.pitch=std::asin(delta.y);
      showMenu(Menu::None); audio.play(Sound::Place);
      notice(action==FarmAction::Car ? "Your car is here / V to drive / V to get out"
        : action==FarmAction::BuyCow ? "Cow delivered / V to collect milk / Sell milk in the shop" : "Horse delivered / V to ride / V to get off");
      return;
    }
    if(action==FarmAction::Name) { beginNaming(); return; }
    if(action==FarmAction::SaveName) { saveName(); return; }
    endNaming();
    if(action==FarmAction::Garden || action==FarmAction::Animals || action==FarmAction::Shop) {
      hud.farmShop=action==FarmAction::Shop; hud.farmGarden=action!=FarmAction::Animals; trackedAnimal.reset(); return;
    }
    auto takeGardenTool=[&](Item item) {
      equipItem(world.inventory,world.crafting,item); hud.farming=true; hud.farmGarden=true; hud.help=true;
      showMenu(Menu::None);
      notice(item==Item::Hoe ? "HOE READY / RIGHT CLICK OR V ON GRASS TO PREPARE SOIL"
        : item==Item::Compost ? "COMPOST READY / V ON A GROWING CROP / TWO EXTRA PER HARVEST"
        : item==Item::Sprinkler ? "SPRINKLER READY / V ON GROUND / WATERS A 5 BY 5 AREA"
        : "GREENHOUSE KIT / V ON LEVEL SOIL / EXTENDS 4 BLOCKS NORTH");
    };
    if(action==FarmAction::Sell) {
      int coins=sellBasket(world);
      if(coins) { audio.play(Sound::Place); notice("HARVEST SOLD / +"+std::to_string(coins)+" COINS"); }
      else notice(basketValue(world.farm)>0 ? "COIN WALLET FULL / SPEND SOME COINS FIRST" : "YOUR BASKET IS EMPTY / PICK SOME RIPE CROPS");
      return;
    }
    if(action>=FarmAction::BuyCompost && action<=FarmAction::BuyGreenhouse) {
      auto purchase=GardenPurchase(int(action)-int(FarmAction::BuyCompost));
      if(buyGardenSupply(world,purchase)) takeGardenTool(purchase==GardenPurchase::Compost ? Item::Compost : purchase==GardenPurchase::Sprinkler ? Item::Sprinkler : Item::Greenhouse);
      else notice(world.farm.garden.coins<gardenPrice(purchase) ? "SELL MORE HARVESTS TO EARN COINS" : "YOU ALREADY HAVE A FULL SUPPLY");
      return;
    }
    if(action==FarmAction::Hoe || action==FarmAction::Compost || action==FarmAction::Sprinkler || action==FarmAction::Greenhouse) {
      takeGardenTool(action==FarmAction::Hoe ? Item::Hoe : action==FarmAction::Compost ? Item::Compost : action==FarmAction::Sprinkler ? Item::Sprinkler : Item::Greenhouse); return;
    }
    if(action==FarmAction::Visit) {
      if(visitGarden(world,player)) { ride.active=false; world.farm.car.speed=0; hud.farming=true; hud.farmGarden=true; hud.farmShop=false; hud.help=true; trackedAnimal.reset(); showMenu(Menu::None); notice("YOUR VEGETABLE GARDEN / P FOR SEEDS AND TOOLS"); }
      else notice("NO CLEAR GARDEN LANDING / USE THE HOE ON NEARBY GRASS");
      return;
    }
    if(action>=FarmAction::Seeds && action<=FarmAction::Gates) {
      constexpr std::array items{Item::Wheat,Item::Carrot,Item::Strawberry,Item::Pumpkin,Item::WateringCan,Item::Fence,Item::Gate};
      auto item=items[int(action)-int(FarmAction::Seeds)];
      equipItem(world.inventory,world.crafting,item); hud.farming=true; hud.help=true; hud.farmGarden=isCrop(itemBlock(item)) || item==Item::WateringCan;
      showMenu(Menu::None);
      notice(item==Item::WateringCan ? "WATERING CAN READY / RIGHT CLICK OR V ON A PLANT"
             : isCrop(itemBlock(item)) ? "SEEDS READY / RIGHT CLICK OR V ON PREPARED SOIL" : "RIGHT CLICK OR V TO BUILD / G HELPS BUILD THE PEN");
    } else if(action==FarmAction::Hatch) {
      if(incubateEgg(world,std::size_t(hud.animalSelected))) {
        audio.play(Sound::Cluck); notice("EGG KEPT WARM / RETURN TO THE WORLD FOR 60 SECONDS");
      } else notice(hatchProblem(world,std::size_t(hud.animalSelected)));
    } else if(action==FarmAction::Find && hud.animalSelected>=0 && hud.animalSelected<int(world.farm.chickens.size())) {
      trackedAnimal=std::size_t(hud.animalSelected); hud.farming=true; hud.help=true;
      showMenu(Menu::None); notice("FOLLOW THE ANIMAL MARKER / R RETURNS TO THE MEADOW");
    } else if(action==FarmAction::Previous && hud.farmPage>0) { --hud.farmPage; hud.animalSelected=hud.farmPage*6; }
    else if(action==FarmAction::Next && (hud.farmPage+1)*6<int(world.farm.chickens.size())) { ++hud.farmPage; hud.animalSelected=hud.farmPage*6; }
    else if(action==FarmAction::Cabin) goHome();
  };
  auto makeRecipe=[&](int index) {
    hud.recipeSelected=index;
    auto recipe=Recipe(index);
    auto problem=craftProblem(world,player,recipe);
    if(!problem.empty()) { notice(problem); return; }
    if(craft(world,player,recipe)) {
      audio.play(Sound::Place,Block::Wood);
      notice(std::string(recipes()[index].name)+" READY / E TO RETURN TO THE WORLD");
      auto item=recipe==Recipe::Workbench ? Item::Workbench : recipe==Recipe::Axe ? Item::Axe
        : recipe==Recipe::Pickaxe ? Item::Pickaxe : Item::Planks;
      equipItem(world.inventory,world.crafting,item); cancelEdits();
    }
  };
  auto pointerPixels=[&](float x,float y) {
    int w=0,h=0,pw=0,ph=0;
    SDL_GetWindowSize(window.get(),&w,&h); SDL_GetWindowSizeInPixels(window.get(),&pw,&ph);
    hud.width=pw; hud.height=ph;
    return glm::vec2(x*float(pw)/float(std::max(w,1)),y*float(ph)/float(std::max(h,1)));
  };
  auto putInSlot=[&](int slot,Item item) {
    if(assignItem(world.inventory,world.crafting,slot,item)) { hud.carried.reset(); cancelEdits(); }
  };
  auto edit=[&](bool place) {
    auto hit=place ? world.raycast(player.eye(),player.direction()) : miningTarget(world,player);
    if(!hit || ride.active) return false;
    if(place) {
      bool alongX=std::abs(player.direction().x)>std::abs(player.direction().z);
      auto status=placementStatus(world,player,hit->adjacent,itemBlock(world.inventory.held()),alongX);
      if(status!=PlacementStatus::Ready) { notice(std::string(placementMessage(status))); return false; }
      bool changed=placeBlock(world,player,hit->adjacent,itemBlock(world.inventory.held()),alongX);
      if(changed) { world.guideFlags|=Placed; soundAt(Sound::Place,itemBlock(world.inventory.held()),hit->adjacent); }
      return changed;
    }
    auto material=world.get(hit->block);
    bool changed=breakBlock(world,hit->block);
    if(changed) { world.guideFlags|=Broke; collectMaterial(world,material); broke({hit->block,material}); }
    return changed;
  };
  auto use=[&](bool repeating) {
    if(ride.active) {
      if(!repeating) { notice(leaveRide(world,player,ride) ? "Back on foot" : "Move to open ground to get out"); cancelEdits(); }
      return;
    }
    const bool* keys=SDL_GetKeyboardState(nullptr);
    bool sneak=keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    auto target=useTarget(world,player,world.inventory.held(),sneak);
    if(target.kind==UseKind::Place) { edit(true); return; }
    if(target.kind==UseKind::Till) {
      if(tillSoil(world,player,target.cell)) {
        debris.emit(target.cell,Block::Dirt); toolSwingRemaining=toolSwingSeconds; soundAt(Sound::Place,Block::Dirt,target.cell);
        notice("SOIL PREPARED / CHOOSE SEEDS AND RIGHT CLICK OR V TO PLANT");
      } else if(!repeating) notice(target.distance>3.5f ? "STEP CLOSER TO PREPARE SOIL" : "USE THE HOE ON CLEAR GRASS OR DIRT");
      return;
    }
    if(target.kind==UseKind::Compost) {
      if(applyCompost(world,player,target.cell)) {
        toolSwingRemaining=toolSwingSeconds; soundAt(Sound::Place,Block::Dirt,target.cell); notice("COMPOST ADDED / TWO EXTRA AT THE NEXT HARVEST");
      } else if(!repeating) notice(target.distance>3.5f ? "STEP CLOSER TO COMPOST THE PLANT" : world.farm.garden.compost<=0 ? "COMPOST BAG EMPTY / P / SHOP FOR MORE" : "THIS PLANT ALREADY HAS COMPOST");
      return;
    }
    if(target.kind==UseKind::Water) {
      if(waterCrop(world,player,target.cell)) {
        debris.water(target.cell); toolSwingRemaining=toolSwingSeconds;
        soundAt(Sound::Water,Block::Leaves,target.cell); notice("WATERED / DOUBLE GROWTH FOR 45 SECONDS");
      } else if(!repeating) notice(target.distance>3.5f ? "STEP CLOSER TO WATER THE PLANT" : cropPrompt(world,target.cell,true));
      return;
    }
    if(repeating) return; // Holding use must not repeatedly toggle doors or gates.
    switch(target.kind) {
      case UseKind::Ranch: {
        auto animal=targetRanch(world,player);
        if(!animal) break;
        if(!animal->car && world.farm.livestock[animal->index].kind==LivestockKind::Cow) {
          if(collectMilk(world,player,animal->index)) { audio.play(Sound::Place); notice("Milk collected / Worth 5 coins / Sell basket in the shop"); }
          else notice(ranchPrompt(world,*animal));
        } else if(mountRanch(world,player,ride,*animal)) {
          cancelEdits(); notice("WASD or arrows to drive / Space brake / V to get out");
        } else notice("Clear some space above the seat first");
        break;
      }
      case UseKind::Greenhouse:
        if(placeGreenhouse(world,player,target.cell)) { toolSwingRemaining=toolSwingSeconds; audio.play(Sound::Place,Block::Glass); notice("GREENHOUSE BUILT / PLANT INSIDE FOR 25% EXTRA GROWTH"); }
        else { auto problem=greenhouseProblem(world,player,target.cell); notice(problem.empty() ? "STEP CLOSER TO PLACE THE GREENHOUSE" : problem); }
        break;
      case UseKind::Door: if(!useDoor(target.cell)) notice("STEP CLOSER OR CLEAR OF THE DOOR"); break;
      case UseKind::Gate:
        if(toggleGate(world,player,target.cell)) soundAt(Sound::Door,Block::Wood,target.cell);
        else notice(target.distance>3.5f ? "STEP CLOSER TO THE GATE" : "KEEP THE GATE CLEAR OF YOU AND THE CHICKENS");
        break;
      case UseKind::Bed: requestSleep(target.cell); break;
      case UseKind::Workbench:
        if(target.distance<=3.5f) showMenu(Menu::Crafting); else notice("STEP CLOSER TO THE WORKBENCH");
        break;
      case UseKind::Chicken: {
        if(!isChick(world.farm.chickens[target.chicken]) && world.farm.chickens[target.chicken].hatchTimer<0
           && !world.farm.chickens[target.chicken].eggReady && world.inventory.held()!=Item::Wheat) {
          notice("SELECT WHEAT IN YOUR HOTBAR TO FEED THE CHICKEN"); break;
        }
        auto result=useChicken(world,player,target.chicken);
        if(result==FarmUse::Fed) { audio.play(Sound::Cluck); notice("HAPPY CHICKEN / AN EGG WILL BE READY IN 30 SECONDS"); }
        else if(result==FarmUse::Petted) { audio.play(Sound::Cluck); notice(animalName(world.farm.chickens[target.chicken],target.chicken)+" LIKES YOU / P TO GIVE A NAME"); }
        else if(result==FarmUse::Collected) { audio.play(Sound::Place); notice("EGG COLLECTED / YOUR FARM IS GROWING"); }
        else notice(chickenPrompt(world,target.chicken));
        break;
      }
      case UseKind::Crop: {
        auto before=world.get(target.cell); auto kind=cropKind(before); int quantity=world.farm.harvest(kind);
        if(harvestCrop(world,player,target.cell)==FarmUse::Harvested) {
          toolSwingRemaining=toolSwingSeconds; soundAt(Sound::Place,Block::Leaves,target.cell); debris.emit(target.cell,before);
          notice("+"+std::to_string(world.farm.harvest(kind)-quantity)+" "+std::string(cropName(kind))+(kind==CropKind::Strawberry ? " / BUSH WILL FRUIT AGAIN" : " / BED READY FOR NEW SEEDS"));
        } else notice(target.distance>3.5f ? "STEP CLOSER TO HARVEST" : world.farm.harvest(kind)>9999-cropYield(kind)
            ? "HARVEST BASKET FULL / THIS CROP WILL WAIT" : cropPrompt(world,target.cell,false));
        break;
      }
      default: break;
    }
  };
  auto pressPlace=[&](unsigned source) {
    if(removeHeld) return;
    placeHeld|=source; repeatedUse=false; buildInput.press();
  };
  auto releasePlace=[&](unsigned source) {
    placeHeld&=~source;
    if(!placeHeld) buildInput.release();
  };
  auto pressRemove=[&](unsigned source) {
    if(ride.active) return;
    if(!removeHeld) {
      removeInput.press(); removeInput.tick(0,true); // Consume the immediate action exactly once.
      edit(false);
    }
    removeHeld|=source; buildInput.cancel(); placeHeld=0;
    toolSwingRemaining=toolSwingSeconds;
  };
  auto releaseRemove=[&](unsigned source) {
    removeHeld&=~source;
    if(!removeHeld) removeInput.cancel();
  };
  capture(!hud.paused);
  int smokeEdits=0;
  int smokeDoorActions=0;
  int smokeSleeps=0;
  const auto start=SDL_GetPerformanceCounter();
  while(running) {
    auto now=SDL_GetPerformanceCounter();
    float realDt=float(double(now-previous)/frequency); previous=now;
    float dt=std::min(realDt,.1f);
    if(!hud.paused && !hud.menuOpen()) totalTime+=dt;
    fpsTime+=realDt; ++fpsFrames;
    guideCooldown=std::max(0.f,guideCooldown-dt);
    if(!hud.paused && !hud.menuOpen()) toolSwingRemaining=std::max(0.f,toolSwingRemaining-dt);
    if(fpsTime>=.5) { hud.fps=float(double(fpsFrames)/fpsTime); fpsFrames=0; fpsTime=0; }
    if(noticeTime>0) { noticeTime-=dt; if(noticeTime<=0) hud.notice.clear(); }
    SDL_Event event;
    bool jump=false;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_EVENT_QUIT) running=false;
      if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST && interactive) {
        hud.paused=true; hud.menu=Menu::None; endNaming(); cancelEdits(); capture(false);
      }
      if(event.type==SDL_EVENT_MOUSE_MOTION && hud.menuOpen()) {
        hud.pointer=pointerPixels(event.motion.x,event.motion.y);
        if(hud.menu==Menu::Inventory) hud.inventoryHover=Ui::inventoryItemAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
        else if(hud.menu==Menu::Crafting) {
          int recipe=Ui::recipeAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
          if(recipe>=0) hud.recipeSelected=recipe;
        }
      }
      if(event.type==SDL_EVENT_MOUSE_MOTION && !hud.paused && !hud.menuOpen() && sleepRemaining<=0 && interactive) {
        player.look(event.motion.xrel,event.motion.yrel); adventure.looked(world,event.motion.xrel,event.motion.yrel);
      }
      if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if(hud.menuOpen() && event.button.button==SDL_BUTTON_LEFT) {
          hud.pointer=pointerPixels(event.button.x,event.button.y);
          auto tab=Ui::menuTabAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
          if(tab!=Menu::None) showMenu(tab);
          else if(hud.menu==Menu::Crafting) {
            int recipe=Ui::recipeAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
            if(recipe>=0) makeRecipe(recipe);
          } else if(hud.menu==Menu::Farm) {
            int animal=hud.farmGarden || hud.farmShop ? -1 : Ui::farmAnimalAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y,hud.farmPage,int(world.farm.chickens.size()));
            if(animal>=0) { endNaming(); hud.animalSelected=animal; }
            else farmAction(Ui::farmActionAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y,hud.farmGarden,hud.farmShop,hud.farmRanch));
          } else {
            int itemIndex=Ui::inventoryItemAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
            int slot=Ui::inventorySlotAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
            if(itemIndex>=0) {
              auto item=itemCatalog[itemIndex];
              if(itemAvailable(item,world.crafting)) hud.carried=item;
              else {
                showMenu(Menu::Crafting); hud.recipeSelected=item==Item::Workbench ? 1 : item==Item::Axe ? 2 : 3;
                notice(craftProblem(world,player,Recipe(hud.recipeSelected)));
              }
            } else if(slot>=0) {
              if(hud.carried) putInSlot(slot,*hud.carried);
              else world.inventory.selected=slot;
            }
          }
        }
        else if(hud.menuOpen()) {}
        else if(hud.paused) { hud.paused=false; cancelEdits(); capture(true); }
        else if(sleepRemaining>0 || !interactive) {}
        else if(event.button.button==SDL_BUTTON_LEFT) pressRemove(mouseEditSource);
        else if(event.button.button==SDL_BUTTON_RIGHT) pressPlace(mouseEditSource);
        else if(event.button.button==SDL_BUTTON_MIDDLE) {
          if(auto hit=world.raycast(player.eye(),player.direction())) {
            auto item=itemFromBlock(world.get(hit->block));
            if(item!=Item::Empty && equipItem(world.inventory,world.crafting,item)) cancelEdits();
          }
        }
      }
      if(event.type==SDL_EVENT_MOUSE_BUTTON_UP) {
        if(event.button.button==SDL_BUTTON_LEFT) {
          releaseRemove(mouseEditSource);
          if(hud.menu==Menu::Inventory && hud.carried) {
            hud.pointer=pointerPixels(event.button.x,event.button.y);
            int slot=Ui::inventorySlotAt(hud.width,hud.height,hud.pointer.x,hud.pointer.y);
            if(slot>=0) putInSlot(slot,*hud.carried);
          }
        }
        if(event.button.button==SDL_BUTTON_RIGHT) releasePlace(mouseEditSource);
      }
      if(event.type==SDL_EVENT_KEY_UP) {
        if(event.key.scancode==SDL_SCANCODE_X) releaseRemove(keyEditSource);
        if(event.key.scancode==SDL_SCANCODE_V) releasePlace(keyEditSource);
      }
      if(event.type==SDL_EVENT_MOUSE_WHEEL && !hud.paused && !hud.menuOpen()) {
        float y=event.wheel.y*(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED ? -1.f : 1.f);
        if(y!=0) { world.inventory.selected=(world.inventory.selected+(y>0 ? hotbarSize-1 : 1))%hotbarSize; cancelEdits(); }
      }
      if(event.type==SDL_EVENT_TEXT_INPUT && hud.naming) appendName(event.text.text);
      if(event.type==SDL_EVENT_KEY_DOWN && hud.naming) {
        auto key=event.key.scancode;
        if(key==SDL_SCANCODE_Q && (event.key.mod&SDL_KMOD_GUI)) running=false;
        else if(key==SDL_SCANCODE_ESCAPE) endNaming();
        else if(key==SDL_SCANCODE_RETURN || key==SDL_SCANCODE_KP_ENTER) saveName();
        else if(key==SDL_SCANCODE_A && (event.key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))) hud.nameSelectedAll=true;
        else if(key==SDL_SCANCODE_BACKSPACE) {
          if(hud.nameSelectedAll) hud.nameDraft.clear();
          else if(!hud.nameDraft.empty()) hud.nameDraft.pop_back();
          hud.nameSelectedAll=false;
        } else if(key==SDL_SCANCODE_V && (event.key.mod&(SDL_KMOD_GUI|SDL_KMOD_CTRL))) {
          if(char* clipboard=SDL_GetClipboardText()) { appendName(clipboard); SDL_free(clipboard); }
        }
        continue; // Typing E, P, M, numbers, etc. must never activate game shortcuts.
      }
      if(event.type==SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        auto key=event.key.scancode;
        if(key==SDL_SCANCODE_Q && (event.key.mod&SDL_KMOD_GUI)) running=false;
        if(key==SDL_SCANCODE_ESCAPE) {
          if(hud.menuOpen()) showMenu(Menu::None);
          else { hud.paused=!hud.paused; cancelEdits(); capture(!hud.paused); }
        }
        if(key==SDL_SCANCODE_F11) SDL_SetWindowFullscreen(window.get(),!(SDL_GetWindowFlags(window.get())&SDL_WINDOW_FULLSCREEN));
        if(key==SDL_SCANCODE_F5) save();
        if(key==SDL_SCANCODE_M) { hud.muted=!hud.muted; notice(hud.muted ? "SOUND OFF / M TO UNMUTE" : "SOUND ON"); }
        if(key==SDL_SCANCODE_E && sleepRemaining<=0) showMenu(hud.menuOpen() ? Menu::None : Menu::Inventory);
        if(key==SDL_SCANCODE_P && sleepRemaining<=0) {
          if(hud.menu!=Menu::Farm) {
            hud.farmGarden=true; hud.farmShop=false;
            if(auto chicken=targetChicken(world,player); !hud.menuOpen() && chicken) { hud.animalSelected=int(*chicken); hud.farmGarden=false; }
          }
          showMenu(hud.menu==Menu::Farm ? Menu::None : Menu::Farm);
        }
        if(hud.menu==Menu::Farm) {
          if(!hud.farmGarden && !hud.farmShop && (key==SDL_SCANCODE_UP || key==SDL_SCANCODE_DOWN) && !world.farm.chickens.empty()) {
            hud.animalSelected=std::clamp(hud.animalSelected+(key==SDL_SCANCODE_UP ? -1 : 1),0,int(world.farm.chickens.size())-1);
            hud.farmPage=hud.animalSelected/6;
          }
          if(!hud.farmGarden && !hud.farmShop && key==SDL_SCANCODE_RETURN) beginNaming();
        } else if(hud.menu==Menu::Crafting) {
          if(key>=SDL_SCANCODE_1 && key<=SDL_SCANCODE_4) makeRecipe(int(key-SDL_SCANCODE_1));
          if(key==SDL_SCANCODE_UP) hud.recipeSelected=(hud.recipeSelected+3)%4;
          if(key==SDL_SCANCODE_DOWN) hud.recipeSelected=(hud.recipeSelected+1)%4;
          if(key==SDL_SCANCODE_RETURN) makeRecipe(hud.recipeSelected);
        } else if(hud.menu==Menu::Inventory) {
          if(key>=SDL_SCANCODE_1 && key<=SDL_SCANCODE_9) {
            int slot=int(key-SDL_SCANCODE_1);
            if(hud.carried) putInSlot(slot,*hud.carried);
            else if(hud.inventoryHover>=0) putInSlot(slot,itemCatalog[hud.inventoryHover]);
            else world.inventory.selected=slot;
          }
          if(key==SDL_SCANCODE_BACKSPACE || key==SDL_SCANCODE_DELETE) putInSlot(world.inventory.selected,Item::Empty);
        }
        if(!hud.paused && !hud.menuOpen() && sleepRemaining<=0) {
          if(key==SDL_SCANCODE_X && interactive) pressRemove(keyEditSource);
          if(key==SDL_SCANCODE_V && interactive) pressPlace(keyEditSource);
          if(key>=SDL_SCANCODE_1 && key<=SDL_SCANCODE_9) { world.inventory.selected=int(key-SDL_SCANCODE_1); cancelEdits(); }
          if(key==SDL_SCANCODE_H) hud.help=!hud.help;
          if(key==SDL_SCANCODE_TAB && (!ride.active || leaveRide(world,player,ride))) {
            player.toggleFlying(); jump=false; cancelEdits();
            notice(player.pose.flying ? "FLIGHT ON / SPACE UP / SHIFT DOWN" : "FLIGHT OFF / BACK TO WALKING");
          }
          if(key==SDL_SCANCODE_SPACE && !ride.active) {
            double pressedAt=double(SDL_GetTicks())*.001;
            if(lastJumpPress>=0 && pressedAt-lastJumpPress<=.3) {
              player.toggleFlying(); jump=false; lastJumpPress=-1;
              notice(player.pose.flying ? "FLIGHT ON / SPACE UP / SHIFT DOWN" : "FLIGHT OFF");
            } else { jump=true; lastJumpPress=pressedAt; }
          }
          if(key==SDL_SCANCODE_R && world.terrain.adventure()) {
            goHome();
          }
          if(key==SDL_SCANCODE_G && !hud.farming && world.terrain.adventure() && adventure.stage(world)<4) notice("FOLLOW THE STEP ON THE LEFT FIRST");
        }
      }
    }
    if(!running) break;
    if(options.smoke) {
      if(frame==24) { player.pose.flying=true; player.velocity={}; player.pose.pitch=-1.3f; if(edit(false)) ++smokeEdits; }
      if(frame==30) { if(edit(true)) ++smokeEdits; }
      if(frame==36) { player.pose.flying=false; player.pose.pitch=-.23f; world.guideFlags|=Looked|Walked|Broke|Placed; }
      if(frame==60) SDL_SetWindowSize(window.get(),1120,720);
      if(frame==90) SDL_SetWindowSize(window.get(),1280,800);
      if(frame==95) world.clock.phase=.745; // Sunset lighting.
      if(frame==110) {
        if(world.terrain.adventure()) { demoCabin(world); cabinView(player); }
        else { player.pose.flying=true; player.pose.position.y+=7; player.pose.pitch=-.35f; }
      }
      if(world.terrain.adventure() && (frame==115 || frame==120 || frame==125))
        if(useDoor({10,25,-2})) ++smokeDoorActions;
      if(frame==128) {
        world.clock.phase=.90;
        if(world.terrain.adventure()) { bedView(player); world.guideFlags|=VisitedCave; requestSleep(cabinBed.cell); }
      }
      if(frame==170 && world.terrain.adventure()) cabinView(player);
    }
    center=chunkAt(int(std::floor(player.pose.position.x)),int(std::floor(player.pose.position.z)));
    world.ensure(center,1); // Collision never depends on an unfinished background job.
    worker.collect(world,center);
    if(!previousCenter || *previousCenter!=center) {
      world.evict(center,viewRadius+2); worker.request(center,world); previousCenter=center;
    }
    if(hud.paused || hud.menuOpen() || sleepRemaining>0) cancelEdits();
    if(!hud.paused && !hud.menuOpen()) debris.tick(dt);
    if(!hud.paused && !hud.menuOpen() && sleepRemaining>0) {
      sleepRemaining=std::max(0.f,sleepRemaining-(options.smoke ? .1f : dt));
      if(!sleepApplied && sleepRemaining<=1.8f && sleepingBed) {
        auto beforeClock=world.clock;
        auto result=sleepInBed(world,player,*sleepingBed);
        if(result==SleepResult::Ready) {
          double elapsed=(double(world.clock.day)-double(beforeClock.day)+world.clock.phase-beforeClock.phase)*WorldClock::daySeconds;
          growFarm(world,float(elapsed));
          sleepApplied=true; audio.play(Sound::Wake); notice("GOOD MORNING / A NEW DAY IN THE MEADOW");
          if(options.smoke) ++smokeSleeps;
          if(options.save) save();
        } else { sleepRemaining=0; notice(sleepMessage(result)); }
      }
    } else if(!hud.paused && !hud.menuOpen()) {
      world.clock.advance(options.smoke ? 1./60. : dt);
      const bool* keys=SDL_GetKeyboardState(nullptr);
      Movement movement;
      if(interactive) {
        movement.forward=float(keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])-float(keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]);
        movement.right=float(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])-float(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
        movement.vertical=float(keys[SDL_SCANCODE_SPACE])-float(keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]);
        movement.sprint=keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
        movement.sneak=keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        movement.jump=jump;
      }
      auto before=player.pose.position;
      bool wasGrounded=player.grounded;
      float fallSpeed=player.velocity.y;
      if(!ride.active) player.tick(world,movement,options.smoke ? 1.f/60.f : dt);
      else movement.jump=keys[SDL_SCANCODE_SPACE];
      tickRanch(world,player,ride,movement,dt);
      if(player.grounded && !player.pose.flying) {
        float travelled=glm::length(glm::vec2(player.pose.position.x-before.x,player.pose.position.z-before.z));
        stride+=travelled;
        bool landed=!wasGrounded && fallSpeed<-2.f;
        if(stride>=1.75f || landed) {
          Cell floor{int(std::floor(player.pose.position.x)),int(std::floor(player.pose.position.y-.05f)),int(std::floor(player.pose.position.z))};
          audio.play(Sound::Step,world.get(floor),landed ? 1.15f : 1.f,leftFoot ? -.13f : .13f);
          leftFoot=!leftFoot; stride=0;
        }
        if(travelled<.001f) stride=0;
      } else stride=0;
      adventure.moved(world,before,player.pose.position);
      tickFarm(world,player,dt,isWheat(itemBlock(world.inventory.held())));
      rainTimer-=dt;
      if(world.farm.garden.raining() && rainTimer<=0) {
        auto eye=player.eye();
        if(world.cropShelter({int(std::floor(eye.x)),int(std::floor(eye.y)),int(std::floor(eye.z))})==0) debris.rain(eye);
        rainTimer=.1f;
      }
      cluckTimer-=dt;
      if(cluckTimer<=0 && !world.farm.chickens.empty()) {
        const auto& chicken=world.farm.chickens[cluckBird++%world.farm.chickens.size()];
        float distance=glm::length(chicken.position-player.eye());
        if(!chicken.sleeping && distance<12) soundAt(Sound::Cluck,Block::Wood,{int(std::floor(chicken.position.x)),int(std::floor(chicken.position.y)),int(std::floor(chicken.position.z))});
        cluckTimer=4.5f;
      }
      bool removing=interactive && removeHeld!=0;
      if(removeInput.tick(dt,removing && !ride.active)) { edit(false); toolSwingRemaining=toolSwingSeconds; }
      if(buildInput.tick(dt,interactive && !removing)) { use(repeatedUse); repeatedUse=true; }
      if(keys[SDL_SCANCODE_G] && !ride.active && guideCooldown==0.f && interactive && !removing && !hud.menuOpen() && sleepRemaining<=0) {
        auto next=adventure.nextPiece(world,player);
        if(hud.farming && !hud.farmGarden) {
          if(buildPenNext(world,player)) audio.play(Sound::Place,Block::Wood);
        } else if(!hud.farming && adventure.buildNext(world,player) && next) {
          equipItem(world.inventory,world.crafting,itemFromBlock(next->block));
          soundAt(Sound::Place,next->block,next->cell);
        }
        guideCooldown=.16f;
      }
      adventure.update(world,player);
      int stage=adventure.stage(world);
      if(stage!=previousStage) {
        if(stage==5) notice("YOUR FIRST HOME IS READY");
        if(stage==6) notice("YOU DISCOVERED THE LANTERN CAVE / YOUR BED AWAITS");
        if(stage==7) notice("YOUR FIRST NIGHT / A HOME TO CALL YOUR OWN");
        previousStage=stage;
      }
      saveTime+=dt;
      if(saveTime>30.f) save();
    }
    if(world.farm.chickens.size()>knownFlockSize) {
      auto added=world.farm.chickens.size()-knownFlockSize;
      notice(added==1 ? animalName(world.farm.chickens.back(),world.farm.chickens.size()-1)+" HATCHED / P OPENS YOUR FARM" : std::to_string(added)+" BABY CHICKS HATCHED / P OPENS YOUR FARM");
      audio.play(Sound::Cluck); knownFlockSize=world.farm.chickens.size();
    }
    hud.farm=farmView(world);
    hud.flying=player.pose.flying;
    if(hud.menuOpen()) hud.craft=craftView(world,player);
    else hud.craft.bag=world.crafting;
    hud.inventory=world.inventory;
    hud.breaking.reset(); hud.breakProgress=0; hud.riding=ride.active; hud.driving=ride.active && ride.car;
    hud.toolSwing=toolSwingRemaining>0 ? 1.f-toolSwingRemaining/toolSwingSeconds : 0.f;
    hud.clock=world.clock; hud.sleeping=sleepRemaining>0; hud.waking=sleepApplied;
    hud.sleepFade=sleepRemaining>2.f ? (3.2f-sleepRemaining)/1.2f : std::min(1.f,sleepRemaining/1.2f);
    hud.audioAvailable=audio.available();
    float exposure=1.f;
    auto eye=player.eye();
    for(int y=int(std::floor(eye.y))+1;y<worldHeight;++y) {
      auto above=world.get({int(std::floor(eye.x)),y,int(std::floor(eye.z))});
      if(opaque(above)) { exposure=above==Block::Leaves ? .65f : .18f; break; }
    }
    audio.environment(world.clock.sky().daylight,exposure,!hud.paused && !hud.menuOpen(),hud.muted);
    hud.guide=hud.farming ? (hud.farmGarden ? gardenGuide(world,player) : farmGuide(world,player)) : adventure.view(world,player);
    if(trackedAnimal && *trackedAnimal<world.farm.chickens.size()) {
      const auto& c=world.farm.chickens[*trackedAnimal];
      auto name=animalName(c,*trackedAnimal);
      hud.guide.enabled=true; hud.guide.destination=c.position+glm::vec3(0,chickenScale(c),0);
      hud.guide.destinationName=name; hud.guide.preview.reset();
      hud.guide.title="FINDING "+name;
      hud.guide.lines={"Follow the marker to your animal.","Names appear above nearby animals.","Press P for your garden and flock."};
      if(glm::length(c.position-player.pose.position)<3) trackedAnimal.reset();
    }
    hud.wheat=world.farm.wheat; hud.eggs=world.farm.eggs;
    auto hit=world.raycast(player.eye(),player.direction());
    const bool* keys=SDL_GetKeyboardState(nullptr);
    auto target=useTarget(world,player,world.inventory.held(),keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]);
    hud.placement=target.kind==UseKind::Place ? placementPreview(world,player,hit,itemBlock(world.inventory.held())) : std::nullopt;
    if(target.kind==UseKind::Greenhouse) {
      auto c=target.cell;
      hud.placement=PlacementPreview{c,Block::Glass,greenhouseProblem(world,player,c).empty() ? PlacementStatus::Ready : PlacementStatus::Occupied,
        Box{glm::vec3(c.x-2,c.y,c.z-4),glm::vec3(c.x+3,c.y+4,c.z+1)}};
    }
    hud.interaction.clear();
    switch(target.kind) {
      case UseKind::Ranch: if(auto animal=targetRanch(world,player)) hud.interaction=ranchPrompt(world,*animal); break;
      case UseKind::Till: hud.interaction=target.distance>3.5f ? "STEP CLOSER / V TO PREPARE SOIL" : "HOE / RIGHT CLICK OR V ON GRASS / THEN PLANT SEEDS"; break;
      case UseKind::Compost:
        hud.interaction=world.farm.garden.compost>0 ? "COMPOST / V ONCE / TWO EXTRA AT HARVEST" : "COMPOST EMPTY / P / SHOP FOR MORE"; break;
      case UseKind::Greenhouse: {
        auto problem=greenhouseProblem(world,player,target.cell);
        hud.interaction=problem.empty() ? "V PLACE GREENHOUSE / DOOR HERE / EXTENDS NORTH" : problem; break;
      }
      case UseKind::Door: hud.interaction=doorOpen(world.get(target.cell)) ? "RIGHT CLICK / V / CLOSE DOOR" : "RIGHT CLICK / V / OPEN DOOR"; break;
      case UseKind::Gate: hud.interaction=gateOpen(world.get(target.cell)) ? "RIGHT CLICK / V / CLOSE GATE" : "RIGHT CLICK / V / OPEN GATE"; break;
      case UseKind::Workbench: hud.interaction="RIGHT CLICK / V / CRAFT"; break;
      case UseKind::Bed: {
        auto status=bedSleepStatus(world,player,target.cell);
        hud.interaction=status==SleepResult::Ready ? "RIGHT CLICK / V / SLEEP" : status==SleepResult::Daytime ? "BED / SLEEP AT DUSK" : "RIGHT CLICK / V / USE BED";
        break;
      }
      case UseKind::Crop: case UseKind::Water:
        hud.interaction=target.distance>3.5f ? "STEP CLOSER TO YOUR CROP" : cropPrompt(world,target.cell,world.inventory.held()==Item::WateringCan); break;
      case UseKind::Chicken:
        hud.interaction=chickenPrompt(world,target.chicken);
        if(!isChick(world.farm.chickens[target.chicken]) && world.farm.chickens[target.chicken].hatchTimer<0
           && world.farm.chickens[target.chicken].eggTimer<0 && !world.farm.chickens[target.chicken].eggReady
           && world.farm.wheat>0 && world.clock.sky().daylight>=.12f && world.inventory.held()!=Item::Wheat)
          hud.interaction="HOLD WHEAT TO FEED / E INVENTORY";
        break;
      default: break;
    }
    if(hud.interaction.empty() && hud.placement) hud.interaction=placementMessage(hud.placement->status);
    if(hud.interaction.empty()) hud.interaction=world.inventory.held()==Item::WateringCan ? "AIM AT A PLANT / RIGHT CLICK OR V TO WATER"
      : world.inventory.held()==Item::Hoe ? "AIM AT GRASS OR DIRT / V TO PREPARE SOIL"
      : world.inventory.held()==Item::Compost ? "AIM AT A GROWING PLANT / V TO ADD COMPOST"
      : isCrop(itemBlock(world.inventory.held())) ? "AIM AT PREPARED SOIL / RIGHT CLICK OR V TO PLANT"
      : itemBlock(world.inventory.held())==Block::Air ? "LEFT CLICK SWING / E INVENTORY" : "AIM AT THE GROUND OR A BLOCK TO BUILD";
    if(ride.active) { hud.placement.reset(); hud.interaction="W / UP FORWARD   S / DOWN REVERSE   A-D STEER   SPACE BRAKE   V EXIT"; }
    renderer.sync(world,center,4);
    bool last=options.frames>0 && frame+1>=options.frames;
    renderer.draw(player,hit,hud,totalTime,debris,world,last ? options.screenshot : std::filesystem::path{});
    ++frame;
    if(last) running=false;
    if(hud.paused || hud.menuOpen()) SDL_Delay(12);
  }
  endNaming(); capture(false);
  // Exit saves propagate failure so the caller never sees a successful save that did not happen.
  if(options.save) world.save(savePath,player.pose);
  double elapsed=double(SDL_GetPerformanceCounter()-start)/frequency;
  std::cout<<"Frames: "<<frame<<"; elapsed: "<<elapsed<<"s; average: "<<double(frame)/elapsed
           <<" fps; chunks: "<<renderer.meshCount()<<"; triangles: "<<renderer.triangleCount()<<'\n';
  if(options.smoke) {
    if(smokeEdits!=2) throw std::runtime_error("Smoke test: block break/place failed");
    if(renderer.meshCount()<49 || renderer.triangleCount()<1000) throw std::runtime_error("Smoke test: terrain did not render");
    if(world.terrain.adventure() && (smokeDoorActions!=3 || !inspectCabin(world).complete() || smokeSleeps!=1
       || !(world.guideFlags&Slept) || world.clock.day!=2 || world.clock.phase<.30 || world.clock.phase>.31))
      throw std::runtime_error("Smoke test: cabin, doors, or sleep failed");
    if(audio.available() && audio.framesRendered()==0) throw std::runtime_error("Smoke test: audio callback did not run");
    std::cout<<"SMOKE PASS: Metal rendering, terrain streaming, break/place, window resize, clean shutdown\n";
    if(world.terrain.adventure()) std::cout<<"ADVENTURE PASS: furnished cabin, three door interactions, night-to-morning sleep\n";
    std::cout<<"Audio frames rendered: "<<audio.framesRendered()<<'\n';
  }
  return 0;
}
}
} // namespace bw

int main(int argc,char** argv) {
  try { return bw::run(bw::parse(argc,argv)); }
  catch(const std::exception& error) { std::cerr<<"Blockworld: "<<error.what()<<'\n'; return 1; }
}

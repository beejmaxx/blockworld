#pragma once
#include "building.hpp"
#include "farm.hpp"
#include "inventory.hpp"
#include <span>
#include <string>

namespace bw {
struct UiVertex { glm::vec2 position; glm::vec4 color; glm::vec2 uv{-1,-1}; };
enum class Menu { None,Inventory,Crafting,Farm };
enum class FarmAction { None,Seeds,Carrots,Strawberries,Pumpkins,WateringCan,Fences,Gates,Name,SaveName,Hatch,Find,Previous,Next,Cabin,Garden,Animals,Shop,Hoe,Compost,Sprinkler,Greenhouse,Visit,Sell,BuyCompost,BuySprinkler,BuyGreenhouse,RanchShop,GardenShop,BuyCow,BuyHorse,Car,BuySheep,BuyFox };
struct AnimalLabel { glm::vec2 position; std::string name; bool baby=false; };
struct HudState {
  int width{},height{};
  ToolSelection tools;
  bool farming=false;
  int wheat=0,eggs=0;
  Item selectedItem() const { return tools.held(); }
  Block selectedBlock() const { return itemBlock(selectedItem()); }
  Tool tool() const { return itemTool(selectedItem()); }
  float fps{};
  bool paused{},flying{},help=true,hidden=false;
  bool muted=false,audioAvailable=true,sleeping=false,waking=false;
  float sleepFade=0;
  Menu menu=Menu::None;
  bool menuOpen() const { return menu!=Menu::None; }
  int inventoryHover=-1,toolPage=0;
  glm::vec2 pointer{};
  int recipeSelected=0;
  FarmView farm;
  int animalSelected=0,farmPage=0;
  bool farmGarden=true;
  bool farmShop=false,farmRanch=false;
  bool riding=false,driving=false;
  bool naming=false,nameSelectedAll=false;
  std::string nameDraft;
  std::vector<AnimalLabel> animalLabels;
  CraftView craft;
  std::optional<PlacementPreview> placement;
  std::optional<Cell> breaking;
  float breakProgress=0;
  float toolSwing=0;
  WorldClock clock;
  std::string notice;
  GuideView guide;
  std::optional<glm::vec2> waypoint;
  float waypointDistance=0;
  std::string interaction;
};
class Ui {
public:
  std::vector<UiVertex> vertices;
  void build(const HudState& hud);
  static int recipeAt(int width,int height,float x,float y);
  static int toolAt(int width,int height,float x,float y,PlayMode mode,int page=0);
  static int toolPageAt(int width,int height,float x,float y);
  static std::optional<PlayMode> modeAt(int width,int height,float x,float y);
  static Menu menuTabAt(int width,int height,float x,float y);
  static int farmAnimalAt(int width,int height,float x,float y,int page,int count);
  static FarmAction farmActionAt(int width,int height,float x,float y,bool garden=false,bool shop=false,bool ranch=false);
private:
  void rectangle(float x,float y,float w,float h,glm::vec4 color);
  void quad(glm::vec2 a,glm::vec2 b,glm::vec2 c,glm::vec2 d,glm::vec4 color);
  void text(std::string_view value,float x,float y,float scale,glm::vec4 color);
  void centered(std::string_view value,float x,float y,float scale,glm::vec4 color);
  void label(std::string_view value,float x,float y,float size,glm::vec4 color);
  void labelCentered(std::string_view value,float x,float y,float size,glm::vec4 color);
  void cube(Block block,float x,float y,float size);
  void itemIcon(Item item,float x,float y,float size);
  void inventory(const HudState& hud);
  void menuTabs(const HudState& hud);
  void modeButtons(const HudState& hud,float x,float y,float width,float height);
  void hammerIcon(float x,float y,float size);
  void workshop(const HudState& hud);
  void farmPage(const HudState& hud);
  void heldTool(const HudState& hud);
};
} // namespace bw

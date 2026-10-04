#include "ui.hpp"
#include "garden.hpp"
#include "ui_font.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void bounds(const Ui& ui,int w,int h) {
  check(!ui.vertices.empty() && ui.vertices.size()%6==0,"UI consists of complete quads");
  for(const auto& vertex : ui.vertices) {
    auto p=vertex.position;
    if(p.x<0 || p.x>w || p.y<0 || p.y>h) std::cerr<<"UI point "<<p.x<<','<<p.y<<" outside "<<w<<'x'<<h<<'\n';
    check(std::isfinite(p.x) && std::isfinite(p.y) && p.x>=0 && p.x<=w && p.y>=0 && p.y<=h,"UI fits inside the viewport");
    check(vertex.color.a>=0 && vertex.color.a<=1,"UI alpha is valid");
    if(vertex.uv.x>=0) check(vertex.uv.x<=1 && vertex.uv.y>=0 && vertex.uv.y<=1,"readable font texture coordinates stay inside the atlas");
  }
}
// Software rasterization of the actual UI vertices allows layout inspection without a native window.
float edge(glm::vec2 a,glm::vec2 b,glm::vec2 p) { return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x); }
bool triangle(glm::vec2 a,glm::vec2 b,glm::vec2 c,glm::vec2 p) {
  auto x=edge(a,b,p),y=edge(b,c,p),z=edge(c,a,p);
  return (x>=0 && y>=0 && z>=0) || (x<=0 && y<=0 && z<=0);
}
void preview(const Ui& ui,int width,int height,const std::filesystem::path& path) {
  if(path.empty()) return;
  std::vector<glm::vec3> pixels(std::size_t(width*height),{.28f,.37f,.32f});
  for(std::size_t i=0;i<ui.vertices.size();i+=6) {
    auto a=ui.vertices[i].position,b=ui.vertices[i+1].position,c=ui.vertices[i+2].position,d=ui.vertices[i+5].position;
    auto lo=glm::min(glm::min(a,b),glm::min(c,d)),hi=glm::max(glm::max(a,b),glm::max(c,d));
    auto ink=ui.vertices[i].color;
    for(int y=std::max(0,int(std::floor(lo.y)));y<std::min(height,int(std::ceil(hi.y)));++y)
      for(int x=std::max(0,int(std::floor(lo.x)));x<std::min(width,int(std::ceil(hi.x)));++x) {
        glm::vec2 p{x+.5f,y+.5f};
        if(triangle(a,b,c,p) || triangle(a,c,d,p)) {
          float alpha=ink.a;
          if(ui.vertices[i].uv.x>=0) {
            const auto& atlas=readableFont();
            auto uv=glm::mix(ui.vertices[i].uv,ui.vertices[i+2].uv,(p-a)/(c-a));
            auto location=uv*glm::vec2(FontAtlas::width,FontAtlas::height)-.5f;
            int sx=int(std::floor(location.x)),sy=int(std::floor(location.y));
            auto sample=[&](int dx,int dy) { return float(atlas.pixels[std::clamp(sy+dy,0,FontAtlas::height-1)*FontAtlas::width+std::clamp(sx+dx,0,FontAtlas::width-1)])/255.f; };
            alpha*=glm::mix(glm::mix(sample(0,0),sample(1,0),location.x-sx),glm::mix(sample(0,1),sample(1,1),location.x-sx),location.y-sy);
          }
          auto& pixel=pixels[std::size_t(y*width+x)]; pixel=glm::mix(pixel,glm::vec3(ink),alpha);
        }
      }
  }
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path,std::ios::binary); out<<"P6\n"<<width<<' '<<height<<"\n255\n";
  for(auto pixel : pixels) for(int c=0;c<3;++c) out.put(char(std::clamp(int(pixel[c]*255+.5f),0,255)));
  check(bool(out),"UI preview written");
}
}
int main(int argc,char** argv) {
  try {
    std::filesystem::path output=argc>1 ? argv[1] : "";
    auto snapshot=[&](const Ui& ui,const HudState& h,const std::string& name) {
      if(!output.empty()) preview(ui,h.width,h.height,output/name);
    };
    World world; Player player; Ui ui; HudState hud;
    const auto& font=readableFont();
    check(std::ranges::any_of(font.pixels,[](auto p){return p>0 && p<255;}) && readableWidth("Carrots",18)>0,"system font has antialiased glyphs and measurable text");
    hud.menu=Menu::Crafting;
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      float x=float(size.x)/2,y=float(size.y)/2-276;
      for(int i=0;i<4;++i) {
        check(Ui::recipeAt(size.x,size.y,x,y+238+i*60+27)==i,"recipe centers are clickable at both window sizes");
        check(Ui::recipeAt(size.x,size.y,x,y+238+i*60+55)==-1,"gaps do not activate recipes");
      }
      check(Ui::recipeAt(size.x,size.y,0,y+250)==-1 && Ui::recipeAt(size.x,size.y,x,y+50)==-1,"menu background and heading cannot craft");
      for(std::uint32_t flags : {0u,1u,3u,7u,15u,31u,63u,127u}) {
        world.crafting={9999,9999,9999,flags};
        hud.craft=craftView(world,player); hud.notice="WORKBENCH READY"; ui.build(hud); bounds(ui,size.x,size.y);
      }
    }
    hud.width=1280; hud.height=800; world.crafting={}; hud.notice.clear();
    hud.craft=craftView(world,player); ui.build(hud); snapshot(ui,hud,"crafting-start.ppm");
    hud.width=800; hud.height=600; world.crafting={1,8,3,15};
    world.insert(Chunk{{0,0}}); world.set({0,2,1},Block::Workbench); player.pose.position={.5f,2,3.5f};
    hud.craft=craftView(world,player); hud.recipeSelected=2; ui.build(hud); snapshot(ui,hud,"crafting-small.ppm");
    hud.menu=Menu::None; hud.paused=true; hud.guide.enabled=true;
    ui.build(hud); bounds(ui,hud.width,hud.height); snapshot(ui,hud,"controls-small.ppm");
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y; hud.paused=false; hud.riding=true; hud.driving=true; hud.guide.enabled=false;
      hud.map=buildMiniMap(world,{0,0},false,true);hud.map.markers.push_back({{1000,200},"City",{.4f,.8f,1}});
      hud.boosting=true;hud.carSpeed=carBoostSpeed;
      ui.build(hud); bounds(ui,size.x,size.y); snapshot(ui,hud,"driving-"+std::to_string(size.x)+".ppm");
    }
    hud.map={};hud.boosting=false;
    hud.width=800; hud.height=600; hud.riding=false; hud.driving=false;
    hud.paused=false; hud.fps=60; hud.guide.enabled=false; hud.breaking=Cell{0,2,1}; hud.breakProgress=.6f;
    hud.interaction="Build / Left-click places / Right-click removes"; hud.tools.choose(Item::Workbench,CraftState{0,0,0,127});
    ui.build(hud); bounds(ui,hud.width,hud.height); snapshot(ui,hud,"building-small.ppm");
    hud.farming=true; hud.tools.choose(Item::Wheat,CraftState{0,0,0,127}); hud.breaking.reset(); hud.interaction="CHICKEN / RIGHT CLICK / V FEED WHEAT";
    hud.wheat=9999; hud.eggs=9999;
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      for(std::uint32_t flags : {0u,1u,3u,7u,15u,31u,63u,127u,255u}) {
        world.farm.flags=flags; hud.guide=farmGuide(world,player);
        ui.build(hud); bounds(ui,hud.width,hud.height);
      }
    }
    hud.width=800; hud.height=600; hud.wheat=3; hud.eggs=1; world.farm.flags=7;
    hud.guide=farmGuide(world,player); ui.build(hud); snapshot(ui,hud,"farm-small.ppm");
    hud.farming=false; hud.guide.enabled=false; hud.interaction="E to choose mode and tools";
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      for(auto item : itemCatalog) {
        hud.tools.choose(item,CraftState{0,0,0,127});
        for(int frame=0;frame<=20;++frame) {
          hud.toolSwing=float(frame)/20; ui.build(hud); bounds(ui,size.x,size.y);
        }
      }
    }
    hud.width=800; hud.height=600; hud.tools.choose(Item::Axe,CraftState{0,0,0,127}); hud.toolSwing=0;
    ui.build(hud); snapshot(ui,hud,"axe-in-hand.ppm");
    hud.toolSwing=.5f; ui.build(hud); snapshot(ui,hud,"axe-swing.ppm");
    hud.tools.choose(Item::Pickaxe,CraftState{0,0,0,127}); hud.toolSwing=0; ui.build(hud); snapshot(ui,hud,"pickaxe-in-hand.ppm");
    hud.tools.choose(Item::Planks,CraftState{0,0,0,127}); ui.build(hud); snapshot(ui,hud,"planks-in-hand.ppm");
    hud.tools.choose(Item::Wheat,CraftState{0,0,0,127}); ui.build(hud); snapshot(ui,hud,"wheat-in-hand.ppm");
    hud.cityAvailable=true;hud.cityLife.bank=bankLimit;hud.farm.garden.coins=coinLimit;
    hud.rentPerDay=2300;hud.serverPerDay=256;
    hud.cityLife.statement={{BankKind::Rent,2300,999},{BankKind::Servers,256,999}};
    for(auto size:{glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x;hud.height=size.y;float x=size.x*.5f-340,y=size.y*.5f-276;hud.menu=Menu::City;
      for(int page=0;page<5;++page) {
        hud.cityPage=CityPage(page);hud.cityMessage.clear();ui.build(hud);bounds(ui,size.x,size.y);
        check(Ui::cityActionAt(hud,x+80+page*128,y+110).action==CityAction::Page,"city tabs have working targets");
        snapshot(ui,hud,"city-"+std::to_string(page)+"-"+std::to_string(size.x)+".ppm");
      }
      hud.cityPage=CityPage::Places;
      for(int i=0;i<6;++i){auto click=Ui::cityActionAt(hud,x+100+(i%2)*328,y+205+(i/2)*110);check(click.action==CityAction::Place && click.index==i,"every destination selects the correct place");}
      hud.cityPage=CityPage::Garage;
      for(int page=0;page<2;++page)for(int i=0;i<10;++i) {
        hud.garagePage=page;auto target=Ui::cityActionAt(hud,x+80+(i%2)*328,y+198+(i/2)*54);
        check(target.action==CityAction::Drive && target.index==page*10+i,"all twenty garage cars are selectable");
      }
      hud.cityPage=CityPage::Residents;hud.nearResident=false;
      check(Ui::cityActionAt(hud,x+300,y+458).action==CityAction::None,"chat requires visiting the resident");
      hud.nearResident=true;check(Ui::cityActionAt(hud,x+300,y+458).action==CityAction::Chat,"nearby resident enables chat");
      hud.cityMessage="Mei: Yes, I'd love to go out with you! Come visit me again.";ui.build(hud);bounds(ui,size.x,size.y);
      auto& household=hud.cityLife.residents[hud.residentSelected];household={};
      check(Ui::cityActionAt(hud,x+400,y+500).action==CityAction::None,"family dialogue requires dating");
      household.dating=true;hud.nearResident=false;
      check(Ui::cityActionAt(hud,x+400,y+500).action==CityAction::None,"family dialogue requires visiting");
      hud.nearResident=true;
      check(Ui::cityActionAt(hud,x+400,y+500).action==CityAction::StartFamily,"family button opens the correct dialogue");
      check(Ui::cityActionAt(hud,x+520,y+458).action==CityAction::SpendNight,"dating enables the adult partner night action");
      hud.cityMessage.clear();ui.build(hud);bounds(ui,size.x,size.y);
      household.pregnancyDue=999;ui.build(hud);bounds(ui,size.x,size.y);
      household.pregnancyDue=0;household.children[0]=998;ui.build(hud);bounds(ui,size.x,size.y);
      check(Ui::cityActionAt(hud,x+520,y+500).action==CityAction::FeedFamily,"parents can feed children from the household page");
      hud.nearResident=false;
      check(Ui::cityActionAt(hud,x+520,y+500).action==CityAction::None,"feeding requires a family visit");
      hud.nearResident=true;
      household={};
      hud.menu=Menu::None;hud.sleeping=true;hud.sleepFade=1;hud.nightPartner="Sofia";
      for(bool morning:{false,true}){hud.waking=morning;ui.build(hud);bounds(ui,size.x,size.y);}
      hud.sleeping=false;hud.nightPartner.clear();hud.menu=Menu::City;
    }
    hud.cityMessage.clear();
    hud.menu=Menu::Inventory; hud.notice.clear();
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      float x=float(size.x)/2-340,y=float(size.y)/2-276;
      for(int m=0;m<3;++m) {
        auto mode=PlayMode(m); hud.tools.mode=mode;
        check(Ui::modeAt(size.x,size.y,x+24+m*640.f/3+50,y+100)==mode,"all three mode buttons work at both sizes");
        check(!Ui::modeAt(size.x,size.y,x+24+m*640.f/3+211,y+100),"gaps between modes cannot switch modes");
        auto choices=modeTools(mode); int columns=mode==PlayMode::Build ? 6 : 3; float stride=640.f/columns;
        for(int i=0;i<int(choices.size());++i) {
          int page=mode==PlayMode::Build ? i/24 : 0,slot=i-page*24;
          float px=x+24+(slot%columns)*stride+stride*.5f,py=y+166+(slot/columns)*76+35;
          check(Ui::toolAt(size.x,size.y,px,py,mode,page)==i,"every labeled tool selects directly without slot assignment");
          check(Ui::toolAt(size.x,size.y,px,py+38,mode,page)==-1,"gaps between tool rows are inert");
        }
        check(Ui::toolAt(size.x,size.y,x+10,y+180,mode)==-1 && Ui::toolAt(size.x,size.y,x+80,y+490,mode)==-1,
          "tool headings and footer cannot select a tool");
        for(auto flags : {0u,127u}) {
          world.crafting.flags=flags; hud.craft=craftView(world,player);
          for(int page=0;page<2;++page) { hud.toolPage=page; ui.build(hud); bounds(ui,size.x,size.y); }
        }
        snapshot(ui,hud,std::string(modeName(mode))+"-tools-"+std::to_string(size.x)+".ppm");
        auto menu=hud.menu; hud.menu=Menu::None; hud.tools.choose(choices[0],world.crafting);
        hud.interaction=mode==PlayMode::Remove ? "Remove mode / Click to remove one block" : mode==PlayMode::Build
          ? "Build / Left-click places / Right-click removes" : "Click to use / E to choose tools";
        ui.build(hud); bounds(ui,size.x,size.y);
        snapshot(ui,hud,std::string(modeName(mode))+"-hud-"+std::to_string(size.x)+".ppm");
        hud.menu=menu;
      }
      check(Ui::menuTabAt(size.x,size.y,float(size.x)/2,y+32)==Menu::Inventory
            && Ui::menuTabAt(size.x,size.y,float(size.x)/2+79,y+32)==Menu::Crafting
            && Ui::menuTabAt(size.x,size.y,float(size.x)/2+174,y+32)==Menu::Farm
            && Ui::menuTabAt(size.x,size.y,float(size.x)/2+268,y+32)==Menu::City,
            "tools, crafting, farm and city menus remain accessible");
    }
    hud.menu=Menu::Farm; hud.farmGarden=false; world.farm.chickens.clear(); world.farm.wheat=12; world.farm.eggs=3;
    for(std::size_t i=0;i<flockLimit;++i) {
      Chicken c; c.name=animalName(c,i); c.position={float(i),2,0};
      if(i>=4) { c.growth=45; c.mother=int(i%4); }
      world.farm.chickens.push_back(c);
    }
    world.farm.chickens[1].hatchTimer=34; hud.farm=farmView(world); hud.notice.clear();
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      float x=float(size.x)/2-340,y=float(size.y)/2-276;
      for(int i=0;i<12;++i) {
        check(Ui::farmAnimalAt(size.x,size.y,x+130,y+220+(i%6)*38,i/6,12)==i,"both pages of animals can be selected");
        check(Ui::farmAnimalAt(size.x,size.y,x+130,y+239+(i%6)*38,i/6,12)==-1,"gaps between animal rows are inert");
        hud.animalSelected=i; hud.farmPage=i/6;
        for(bool naming : {false,true}) {
          hud.naming=naming; hud.nameDraft="Buttercup Peep 123"; hud.nameSelectedAll=true;
          for(int page=0;page<2;++page) { hud.toolPage=page; ui.build(hud); bounds(ui,size.x,size.y); }
        }
      }
      check(Ui::farmAnimalAt(size.x,size.y,x+280,y+220,0,12)==-1
            && Ui::farmAnimalAt(size.x,size.y,x+130,y+220,1,4)==-1,"empty and outside flock rows cannot select an animal");
      check(Ui::farmActionAt(size.x,size.y,x+100,y+114)==FarmAction::Garden
            && Ui::farmActionAt(size.x,size.y,x+466,y+380)==FarmAction::Hatch
            && Ui::farmActionAt(size.x,size.y,x+320,y+220)==FarmAction::Name
            && Ui::farmActionAt(size.x,size.y,x+466,y+430)==FarmAction::Find
            && Ui::farmActionAt(size.x,size.y,x+290,y+380)==FarmAction::None,"farming, hatching, finding, and renaming have separate targets");
      hud.naming=false; hud.animalSelected=1; hud.farmPage=0; ui.build(hud); snapshot(ui,hud,size.x==800 ? "farm-page-small.ppm" : "farm-page.ppm");
      hud.animalSelected=4; hud.naming=true; hud.nameSelectedAll=false; ui.build(hud); snapshot(ui,hud,size.x==800 ? "name-chick-small.ppm" : "name-chick.ppm");
    }
    hud.naming=false; hud.farmGarden=true; hud.farm.harvest={9999,9999,9999,9999};
    hud.farm.plants=256; hud.farm.ripe=112; hud.farm.watered=64;
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      float x=float(size.x)/2-340,y=float(size.y)/2-276;
      constexpr std::array actions{FarmAction::Seeds,FarmAction::Carrots,FarmAction::Strawberries,FarmAction::Pumpkins};
      for(int i=0;i<4;++i) check(Ui::farmActionAt(size.x,size.y,x+24+(i%2)*328+150,y+144+(i/2)*104+45,true)==actions[i],"every garden card equips the intended seeds");
      check(Ui::farmActionAt(size.x,size.y,x+340,y+195,true)==FarmAction::None
         && Ui::farmActionAt(size.x,size.y,x+180,y+243,true)==FarmAction::None,"garden card gaps cannot select a crop");
      check(Ui::farmActionAt(size.x,size.y,x+400,y+390,true)==FarmAction::WateringCan
         && Ui::farmActionAt(size.x,size.y,x+510,y+110,true)==FarmAction::Animals
         && Ui::farmActionAt(size.x,size.y,x+100,y+480,true)==FarmAction::Visit
         && Ui::farmActionAt(size.x,size.y,x+300,y+480,true)==FarmAction::Shop,"garden tools and animal tab are accessible");
      ui.build(hud); bounds(ui,size.x,size.y); snapshot(ui,hud,size.x==800 ? "garden-small.ppm" : "garden.ppm");
      check(ui.vertices.size()<100000,"farm text uses a compact cached font atlas");
      check(Ui::farmActionAt(size.x,size.y,x+550,y+480,true)==FarmAction::Cabin,"Go home and Visit garden have separate destinations");
      hud.farmShop=true; hud.farm.garden.coins=999999; hud.farm.garden.compost=999;
      hud.farm.garden.sprinklers=999; hud.farm.garden.greenhouses=999; hud.farm.basketValue=139986;
      check(Ui::farmActionAt(size.x,size.y,x+500,y+190,true,true)==FarmAction::Sell
        && Ui::farmActionAt(size.x,size.y,x+490,y+280,true,true)==FarmAction::BuyCompost
        && Ui::farmActionAt(size.x,size.y,x+490,y+356,true,true)==FarmAction::BuySprinkler
        && Ui::farmActionAt(size.x,size.y,x+490,y+432,true,true)==FarmAction::BuyGreenhouse
        && Ui::farmActionAt(size.x,size.y,x+590,y+280,true,true)==FarmAction::Compost
        && Ui::farmActionAt(size.x,size.y,x+590,y+356,true,true)==FarmAction::Sprinkler
        && Ui::farmActionAt(size.x,size.y,x+590,y+432,true,true)==FarmAction::Greenhouse,"selling, buying, and taking supplies have distinct targets");
      ui.build(hud); bounds(ui,size.x,size.y); snapshot(ui,hud,size.x==800 ? "shop-small.ppm" : "shop.ppm");
      hud.farmShop=false;
    }
    hud.width=800; hud.height=600; hud.menu=Menu::None; hud.guide.enabled=false;
    hud.tools.choose(Item::WateringCan,CraftState{0,0,0,127}); hud.toolSwing=0; hud.interaction="PUMPKINS / RIGHT CLICK / V WATER";
    ui.build(hud); snapshot(ui,hud,"watering-can.ppm");
    hud.toolSwing=.5f; ui.build(hud); snapshot(ui,hud,"watering-can-pour.ppm");
    for(auto item : {Item::Hoe,Item::Compost,Item::Sprinkler,Item::Greenhouse}) {
      hud.tools.choose(item,CraftState{0,0,0,127}); ui.build(hud); bounds(ui,hud.width,hud.height);
      snapshot(ui,hud,"garden-tool-"+std::to_string(int(item))+".ppm");
    }
    hud.tools.choose(Item::Strawberry,CraftState{0,0,0,127}); hud.toolSwing=0; hud.interaction="AIM AT PREPARED SOIL / RIGHT CLICK OR V TO PLANT";
    ui.build(hud); snapshot(ui,hud,"strawberry-seeds.ppm");
    hud.farming=true; hud.farmGarden=true; hud.farmShop=false; hud.help=true;
    world.farm.garden.initialized=true; world.farm.garden.coins=12; world.farm.garden.weather=160;
    world.farm.carrots=4; world.farm.strawberries=3; world.farm.pumpkins=1;
    hud.farm=farmView(world);
    hud.tools.choose(Item::WateringCan,world.crafting); hud.interaction="STRAWBERRIES / GROWTH 73 OF 100 / WET / COMPOST +2";
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y;
      for(std::uint32_t flags : {0u,1u,3u,7u,15u,31u,63u}) {
        world.farm.garden.flags=flags; hud.guide=gardenGuide(world,player);
        ui.build(hud); bounds(ui,size.x,size.y);
      }
      snapshot(ui,hud,size.x==800 ? "garden-hud-small.ppm" : "garden-hud.ppm");
    }
    hud.width=800; hud.height=600; hud.paused=true;
    ui.build(hud); bounds(ui,hud.width,hud.height); snapshot(ui,hud,"garden-pause.ppm");
    hud.paused=false; hud.menu=Menu::Farm;
    ui.build(hud); snapshot(ui,hud,"garden-playing.ppm");
    hud.farmShop=true; ui.build(hud); snapshot(ui,hud,"shop-playing.ppm");
    hud.farmRanch=true; hud.farm.garden.coins=55;
    for(auto size : {glm::ivec2(800,600),glm::ivec2(1280,800)}) {
      hud.width=size.x; hud.height=size.y; auto x=size.x*.5f-340,y=size.y*.5f-276;
      constexpr std::array animals{FarmAction::BuyCow,FarmAction::BuyHorse,FarmAction::BuySheep,FarmAction::BuyFox};
      for(int i=0;i<4;++i) check(Ui::farmActionAt(size.x,size.y,x+272+(i%2)*328,y+312+(i/2)*98,true,true,true)==animals[i],"each animal purchase has its own button");
      check(Ui::farmActionAt(size.x,size.y,x+330,y+462,true,true,true)==FarmAction::Car,"free car remains available from the animal shop");
      check(Ui::farmActionAt(size.x,size.y,x+180,y+493,true,true,true)==FarmAction::GardenShop
        && Ui::farmActionAt(size.x,size.y,x+510,y+493,true,true,false)==FarmAction::RanchShop,"both shop sections can be reached");
      ui.build(hud); bounds(ui,size.x,size.y); snapshot(ui,hud,size.x==800 ? "ranch-shop-small.ppm" : "ranch-shop.ppm");
    }
    hud.farmRanch=false;
    hud.farmShop=false; hud.guide.enabled=false; hud.farming=false;
    hud.naming=false; hud.menu=Menu::None; hud.width=800; hud.height=600;
    hud.animalLabels={{{400,240},"Clover",false},{{540,285},"Buttercup Peep 123",true}};
    ui.build(hud); bounds(ui,hud.width,hud.height); snapshot(ui,hud,"animal-names.ppm");
    hud.guide.enabled=true; hud.guide.farm=true; hud.guide.destinationName="Buttercup Peep 123"; hud.waypointDistance=123456;
    for(float x : {-100.f,900.f}) { hud.waypoint=glm::vec2(x,450); ui.build(hud); bounds(ui,hud.width,hud.height); }
    std::cout<<"PASS UI hit targets and layout bounds\n";
  } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

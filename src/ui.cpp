#include "ui.hpp"
#include "ui_font.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

namespace bw {
namespace {
using Glyph=std::array<unsigned char,7>;
Glyph glyph(char c) {
  switch(c) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '\'': return {4,4,8,0,0,0,0};
    case ':': return {0,4,4,0,4,4,0}; case '.': return {0,0,0,0,0,6,6};
    case '/': return {1,1,2,4,8,16,16}; case '-': return {0,0,0,31,0,0,0};
    case '+': return {0,4,4,31,4,4,0}; case '[': return {14,8,8,8,8,8,14};
    case '%': return {25,25,2,4,8,19,19};
    case ']': return {14,2,2,2,2,2,14}; default: return {};
  }
}
constexpr glm::vec4 cream{.96f,.94f,.84f,1}, muted{.72f,.79f,.73f,1}, panel{.065f,.105f,.10f,.87f}, accent{.77f,.87f,.47f,1};
constexpr float workshopWidth=680,workshopHeight=552,recipeTop=238,recipeStride=60,recipeHeight=54;
constexpr float toolsTop=166,toolStrideY=76,toolHeight=70;
int toolColumns(PlayMode mode) { return mode==PlayMode::Build ? 6 : 3; }
constexpr glm::vec4 removeColor{.98f,.57f,.42f,1},buildColor{.57f,.80f,.94f,1};
glm::vec4 modeColor(PlayMode mode) { return mode==PlayMode::Remove ? removeColor : mode==PlayMode::Build ? buildColor : accent; }
}
void Ui::quad(glm::vec2 a,glm::vec2 b,glm::vec2 c,glm::vec2 d,glm::vec4 color) {
  for (auto p : {a,b,c,a,c,d}) vertices.push_back({p,color});
}
void Ui::rectangle(float x,float y,float w,float h,glm::vec4 color) {
  quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},color);
}
void Ui::text(std::string_view value,float x,float y,float scale,glm::vec4 color) {
  for (char ch : value) {
    auto g=glyph(char(std::toupper(static_cast<unsigned char>(ch))));
    for (int row=0;row<7;++row) for (int col=0;col<5;++col)
      if (g[row]&(1u<<(4-col))) rectangle(x+col*scale,y+row*scale,scale,scale,color);
    x+=6*scale;
  }
}
void Ui::centered(std::string_view value,float x,float y,float scale,glm::vec4 color) {
  text(value,x-(float(value.size())*6-1)*scale*.5f,y,scale,color);
}
void Ui::label(std::string_view value,float x,float y,float size,glm::vec4 color) {
  const auto& font=readableFont(); float scale=size/FontAtlas::em;
  for(unsigned char c : value) {
    const auto& glyph=font.glyphs[c<128 ? c : '?'];
    if(glyph.size.x>0) {
      auto lo=glm::vec2(x,y)+glyph.offset*scale,hi=lo+glyph.size*scale;
      std::array<UiVertex,4> q{{{lo,color,glyph.uvMin},{{hi.x,lo.y},color,{glyph.uvMax.x,glyph.uvMin.y}},
        {hi,color,glyph.uvMax},{{lo.x,hi.y},color,{glyph.uvMin.x,glyph.uvMax.y}}}};
      for(int index : {0,1,2,0,2,3}) vertices.push_back(q[index]);
    }
    x+=glyph.advance*scale;
  }
}
void Ui::labelCentered(std::string_view value,float x,float y,float size,glm::vec4 color) {
  label(value,x-readableWidth(value,size)*.5f,y,size,color);
}
void Ui::cube(Block block,float x,float y,float size) {
  if(block==Block::StoneSlab) {
    auto color=blockColor(Block::Stone);
    quad({x,y+size*.5f},{x+size,y+size},{x,y+size*1.5f},{x-size,y+size},glm::vec4(color*1.1f,1));
    quad({x-size,y+size},{x,y+size*1.5f},{x,y+size*2},{x-size,y+size*1.5f},glm::vec4(color*.75f,1));
    quad({x,y+size*1.5f},{x+size,y+size},{x+size,y+size*1.5f},{x,y+size*2},glm::vec4(color*.9f,1));
    return;
  }
  if(isCrop(block) && !isWheat(block)) {
    float s=size/15.f;
    auto r=[&](float dx,float dy,float w,float h,glm::vec4 color){rectangle(x+dx*s,y+dy*s,w*s,h*s,color);};
    constexpr glm::vec4 green{.39f,.72f,.24f,1},orange{.98f,.51f,.12f,1};
    auto kind=cropKind(block);
    if(kind==CropKind::Carrot) {
      r(-8,10,16,11,orange); r(-6,21,12,7,orange); r(-3,28,6,7,orange);
      r(-2,0,4,11,green); r(-10,2,7,5,green); r(4,1,7,5,green);
      r(-7,14,7,2,{1.f,.72f,.27f,1}); r(-4,24,6,2,{.84f,.36f,.07f,1});
    } else if(kind==CropKind::Strawberry) {
      r(-13,10,26,17,{.90f,.18f,.29f,1}); r(-9,27,18,5,{.90f,.18f,.29f,1}); r(-4,32,8,4,{.90f,.18f,.29f,1});
      r(-11,6,22,5,green); r(-3,1,6,13,green);
      for(int row=0;row<3;++row) for(int col=0;col<3;++col) r(-8+col*7+(row%2),14+row*6,2,2,{1.f,.87f,.4f,1});
    } else {
      r(-16,11,32,21,orange); r(-12,7,24,30,orange);
      for(float dx : {-8.f,0.f,8.f}) r(dx-1,10,2,25,{.79f,.34f,.08f,1});
      r(-3,1,6,8,{.41f,.31f,.13f,1}); r(3,3,9,4,green);
    }
    return;
  }
  if(isWheat(block)) {
    for(float offset : {-9.f,0.f,9.f}) {
      rectangle(x+offset-1,y+12,2,24,{.43f,.66f,.24f,1});
      rectangle(x+offset-3,y+5,6,14,{.94f,.75f,.30f,1});
      rectangle(x+offset-5,y+17,10,3,{.65f,.73f,.29f,1});
    }
    return;
  }
  if(block==Block::Fence || isGate(block)) {
    auto wood=glm::vec4(.72f,.49f,.26f,1);
    rectangle(x-19,y+4,7,31,wood); rectangle(x+12,y+4,7,31,wood);
    rectangle(x-18,y+11,36,5,wood); rectangle(x-18,y+25,36,5,wood);
    if(isGate(block)) rectangle(x+3,y+18,4,4,accent);
    return;
  }
  if(isBed(block)) {
    rectangle(x-18,y+13,36,15,{.45f,.27f,.15f,1});
    rectangle(x-18,y+10,36,13,{.75f,.28f,.23f,1});
    rectangle(x+6,y+8,11,10,{.96f,.92f,.80f,1});
    rectangle(x-15,y+28,4,6,{.45f,.27f,.15f,1});
    rectangle(x+11,y+28,4,6,{.45f,.27f,.15f,1}); return;
  }
  if(block==Block::Torch) {
    rectangle(x-3,y+size*.65f,6,size*1.25f,{.46f,.29f,.16f,1});
    quad({x,y},{x+8,y+size*.6f},{x,y+size},{x-8,y+size*.6f},{1.f,.66f,.19f,1});
    rectangle(x-2,y+size*.4f,4,6,{1.f,.94f,.65f,1}); return;
  }
  if(isDoor(block)) {
    rectangle(x-10,y,20,size*2,{.45f,.28f,.13f,1});
    rectangle(x-8,y+2,16,size*2-4,{.69f,.46f,.23f,1});
    rectangle(x-6,y+4,12,9,{.27f,.33f,.29f,1});
    rectangle(x-1,y+4,2,9,{.49f,.32f,.15f,1});
    rectangle(x+4,y+size*1.25f,3,3,{.96f,.82f,.43f,1}); return;
  }
  auto color=blockColor(block);
  float h=size*.5f;
  glm::vec2 top{x,y},left{x-size,y+h},middle{x,y+size},right{x+size,y+h},bottom{x,y+size*2};
  quad(top,right,middle,left,glm::vec4(color*1.10f,1));
  auto side=block==Block::Grass ? blockColor(Block::Dirt) : color;
  quad(left,middle,bottom,{x-size,y+size*1.5f},glm::vec4(side*.75f,1));
  quad(middle,right,{x+size,y+size*1.5f},bottom,glm::vec4(side*.92f,1));
  if(block==Block::Grass) {
    quad(left,middle,{x,y+size*1.20f},{x-size,y+h+size*.20f},glm::vec4(color*.75f,1));
    quad(middle,right,{x+size,y+h+size*.20f},{x,y+size*1.20f},glm::vec4(color*.92f,1));
  }
  for(int i=0;i<5;++i) {
    float px=x-size*.45f+float((i*7)%5)*size*.18f, py=y+size*.36f+float((i*3)%4)*size*.1f;
    rectangle(px,py,2,2,glm::vec4(color*.82f,1));
  }
  if(block==Block::Glass) {
    quad({x-1,y+4},{x+size-4,y+size*.5f+2},{x+size-4,y+size*.5f+5},{x-1,y+7},{.93f,1.f,1.f,1});
    rectangle(x+3,y+size+1,2,8,{.94f,1.f,1.f,1});
  }
  if(block==Block::Workbench) {
    for(float t : {.33f,.66f}) {
      auto a=glm::mix(left,top,t),b=glm::mix(middle,right,t);
      quad(a,b,b+glm::vec2(0,1.5f),a+glm::vec2(0,1.5f),{.26f,.16f,.09f,1});
      a=glm::mix(top,right,t); b=glm::mix(left,middle,t);
      quad(a,b,b+glm::vec2(0,1.5f),a+glm::vec2(0,1.5f),{.26f,.16f,.09f,1});
    }
  }
}
void Ui::itemIcon(Item item,float x,float y,float size) {
  auto tool=itemTool(item);
  if(item==Item::Hoe || item==Item::Compost || item==Item::Sprinkler || item==Item::Greenhouse) {
    float s=size/15.f;
    auto r=[&](float dx,float dy,float w,float h,glm::vec4 color){rectangle(x+dx*s,y+dy*s,w*s,h*s,color);};
    if(item==Item::Hoe) {
      r(-2,6,4,32,{.72f,.47f,.25f,1}); r(-15,5,23,6,{.65f,.72f,.72f,1}); r(-17,8,8,9,{.49f,.58f,.58f,1});
    } else if(item==Item::Compost) {
      r(-12,9,24,28,{.66f,.48f,.27f,1}); r(-9,5,18,5,{.85f,.70f,.44f,1});
      r(-8,18,16,12,{.32f,.55f,.24f,1}); r(-4,22,8,3,accent);
    } else if(item==Item::Sprinkler) {
      r(-14,31,28,5,{.53f,.61f,.61f,1}); r(-3,11,6,22,{.35f,.70f,.77f,1});
      r(-17,10,34,5,{.47f,.83f,.88f,1}); r(-16,3,3,4,{.55f,.85f,1.f,1}); r(13,3,3,4,{.55f,.85f,1.f,1});
    } else {
      r(-18,9,36,27,{.55f,.77f,.76f,1});
      for(float dx : {-18.f,-2.f,15.f}) r(dx,9,3,27,{.73f,.54f,.30f,1});
      r(-18,8,36,3,cream); r(-18,21,36,3,cream); r(-18,34,36,3,{.73f,.54f,.30f,1});
    }
    return;
  }
  if(item==Item::WateringCan) {
    float s=size/15.f;
    auto r=[&](float dx,float dy,float w,float h,glm::vec4 color){rectangle(x+dx*s,y+dy*s,w*s,h*s,color);};
    constexpr glm::vec4 blue{.25f,.65f,.79f,1},light{.53f,.85f,.89f,1};
    r(7,8,12,4,light); r(15,8,4,21,light); r(7,25,12,4,light);
    r(-12,13,24,23,blue); r(-12,12,24,4,light); r(-8,19,4,12,light);
    r(-18,14,7,8,blue); r(-23,7,7,11,blue); r(-26,5,11,4,light);
    return;
  }
  if(item==Item::Empty) { rectangle(x-10,y+14,20,17,{.83f,.62f,.43f,1}); return; }
  if(tool==Tool::Hands) { cube(itemBlock(item),x,y,size); return; }
  float s=size/15.f;
  auto r=[&](float dx,float dy,float w,float h,glm::vec4 color){rectangle(x+dx*s,y+dy*s,w*s,h*s,color);};
  r(-3,8,6,30,{.54f,.31f,.15f,1}); r(-2,8,2,28,{.75f,.50f,.26f,1});
  if(tool==Tool::Axe) {
    r(-18,3,22,16,{.76f,.49f,.23f,1}); r(-21,1,4,20,{.94f,.74f,.44f,1});
    r(-16,6,12,2,{.59f,.36f,.16f,1});
  } else {
    r(-17,3,34,7,{.66f,.71f,.71f,1}); r(-20,7,6,12,{.53f,.58f,.58f,1});
    r(14,7,6,12,{.53f,.58f,.58f,1}); r(-21,17,3,6,cream); r(18,17,3,6,cream);
  }
}
void Ui::hammerIcon(float x,float y,float size) {
  float scale=size/15.f;
  rectangle(x-3*scale,y+8*scale,6*scale,30*scale,{.70f,.43f,.24f,1});
  rectangle(x-17*scale,y+3*scale,34*scale,14*scale,{.64f,.71f,.74f,1});
  rectangle(x-17*scale,y+3*scale,5*scale,14*scale,removeColor);
}
void Ui::modeButtons(const HudState& h,float x,float y,float width,float height) {
  float stride=(width+8)/3;
  for(int i=0;i<3;++i) {
    auto mode=PlayMode(i); auto ink=modeColor(mode); bool active=h.tools.mode==mode;
    rectangle(x+i*stride,y,stride-8,height,active ? ink : panel);
    labelCentered(modeName(mode),x+i*stride+(stride-8)*.5f,y+(height-28)*.5f,22,active ? panel : cream);
  }
}
int Ui::toolAt(int width,int height,float px,float py,PlayMode mode,int page) {
  float x=float(width)*.5f-workshopWidth*.5f+24,y=float(height)*.5f-workshopHeight*.5f+toolsTop;
  if(px<x || py<y) return -1;
  int columns=toolColumns(mode); float stride=640.f/float(columns);
  int col=int((px-x)/stride),row=int((py-y)/toolStrideY),index=row*columns+col+(mode==PlayMode::Build ? std::clamp(page,0,1)*24 : 0);
  if(row>=4 || col>=columns || index>=int(modeTools(mode).size()) || px-x-col*stride>=stride-8 || py-y-row*toolStrideY>=toolHeight) return -1;
  return index;
}
int Ui::toolPageAt(int width,int height,float px,float py) {
  float x=float(width)*.5f-workshopWidth*.5f,y=float(height)*.5f-workshopHeight*.5f;
  if(py<y+480 || py>=y+514) return -1;
  if(px>=x+24 && px<x+166) return 0;
  if(px>=x+514 && px<x+656) return 1;
  return -1;
}
std::optional<PlayMode> Ui::modeAt(int width,int height,float px,float py) {
  float x=float(width)*.5f-workshopWidth*.5f+24,y=float(height)*.5f-workshopHeight*.5f+76;
  if(px<x || py<y || py>=y+50) return {};
  constexpr float stride=640.f/3;
  int col=int((px-x)/stride);
  if(col>=3 || px-x-col*stride>=stride-8) return {};
  return PlayMode(col);
}
Menu Ui::menuTabAt(int width,int height,float px,float py) {
  float x=float(width)*.5f-workshopWidth*.5f,y=float(height)*.5f-workshopHeight*.5f;
  if(py<y+16 || py>=y+48) return Menu::None;
  if(px>=x+304 && px<x+400) return Menu::Inventory;
  if(px>=x+408 && px<x+558) return Menu::Crafting;
  if(px>=x+566 && px<x+656) return Menu::Farm;
  return Menu::None;
}
void Ui::menuTabs(const HudState& h) {
  float x=float(h.width)*.5f-workshopWidth*.5f,y=float(h.height)*.5f-workshopHeight*.5f;
  auto tab=[&](Menu menu,float offset,float width,std::string_view label) {
    rectangle(x+offset,y+16,width,32,h.menu==menu ? accent : panel);
    labelCentered(label,x+offset+width*.5f,y+20,17,h.menu==menu ? panel : cream);
  };
  tab(Menu::Inventory,304,96,"Tools"); tab(Menu::Crafting,408,150,"Crafting"); tab(Menu::Farm,566,90,"Farm");
}
int Ui::farmAnimalAt(int width,int height,float px,float py,int page,int count) {
  float x=float(width)*.5f-workshopWidth*.5f,y=float(height)*.5f-workshopHeight*.5f;
  if(px<x+24 || px>=x+274 || py<y+204) return -1;
  int row=int((py-y-204)/38),index=page*6+row;
  return row<6 && py-y-204-row*38<34 && index>=0 && index<count ? index : -1;
}
FarmAction Ui::farmActionAt(int width,int height,float px,float py,bool garden,bool shop,bool ranch) {
  float x=float(width)*.5f-workshopWidth*.5f,y=float(height)*.5f-workshopHeight*.5f;
  auto in=[&](float bx,float by,float w,float h) { return px>=x+bx && px<x+bx+w && py>=y+by && py<y+by+h; };
  if(in(24,92,200,36)) return FarmAction::Garden;
  if(in(240,92,200,36)) return FarmAction::Shop;
  if(in(456,92,200,36)) return FarmAction::Animals;
  if(shop) {
    if(in(448,177,192,48)) return FarmAction::Sell;
    if(in(24,478,304,30)) return FarmAction::GardenShop;
    if(in(352,478,304,30)) return FarmAction::RanchShop;
    if(ranch) {
      constexpr std::array actions{FarmAction::BuyCow,FarmAction::BuyHorse,FarmAction::BuySheep,FarmAction::BuyFox};
      for(int i=0;i<4;++i) if(in(228+(i%2)*328,296+(i/2)*98,88,32)) return actions[i];
      if(in(24,449,632,26)) return FarmAction::Car;
      return FarmAction::None;
    }
    constexpr std::array buy{FarmAction::BuyCompost,FarmAction::BuySprinkler,FarmAction::BuyGreenhouse};
    constexpr std::array take{FarmAction::Compost,FarmAction::Sprinkler,FarmAction::Greenhouse};
    for(int i=0;i<3;++i) {
      if(in(444,264+i*76,100,44)) return buy[i];
      if(in(556,264+i*76,84,44)) return take[i];
    }
    return FarmAction::None;
  }
  if(garden) {
    constexpr std::array actions{FarmAction::Seeds,FarmAction::Carrots,FarmAction::Strawberries,FarmAction::Pumpkins};
    for(int i=0;i<4;++i) if(in(24+(i%2)*328,144+(i/2)*104,304,92)) return actions[i];
    if(in(24,356,200,62)) return FarmAction::Hoe;
    if(in(240,356,200,62)) return FarmAction::WateringCan;
    if(in(456,356,200,62)) return FarmAction::Compost;
    if(in(24,466,196,34)) return FarmAction::Visit;
    if(in(242,466,196,34)) return FarmAction::Shop;
    if(in(460,466,196,34)) return FarmAction::Cabin;
    return FarmAction::None;
  }
  if(in(300,204,332,36)) return FarmAction::Name;
  if(in(542,248,90,26)) return FarmAction::SaveName;
  if(in(300,361,332,42)) return FarmAction::Hatch;
  if(in(300,413,332,35)) return FarmAction::Find;
  if(in(24,448,116,30)) return FarmAction::Previous;
  if(in(158,448,116,30)) return FarmAction::Next;
  if(in(24,488,250,28)) return FarmAction::Cabin;
  return FarmAction::None;
}
void Ui::farmPage(const HudState& h) {
  float cx=float(h.width)*.5f,x=cx-workshopWidth*.5f,y=float(h.height)*.5f-workshopHeight*.5f;
  rectangle(0,0,float(h.width),float(h.height),{.025f,.055f,.05f,.60f});
  rectangle(x,y,workshopWidth,workshopHeight,{.07f,.115f,.105f,.98f}); rectangle(x,y,workshopWidth,3,accent);
  label("Your farm",x+24,y+15,30,cream);
  label(h.farmShop ? "Sell your harvest and improve your farm." : h.farmGarden ? "Choose seeds or a tool. Seeds and water are free." : "Feed your hens, collect eggs, and raise chicks.",x+24,y+61,17,cream);
  auto button=[&](float bx,float by,float w,float height,std::string_view caption,bool enabled=true) {
    rectangle(x+bx,y+by,w,height,enabled ? glm::vec4(.26f,.38f,.25f,1) : panel);
    labelCentered(caption,x+bx+w*.5f,y+by+(height-21)*.5f,17,enabled ? cream : muted);
  };
  button(24,92,200,36,"Garden",h.farmGarden && !h.farmShop); button(240,92,200,36,"Shop",h.farmShop);
  button(456,92,200,36,"Animals",!h.farmGarden && !h.farmShop);
  if(h.farmShop) {
    label("Your coins: "+std::to_string(h.farm.garden.coins),x+24,y+139,23,accent);
    if(h.farm.milk) label("Milk in basket: "+std::to_string(h.farm.milk),x+400,y+143,17,cream);
    rectangle(x+24,y+174,632,62,panel);
    label("Harvest value: "+std::to_string(h.farm.basketValue)+" coins",x+38,y+179,19,cream);
    label("Sells crops and milk. Keeps your eggs.",x+38,y+207,15,cream);
    button(448,177,192,48,"Sell basket",h.farm.basketValue>0 && h.farm.basketValue<=coinLimit-h.farm.garden.coins);
    if(h.farmRanch) {
      constexpr std::array names{"Cow - 20 coins","Horse - 35 coins","Sheep - 12 coins","Friendly fox - 15 coins"};
      constexpr std::array notes{"V: milk. Sell each bottle for 5 coins.","V: ride. WASD or arrows to move.","A woolly friend. V: pet.","V: pet. Your chickens are safe."};
      std::array stocks{h.farm.cows,h.farm.horses,h.farm.sheep,h.farm.foxes};
      int total=h.farm.cows+h.farm.horses+h.farm.sheep+h.farm.foxes;
      for(int i=0;i<4;++i) {
        float bx=24+(i%2)*328,by=246+(i/2)*98;
        rectangle(x+bx,y+by,304,92,{.14f,.23f,.17f,1});
        label(names[i],x+bx+12,y+by+5,19,cream); label(notes[i],x+bx+12,y+by+34,15,cream);
        label("On your farm: "+std::to_string(stocks[i]),x+bx+12,y+by+65,15,accent);
        button(bx+204,by+50,88,32,i==3 ? "Adopt" : "Buy",total<int(livestockLimit) && h.farm.garden.coins>=livestockPrice(LivestockKind(i)));
      }
      button(24,449,632,26,h.farm.carOwned ? "Bring my car here (C)" : "Get my free car (C)");
    } else {
    constexpr std::array items{Item::Compost,Item::Sprinkler,Item::Greenhouse};
    constexpr std::array names{"Compost - 3 coins","Sprinkler - 12 coins","Greenhouse - 40 coins"};
    constexpr std::array notes{"5 uses / +2 vegetables per harvest","Waters a 5 x 5 area","5 x 5 building / 25% faster growth"};
    std::array stocks{h.farm.garden.compost,h.farm.garden.sprinklers,h.farm.garden.greenhouses};
    constexpr std::array prices{3,12,40};
    for(int i=0;i<3;++i) {
      float by=250+i*76;
      rectangle(x+24,y+by,632,68,{.14f,.23f,.17f,1}); itemIcon(items[i],x+58,y+by+12,16);
      label(names[i],x+90,y+by+4,19,cream);
      label(notes[i],x+90,y+by+28,15,cream);
      label("In your bag: "+std::to_string(stocks[i]),x+90,y+by+48,14,accent);
      button(444,by+14,100,44,"Buy",h.farm.garden.coins>=prices[i] && stocks[i]<=gardenSupplyLimit-(i==0 ? 5 : 1));
      button(556,by+14,84,44,"Equip",stocks[i]>0);
    }
    }
    button(24,478,304,30,"Garden upgrades",!h.farmRanch);
    button(352,478,304,30,"Animals & car",h.farmRanch);
    if(!h.notice.empty()) labelCentered(h.notice,cx,y+510,std::min(15.f,15.f*620.f/readableWidth(h.notice,15)),accent);
    labelCentered("Esc or P: Back to game",cx,y+528,16,cream);
    return;
  }
  if(h.farmGarden) {
    constexpr std::array items{Item::Wheat,Item::Carrot,Item::Strawberry,Item::Pumpkin};
    for(int i=0;i<4;++i) {
      float bx=x+24+(i%2)*328,by=y+144+(i/2)*104;
      rectangle(bx,by,304,92,{.14f,.23f,.17f,1}); rectangle(bx,by,3,92,accent);
      itemIcon(items[i],bx+34,by+21,16);
      constexpr std::array names{"Plant wheat","Plant carrots","Plant strawberries","Plant pumpkins"};
      label(names[i],bx+69,by+9,21,cream);
      label("In basket: "+std::to_string(h.farm.harvest[i]),bx+69,by+38,18,accent);
      constexpr std::array descriptions{"90 seconds / Replant","75 seconds / Replant","105 seconds / Grows again","150 seconds / Leave a gap"};
      label(descriptions[i],bx+69,by+66,15,cream);
    }
    constexpr std::array tools{Item::Hoe,Item::WateringCan,Item::Compost};
    constexpr std::array labels{"Hoe","Watering can","Compost"};
    for(int i=0;i<3;++i) {
      float bx=24+i*216;
      rectangle(x+bx,y+356,200,62,{.13f,.27f,.23f,1}); itemIcon(tools[i],x+bx+26,y+369,13);
      label(labels[i],x+bx+53,y+362,18,cream);
      label(i==0 ? "Prepare soil" : i==1 ? "Faster growth" : "Uses left: "+std::to_string(h.farm.garden.compost),x+bx+53,y+392,15,cream);
    }
    label(std::to_string(h.farm.plants)+" plants     "+std::to_string(h.farm.ripe)+" ready to pick     "+std::to_string(h.farm.watered)+" watered",x+24,y+431,18,cream);
    button(24,466,196,34,"Visit garden",h.farm.garden.initialized); button(242,466,196,34,"Sell / Upgrades"); button(460,466,196,34,"Go home (R)");
    labelCentered("Farm mode: Click to use tools or pick ripe crops.",cx,y+507,15,cream);
    labelCentered("Esc or P: Back to game. Growing pauses here.",cx,y+529,15,cream);
    return;
  }
  char summary[100];
  std::snprintf(summary,sizeof(summary),"WHEAT %d   EGGS %d   PLANTS %d / %d RIPE",h.farm.wheat,h.farm.eggs,h.farm.plants,h.farm.ripe);
  text(summary,x+24,y+152,1.3f,cream);
  text("YOUR FLOCK / "+std::to_string(h.farm.animals.size())+" OF 12",x+24,y+181,1.2f,accent);
  for(int row=0;row<6;++row) {
    int index=h.farmPage*6+row; if(index>=int(h.farm.animals.size())) break;
    const auto& animal=h.farm.animals[index]; float top=y+204+row*38;
    rectangle(x+24,top,250,34,index==h.animalSelected ? glm::vec4(.24f,.33f,.23f,1) : panel);
    rectangle(x+34,top+10,12,12,animal.baby ? glm::vec4(.95f,.76f,.28f,1) : cream);
    text(animal.name,x+55,top+7,1.25f,cream);
    text(animal.baby ? "CHICK" : animal.hatching>=0 ? "KEEPING AN EGG WARM" : "HEN",x+55,top+22,.9f,muted);
  }
  button(24,448,116,30,"PREVIOUS",h.farmPage>0);
  button(158,448,116,30,"NEXT",(h.farmPage+1)*6<int(h.farm.animals.size()));
  if(h.animalSelected>=0 && h.animalSelected<int(h.farm.animals.size())) {
    const auto& animal=h.farm.animals[h.animalSelected];
    text(animal.baby ? "BABY CHICK" : "ADULT HEN",x+300,y+181,1.2f,accent);
    rectangle(x+300,y+204,332,36,h.naming ? glm::vec4(.30f,.39f,.27f,1) : panel);
    if(h.naming && h.nameSelectedAll) rectangle(x+308,y+212,std::max(8.f,float(h.nameDraft.size())*9.f),17,{.46f,.55f,.32f,1});
    text(h.naming ? h.nameDraft : animal.name,x+312,y+216,1.5f,cream);
    if(h.naming && !h.nameSelectedAll) rectangle(x+312+float(h.nameDraft.size())*9,y+213,2,19,accent);
    text(h.naming ? "ENTER SAVES / ESC CANCELS" : "CLICK THE NAME TO RENAME",x+300,y+252,1.05f,muted);
    if(h.naming) button(542,248,90,26,"SAVE");
    text(animal.status,x+300,y+288,std::min(1.3f,324.f/(6*float(std::max(std::size_t(1),animal.status.size())))),cream);
    if(animal.hatching>=0) text("STAYS HERE UNTIL HER CHICK HATCHES",x+300,y+313,1.1f,muted);
    else if(!animal.mother.empty()) text("MOTHER / "+animal.mother,x+300,y+313,1.15f,muted);
    else text("WHEAT MAKES HER FOLLOW YOU",x+300,y+313,1.1f,muted);
    if(animal.baby || animal.hatching>=0) {
      rectangle(x+300,y+337,332,5,panel);
      float progress=animal.baby ? animal.growth : 1-std::clamp(animal.hatching/eggHatchSeconds,0.f,1.f);
      rectangle(x+300,y+337,332*progress,5,accent);
    }
    button(300,361,332,42,animal.hatching>=0 ? "EGG IS WARM AND SAFE" : "HATCH ONE EGG",animal.hatchProblem.empty());
    button(300,413,332,35,"FIND THIS ANIMAL");
    auto hint=animal.hatching>=0 ? "RETURN TO THE WORLD TO LET TIME PASS" : animal.hatchProblem.empty() ? "USES 1 EGG / HATCHES IN 60 PLAY SECONDS" : animal.hatchProblem;
    centered(hint,x+466,y+463,std::min(1.1f,328.f/(6*float(hint.size()))),muted);
  } else text("YOUR FLOCK LIVES NEAR THE MEADOW",x+300,y+250,1.05f,muted);
  button(24,488,250,28,"Go home (R)");
  if(!h.notice.empty()) centered(h.notice,x+466,y+497,std::min(1.1f,332.f/(float(h.notice.size())*6)),accent);
  centered("P / E / ESC RETURN TO WORLD   GROWING PAUSES ON THIS PAGE",cx,y+529,1.1f,cream);
}
void Ui::inventory(const HudState& h) {
  float cx=float(h.width)*.5f,x=cx-workshopWidth*.5f,y=float(h.height)*.5f-workshopHeight*.5f;
  auto ink=modeColor(h.tools.mode);
  rectangle(0,0,float(h.width),float(h.height),{.025f,.055f,.05f,.60f});
  rectangle(x,y,workshopWidth,workshopHeight,{.07f,.115f,.105f,.98f}); rectangle(x,y,workshopWidth,3,ink);
  label(h.tools.mode==PlayMode::Build ? "Build materials" : "Choose tools",x+24,y+15,28,cream);
  label("Choose a mode, then click a tool to start playing.",x+24,y+53,17,cream);
  modeButtons(h,x+24,y+76,632,50);
  label(h.tools.mode==PlayMode::Farm ? "Tend crops and animals. Your buildings stay safe."
    : h.tools.mode==PlayMode::Build ? "Left-click builds. Right-click removes. V also places."
    : "Click to remove one block instantly. Animals stay safe.",x+24,y+136,17,ink);
  auto choices=modeTools(h.tools.mode); int columns=toolColumns(h.tools.mode); float stride=640.f/float(columns);
  int first=h.tools.mode==PlayMode::Build ? std::clamp(h.toolPage,0,1)*24 : 0;
  for(int i=first;i<std::min(first+24,int(choices.size()));++i) {
    int slot=i-first;
    float sx=x+24+(slot%columns)*stride,sy=y+toolsTop+(slot/columns)*toolStrideY,width=stride-8;
    bool ready=itemAvailable(choices[i],h.craft.bag),selected=choices[i]==h.selectedItem();
    rectangle(sx,sy,width,toolHeight,selected ? glm::vec4(.23f,.33f,.28f,1) : panel);
    if(selected) rectangle(sx,sy,3,toolHeight,ink);
    if(h.tools.removesBlocks() && choices[i]==Item::Empty) hammerIcon(sx+width*.5f,sy+4,11);
    else itemIcon(choices[i],sx+width*.5f,sy+4,11);
    auto name=toolName(choices[i]); float size=std::min(17.f,17.f*(width-12)/std::max(1.f,readableWidth(name,17)));
    labelCentered(name,sx+width*.5f,sy+44,size,cream);
    if(i<9) label(std::to_string(i+1),sx+8,sy+5,13,muted);
    if(!ready) { rectangle(sx,sy,width,toolHeight,{.03f,.06f,.05f,.45f}); label("Craft first",sx+8,sy+5,13,ink); }
  }
  std::string selection=std::string(modeName(h.tools.mode))+" / "+std::string(toolName(h.selectedItem()));
  labelCentered(selection,cx,y+485,19,ink);
  if(h.tools.mode==PlayMode::Build) {
    rectangle(x+24,y+480,142,34,h.toolPage==0 ? ink : panel);
    labelCentered("Materials",x+95,y+486,17,h.toolPage==0 ? panel : cream);
    rectangle(x+514,y+480,142,34,h.toolPage==1 ? ink : panel);
    labelCentered("More",x+585,y+486,17,h.toolPage==1 ? panel : cream);
  }
  labelCentered("E or Esc: Back to game",cx,y+526,16,cream);
}
int Ui::recipeAt(int width,int height,float px,float py) {
  float x=float(width)*.5f-workshopWidth*.5f,y=float(height)*.5f-workshopHeight*.5f;
  if(px<x+20 || px>=x+workshopWidth-20) return -1;
  for(int i=0;i<4;++i) if(py>=y+recipeTop+i*recipeStride && py<y+recipeTop+i*recipeStride+recipeHeight) return i;
  return -1;
}
void Ui::workshop(const HudState& h) {
  float cx=float(h.width)*.5f,x=cx-workshopWidth*.5f,y=float(h.height)*.5f-workshopHeight*.5f;
  rectangle(0,0,float(h.width),float(h.height),{.025f,.055f,.05f,.70f});
  rectangle(x,y,workshopWidth,workshopHeight,{.07f,.115f,.105f,1}); rectangle(x,y,workshopWidth,3,accent);
  label("Crafting",x+24,y+15,30,cream);
  label("Make tools here. Building blocks are always free.",x+24,y+59,17,cream);
  rectangle(x+20,y+88,640,91,{.13f,.20f,.15f,1});
  label(h.craft.title,x+34,y+92,21,accent);
  for(int i=0;i<2;++i) label(h.craft.lines[i],x+34,y+121+i*22,17,cream);
  for(int i=0;i<7;++i) rectangle(x+34+i*86,y+173,75,3,i<h.craft.lesson ? accent : glm::vec4(.27f,.35f,.28f,1));
  std::string bag="Logs: "+std::to_string(h.craft.bag.wood)+"     Planks: "+std::to_string(h.craft.bag.planks)+"     Stone: "+std::to_string(h.craft.bag.stone);
  label(bag,x+24,y+187,20,cream);
  label(h.craft.benchNearby ? "Workbench nearby - you can make tools" : "Place a workbench nearby to make an axe or pickaxe",x+24,y+214,16,cream);
  for(int i=0;i<4;++i) {
    const auto& recipe=recipes()[i];
    bool done=i!=0 && h.craft.bag.has(recipe.unlock),ready=h.craft.problems[i].empty();
    float top=y+recipeTop+i*recipeStride;
    rectangle(x+20,top,640,recipeHeight,i==h.recipeSelected ? glm::vec4(.22f,.32f,.23f,1) : glm::vec4(.12f,.20f,.15f,1));
    if(i==h.recipeSelected) rectangle(x+20,top,3,recipeHeight,accent);
    rectangle(x+30,top+9,30,36,panel); labelCentered(std::to_string(i+1),x+45,top+12,20,accent);
    label(recipe.name,x+74,top+3,20,cream);
    label(recipe.cost,x+278,top+6,16,cream);
    labelCentered(done ? "Made" : ready ? "Craft" : "Locked",x+621,top+8,16,ready ? accent : cream);
    label(done || ready ? recipe.benefit : std::string_view(h.craft.problems[i]),x+74,top+32,15,ready || done ? cream : glm::vec4(1.f,.83f,.58f,1));
  }
  if(!h.notice.empty()) labelCentered(h.notice,cx,y+491,std::min(17.f,17.f*620.f/readableWidth(h.notice,17)),accent);
  labelCentered("Click a recipe or press 1-4.    E or Esc: Back to game",cx,y+526,17,cream);
}
void Ui::heldTool(const HudState& h) {
  // Small, shaded cuboids form the hand and tool. Project them into the HUD so
  // nearby world blocks cannot hide the player's hand. Faces draw back to front.
  struct Face { std::array<glm::vec2,4> points; glm::vec4 color; float depth; };
  std::vector<Face> faces; faces.reserve(60);
  float swing=std::sin(std::clamp(h.toolSwing,0.f,1.f)*3.14159265f);
  float scale=58.f*std::clamp(float(h.height)/600.f,1.f,1.35f);
  glm::vec2 origin{float(h.width)-105.f-36.f*swing,float(h.height)-224.f+18.f*swing};
  if(h.selectedItem()==Item::WateringCan) origin.y-=52.f*swing;
  auto rotation=glm::rotate(glm::mat4(1),.24f+.86f*swing,glm::vec3(0,0,1));
  rotation=glm::rotate(rotation,-.18f,glm::vec3(1,0,0));
  rotation=glm::rotate(rotation,-.55f+.18f*swing,glm::vec3(0,1,0));
  auto box=[&](glm::vec3 lo,glm::vec3 hi,glm::vec3 color) {
    std::array<glm::vec3,8> corners;
    for(int i=0;i<8;++i) {
      glm::vec3 p{i&1 ? hi.x : lo.x,i&2 ? hi.y : lo.y,i&4 ? hi.z : lo.z};
      corners[i]=glm::vec3(rotation*glm::vec4(p,1));
    }
    constexpr std::array<std::array<int,4>,6> sides{{{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}}};
    for(const auto& side : sides) {
      auto normal=glm::normalize(glm::cross(corners[side[1]]-corners[side[0]],corners[side[2]]-corners[side[0]]));
      if(normal.z<=0) continue;
      float light=.68f+.32f*std::max(0.f,glm::dot(normal,glm::normalize(glm::vec3(-.5f,.8f,1.f))));
      Face face{}; face.color=glm::vec4(color*light,1);
      for(int i=0;i<4;++i) {
        auto p=corners[side[i]];
        face.points[i]=origin+scale*glm::vec2(p.x,-p.y); face.depth+=p.z*.25f;
      }
      faces.push_back(face);
    }
  };
  const glm::vec3 shaft{.52f,.29f,.13f},grain{.70f,.44f,.22f},skin{.83f,.62f,.43f},sleeve{.28f,.48f,.36f};
  auto block=h.selectedBlock();
  if(h.tools.removesBlocks() && h.selectedItem()==Item::Empty) {
    box({-.10f,-.10f,-.10f},{.10f,1.90f,.10f},shaft);
    box({-.74f,1.55f,-.25f},{.74f,2.13f,.25f},{.62f,.69f,.73f});
    box({-.81f,1.52f,-.28f},{-.60f,2.16f,.28f},{.91f,.42f,.28f});
    box({.60f,1.52f,-.28f},{.81f,2.16f,.28f},{.75f,.81f,.83f});
  } else if(h.selectedItem()==Item::Hoe) {
    box({-.08f,-.10f,-.08f},{.08f,1.90f,.08f},shaft);
    box({-.10f,1.80f,-.12f},{.22f,1.98f,.12f},{.60f,.68f,.69f});
    box({-.78f,1.65f,-.17f},{-.06f,1.84f,.17f},{.64f,.72f,.73f});
    box({-.85f,1.44f,-.17f},{-.70f,1.84f,.17f},{.45f,.54f,.54f});
  } else if(h.selectedItem()==Item::Compost) {
    box({-.48f,.22f,-.26f},{.48f,1.35f,.26f},{.65f,.47f,.26f});
    box({-.39f,1.30f,-.22f},{.39f,1.47f,.22f},{.85f,.70f,.44f});
    box({-.30f,.61f,.261f},{.30f,1.05f,.28f},{.28f,.51f,.22f});
    box({-.18f,.81f,.281f},{.18f,.91f,.30f},{.76f,.86f,.47f});
  } else if(h.selectedItem()==Item::Greenhouse) {
    box({-.66f,.28f,-.38f},{.66f,1.32f,.38f},{.51f,.76f,.76f});
    for(float x : {-.66f,-.04f,.59f}) box({x,.28f,.39f},{x+.07f,1.34f,.44f},grain);
    for(float y : {.28f,.83f,1.30f}) box({-.66f,y,-.40f},{.66f,y+.07f,.45f},{.85f,.78f,.58f});
  } else if(h.selectedItem()==Item::WateringCan) {
    const glm::vec3 blue{.24f,.63f,.77f},rim{.51f,.83f,.89f};
    // A hollow handle, broad body, and a long spout make the held tool recognizable.
    box({.29f,.20f,-.11f},{.72f,.34f,.11f},rim);
    box({.62f,.25f,-.11f},{.78f,1.11f,.11f},rim);
    box({.29f,1.00f,-.11f},{.72f,1.15f,.11f},rim);
    box({-.79f,.18f,-.35f},{.36f,1.04f,.35f},blue);
    box({-.83f,.99f,-.39f},{.40f,1.10f,.39f},rim);
    box({-.68f,1.10f,-.22f},{.20f,1.115f,.22f},{.14f,.39f,.49f});
    box({-.66f,.32f,.351f},{-.57f,.85f,.36f},rim);
    box({-1.09f,.59f,-.13f},{-.72f,.84f,.13f},blue);
    box({-1.32f,.73f,-.12f},{-1.03f,1.03f,.12f},blue);
    box({-1.53f,.96f,-.20f},{-1.27f,1.13f,.20f},rim);
    for(float z : {-.11f,0.f,.11f}) box({-1.54f,1.005f,z-.018f},{-1.531f,1.04f,z+.018f},{.15f,.35f,.41f});
  } else if(h.tool()!=Tool::Hands) {
    box({-.115f,-.12f,-.105f},{.115f,2.08f,.105f},shaft);
    box({-.095f,.56f,.106f},{-.04f,1.44f,.116f},grain);
    box({.04f,.81f,.106f},{.08f,1.61f,.116f},grain*.82f);
    box({-.13f,-.16f,-.12f},{.13f,.03f,.12f},grain);
    if(h.tool()==Tool::Axe) {
      box({-.32f,1.64f,-.17f},{.24f,2.01f,.17f},{.62f,.37f,.16f});
      box({-1.04f,1.43f,-.19f},{-.29f,2.13f,.19f},{.75f,.49f,.23f});
      box({-1.19f,1.36f,-.13f},{-1.04f,2.20f,.13f},{.90f,.68f,.38f});
      box({-.99f,1.57f,.191f},{-.35f,1.63f,.203f},grain*.83f);
      box({-.86f,1.91f,.191f},{-.34f,1.96f,.203f},grain);
      box({-.15f,1.62f,-.18f},{.15f,1.73f,.18f},{.34f,.30f,.21f});
    } else {
      const glm::vec3 stone{.65f,.70f,.70f};
      box({-.75f,1.84f,-.16f},{.75f,2.14f,.16f},stone);
      box({-.99f,1.64f,-.14f},{-.72f,2.08f,.14f},stone*.91f);
      box({-1.13f,1.39f,-.10f},{-.96f,1.80f,.10f},stone*.81f);
      box({.72f,1.64f,-.14f},{.99f,2.08f,.14f},stone*.91f);
      box({.96f,1.39f,-.10f},{1.13f,1.80f,.10f},stone*.81f);
      box({-.22f,1.83f,.161f},{.22f,2.15f,.20f},{.44f,.47f,.45f});
      box({-.19f,1.94f,.201f},{.19f,2.05f,.21f},{.73f,.54f,.32f});
    }
  } else if(block!=Block::Air) {
    auto color=blockColor(block);
    if(block==Block::Sprinkler) {
      box({-.43f,.22f,-.35f},{.43f,.36f,.35f},{.56f,.62f,.60f});
      box({-.08f,.35f,-.08f},{.08f,1.25f,.08f},{.35f,.67f,.73f});
      box({-.64f,1.16f,-.10f},{.64f,1.35f,.10f},{.47f,.82f,.86f});
    } else if(isWheat(block)) {
      for(float x : {-.27f,0.f,.27f}) {
        box({x-.035f,.05f,-.035f},{x+.035f,1.4f,.035f},{.43f,.64f,.23f});
        box({x-.10f,1.02f,-.10f},{x+.10f,1.60f,.10f},{.91f,.73f,.30f});
      }
    } else if(isCrop(block)) {
      box({-.41f,.23f,-.12f},{.41f,1.43f,.12f},{.94f,.87f,.64f});
      box({-.42f,.23f,-.13f},{.42f,.43f,.13f},{.36f,.61f,.25f});
      box({-.42f,1.29f,-.13f},{.42f,1.43f,.13f},{.36f,.61f,.25f});
      box({-.25f,.66f,.125f},{.25f,1.07f,.18f},color);
      box({-.06f,1.06f,.13f},{.06f,1.22f,.18f},{.33f,.59f,.18f});
      if(cropKind(block)==CropKind::Carrot) box({-.10f,.49f,.13f},{.10f,.66f,.18f},color);
      if(cropKind(block)==CropKind::Pumpkin) for(float x : {-.14f,0.f,.14f}) box({x-.015f,.69f,.181f},{x+.015f,1.04f,.19f},color*.7f);
      if(cropKind(block)==CropKind::Strawberry) for(float x : {-.14f,0.f,.14f}) box({x,.79f,.181f},{x+.026f,.83f,.19f},{1.f,.87f,.4f});
    } else if(block==Block::Torch) {
      box({-.10f,.08f,-.10f},{.10f,1.20f,.10f},shaft);
      box({-.17f,1.20f,-.17f},{.17f,1.52f,.17f},{1.f,.69f,.16f});
    } else if(block==Block::Fence || isGate(block)) {
      box({-.52f,.20f,-.12f},{-.34f,1.45f,.12f},grain);
      box({.34f,.20f,-.12f},{.52f,1.45f,.12f},grain);
      for(float y : {.50f,1.03f}) box({-.52f,y,-.09f},{.52f,y+.15f,.09f},grain);
    } else if(isDoor(block)) {
      box({-.45f,.24f,-.10f},{.45f,1.86f,.10f},color);
      box({.24f,.86f,.11f},{.35f,.98f,.15f},{.91f,.72f,.28f});
    } else if(isBed(block)) {
      box({-.64f,.24f,-.40f},{.64f,.63f,.40f},grain);
      box({-.64f,.62f,-.40f},{.64f,.83f,.40f},{.73f,.29f,.24f});
      box({.27f,.83f,-.37f},{.60f,.98f,.37f},{.95f,.92f,.82f});
    } else {
      box({-.55f,.24f,-.45f},{.55f,block==Block::StoneSlab ? .79f : 1.34f,.45f},block==Block::Grass ? blockColor(Block::Dirt) : color);
      if(block==Block::Grass) box({-.56f,1.16f,-.46f},{.56f,1.36f,.46f},color);
      if(block==Block::Wood || block==Block::Planks || block==Block::Workbench) {
        for(float y : {.46f,.73f,1.f}) box({-.54f,y,.451f},{.54f,y+.035f,.46f},color*.62f);
      }
    }
  }
  // A fist wraps around the handle. The forearm enters from the screen edge,
  // behind the mode buttons and other controls.
  box({-.25f,-.71f,-.24f},{.28f,-.16f,.26f},sleeve);
  box({-.28f,-.18f,-.25f},{.30f,.29f,.28f},skin);
  box({-.31f,.05f,.12f},{-.11f,.36f,.34f},skin*1.06f);
  box({-.13f,.09f,.281f},{.27f,.13f,.29f},skin*.8f);
  auto wrist=glm::vec3(rotation*glm::vec4(0,-.52f,0,1));
  auto armTop=origin+scale*glm::vec2(wrist.x,-wrist.y);
  glm::vec2 armEnd{float(h.width)-38.f,float(h.height)-1.f};
  quad(armTop+glm::vec2(-13,-3),armTop+glm::vec2(13,-3),armEnd+glm::vec2(32,0),armEnd-glm::vec2(36,0),glm::vec4(sleeve*.85f,1));
  quad(armTop+glm::vec2(13,-3),armTop+glm::vec2(21,4),armEnd+glm::vec2(37,0),armEnd+glm::vec2(32,0),glm::vec4(sleeve*.6f,1));
  std::ranges::sort(faces,{},&Face::depth);
  for(const auto& face : faces) quad(face.points[0],face.points[1],face.points[2],face.points[3],face.color);


}
void Ui::build(const HudState& h) {
  vertices.clear();
  if(h.hidden) return;
  if(h.menuOpen()) {
    if(h.menu==Menu::Crafting) workshop(h); else if(h.menu==Menu::Farm) farmPage(h); else inventory(h);
    menuTabs(h); return;
  }
  if(!h.paused && !h.sleeping) {
    for(const auto& label : h.animalLabels) {
      float width=float(label.name.size())*7.2f+16;
      rectangle(label.position.x-width*.5f,label.position.y-4,width,20,panel);
      centered(label.name,label.position.x,label.position.y+1,1.2f,label.baby ? accent : cream);
    }
    if(!h.riding) heldTool(h);
  }
  float w=float(h.width),height=float(h.height),cx=w*.5f;
  char status[64];
  int minutes=int(h.clock.phase*24*60);
  std::snprintf(status,sizeof(status),"DAY %u / %02d:%02d",h.clock.day,minutes/60,minutes%60);
  rectangle(w-228,24,204,86,panel);
  text(status,w-214,38,1.5f,cream);
  text(h.farm.garden.raining() ? "RAIN / WATERING CROPS" : h.clock.period(),w-214,61,h.farm.garden.raining() ? .8f : 1.2f,accent);
  std::snprintf(status,sizeof(status),"%.0F FPS",h.fps);
  text(status,w-100,62,1.f,muted);
  text(!h.audioAvailable ? "SOUND UNAVAILABLE" : h.muted ? "M / SOUND OFF" : "M / SOUND ON",w-214,85,1.1f,muted);

  if(h.guide.enabled && h.help && !h.paused) {
    float panelHeight=h.guide.landmark ? 120.f : h.guide.farm ? (h.farmGarden ? 220.f : 168.f) : h.guide.stage==4 ? 279.f : 144.f;
    rectangle(24,106,350,panelHeight,panel);
    rectangle(24,106,350,2,accent);
    if(h.guide.landmark) {
      label(h.guide.title,39,120,20,accent);
      for(int i=0;i<3;++i) label(h.guide.lines[i],39,153+i*21,16,i==0 ? cream : muted);
    } else {
      text(h.guide.title,39,124,1.5f,accent);
      for(int i=0;i<3;++i) text(h.guide.lines[i],39,153+i*19,std::min(1.3f,320.f/std::max(1.f,float(h.guide.lines[i].size())*6)),i==0 ? cream : muted);
    }
    if(h.guide.farm) {
      if(h.farmGarden) {
        text("YOUR HARVEST BASKET",39,220,1.2f,accent);
        constexpr std::array seeds{Item::Wheat,Item::Carrot,Item::Strawberry,Item::Pumpkin};
        for(int i=0;i<4;++i) {
          itemIcon(seeds[i],58+i*80,237,10);
          text(std::to_string(h.farm.harvest[i]),79+i*80,252,1.1f,cream);
        }
        text("COINS "+std::to_string(h.farm.garden.coins)+" / P THEN SHOP",39,286,1.15f,accent);
        for(int i=0;i<6;++i) rectangle(39+i*54,312,44,4,i<h.guide.stage ? accent : glm::vec4(.25f,.33f,.28f,1));
      } else {
        std::snprintf(status,sizeof(status),"WHEAT %d     EGGS %d",h.wheat,h.eggs);
        text(status,39,220,1.4f,accent);
        for(int i=0;i<8;++i) rectangle(39+i*40,251,32,4,i<h.guide.stage ? accent : glm::vec4(.25f,.33f,.28f,1));
      }
    } else if(h.guide.stage==4) {
      constexpr std::array labels{"FLOOR + WALLS","WINDOWS","ROOF","DOOR","TORCH","BED"};
      for(int i=0;i<int(labels.size());++i) {
        const auto& count=h.guide.cabin.parts[i];
        float row=219.f+i*21;
        bool done=count.done==count.total;
        rectangle(39,row+1,7,7,done ? accent : glm::vec4(.27f,.34f,.29f,1));
        text(labels[i],55,row,1.2f,done ? accent : muted);
        char progress[24]; std::snprintf(progress,sizeof(progress),"%d / %d",count.done,count.total);
        text(progress,285,row,1.2f,cream);
      }
      rectangle(39,358,320,5,{.20f,.28f,.23f,1});
      float fraction=float(h.guide.cabin.done)/float(std::max(h.guide.cabin.total,1));
      rectangle(39,358,320*fraction,5,accent);
    } else if(!h.guide.landmark) {
      for(int i=0;i<7;++i) rectangle(39+i*46,224,36,4,i<h.guide.stage ? accent : glm::vec4(.25f,.33f,.28f,1));
    }
    if(h.waypoint) {
      float x=std::clamp(h.waypoint->x,100.f,w-100.f),y=std::clamp(h.waypoint->y-34.f,94.f,height-185.f);
      if(x<390 && y<400) { x=410; y=std::max(y,410.f); }
      char label[80]; std::snprintf(label,sizeof(label),"%s / %.0FM",h.guide.destinationName.c_str(),h.waypointDistance);
      float textWidth=float(std::char_traits<char>::length(label))*7.2f+18;
      x=std::clamp(x,textWidth*.5f+8,w-textWidth*.5f-8);
      quad({x,y-7},{x+7,y},{x,y+7},{x-7,y},accent);
      rectangle(x-textWidth*.5f,y+14,textWidth,22,panel);
      centered(label,x,y+21,1.2f,cream);
    } else if(h.guide.destination && !(h.guide.farm && h.farmGarden) && 106+panelHeight+12<height-218)
      text(h.guide.farm ? (h.farmGarden ? "P / GARDEN / VISIT GARDEN" : "TURN TOWARD THE CHICKEN PEN") : "TURN TOWARD THE PATH / R TO GO HOME",39,106+panelHeight+12,1.2f,cream);
  }

  if(!h.paused) {
    rectangle(cx-7,height*.5f-1,14,2,{0,0,0,.4f}); rectangle(cx-1,height*.5f-7,2,14,{0,0,0,.4f});
    rectangle(cx-6,height*.5f-.5f,12,1,cream); rectangle(cx-.5f,height*.5f-6,1,12,cream);
    if(h.breaking) {
      rectangle(cx-55,height*.5f+13,110,5,{.05f,.08f,.06f,.85f});
      rectangle(cx-55,height*.5f+13,110*std::clamp(h.breakProgress,0.f,1.f),5,accent);
    }
  }
  std::string selected=std::string(toolName(h.selectedItem()));
  if(h.riding) {
    rectangle(cx-270,height-140,540,115,panel);
    labelCentered(h.driving ? "Driving your farm car" : "Riding your horse",cx,height-131,23,accent);
    labelCentered("W / Up: forward    S / Down: reverse",cx,height-96,18,cream);
    labelCentered("A D / Left Right: steer    Space: brake",cx,height-72,18,cream);
    labelCentered(h.driving ? "V: get out    C: move to clear ground" : "V: get off",cx,height-47,18,accent);
  } else {
  labelCentered(h.tools.mode==PlayMode::Build ? selected+" / E: more materials" : "E  Choose mode / tools",cx,height-159,18,cream);
  modeButtons(h,cx-228,height-132,456,40);
  if(h.tools.mode==PlayMode::Build) {
    auto materials=modeTools(PlayMode::Build);
    for(int i=0;i<9;++i) {
      float x=cx-249+i*56,y=height-84;
      bool active=materials[i]==h.selectedItem();
      rectangle(x,y,50,50,active ? buildColor : panel);
      itemIcon(materials[i],x+28,y+12,12);
      label(std::to_string(i+1),x+4,y+2,14,active ? panel : cream);
    }
    rectangle(cx-249,height-30,498,25,panel);
    labelCentered("Left-click: place    Right-click: remove",cx,height-29,16,cream);
  } else {
    rectangle(cx-228,height-84,456,59,panel);
    labelCentered(selected,cx,height-80,20,modeColor(h.tools.mode));
    labelCentered(h.tools.mode==PlayMode::Remove ? "Click: remove one block     V: interact"
      : "Click: use tool / harvest     P: farm shop",cx,height-50,16,cream);
  }
  }
  if(!h.interaction.empty() && !h.paused && !h.riding) {
    float size=std::min(17.f,17.f*(w-48.f)/std::max(1.f,readableWidth(h.interaction,17)));
    float iw=readableWidth(h.interaction,size)+24;
    float top=height*.5f+23;
    if(h.help && h.guide.enabled && h.guide.farm && h.farmGarden) top=std::max(top,334.f);
    rectangle(cx-iw*.5f,top,iw,28,panel);
    labelCentered(h.interaction,cx,top+3,size,cream);
  }
  if(h.help && !h.paused) {
    label(h.riding ? "Mouse: look around    R: return home" : h.flying ? "Flying: Space up / Shift down / Tab to land" : "WASD / arrows: move     Space: jump",24,height-217,15,cream);
    label("T: city    U: penthouse    J: coast    C: car    R: home    H: help",24,height-194,14,muted);
  }
  if(!h.notice.empty()) {
    float size=std::min(18.f,18.f*(w-80)/std::max(1.f,readableWidth(h.notice,18)));
    float tw=readableWidth(h.notice,size)+32;
    rectangle(cx-tw*.5f,30,tw,34,panel);
    labelCentered(h.notice,cx,36,size,accent);
  }
  if(h.paused) {
    rectangle(0,0,w,height,{.025f,.055f,.05f,.43f});
    float x=cx-260,top=height*.5f-230;
    rectangle(x,top,520,454,{.07f,.115f,.105f,.96f});
    rectangle(x,top,520,3,accent);
    labelCentered("Your farm, your world",cx,top+27,30,cream);
    labelCentered("Choose Farm, Build, or Remove with E.",cx,top+76,19,muted);
    rectangle(x+36,top+113,448,1,{.28f,.36f,.29f,1});
    auto row=[&](std::string_view key,std::string_view action,float dy) {
      label(key,x+38,top+dy,17,accent); label(action,x+216,top+dy,17,cream);
    };
    row("WASD / arrows","Move",136); row("Mouse","Look around",166);
    row("Left-click",h.tools.mode==PlayMode::Remove ? "Remove a block" : h.tools.mode==PlayMode::Build ? "Place / use (also V)" : "Use tool / harvest",196);
    row("E","Choose mode / tools",226); row("P","Garden, shop, animals",256);
    row(h.tools.mode==PlayMode::Build ? "Right-click" : "V / right-click",h.tools.mode==PlayMode::Build ? "Remove a block" : "Place / interact",286);
    rectangle(x+36,top+326,448,44,accent);
    labelCentered("Click or press Esc to play",cx,top+332,22,panel);
    labelCentered("Space: jump     Tab: fly     R: home     M: sound",cx,top+391,15,muted);
    labelCentered("J: coast    T: city    C: car    K: castle",cx,top+421,17,accent);
  }
  if(h.sleeping) {
    rectangle(0,0,w,height,{.018f,.025f,.055f,h.sleepFade});
    if(h.sleepFade>.7f) {
      auto ink=cream; ink.a=(h.sleepFade-.7f)/.3f;
      centered(h.waking ? "A NEW DAY" : "REST WELL",cx,height*.5f-15,3.f,ink);
      centered(h.waking ? "MORNING LIGHT FINDS YOUR HOME" : "THE MEADOW GROWS QUIET",cx,height*.5f+26,1.3f,ink);
    }
  }
}
} // namespace bw

#include "ui.hpp"
#include "estate.hpp"
#include "ui_font.hpp"
#include <algorithm>

namespace bw {
namespace {
constexpr glm::vec4 ink{.96f,.96f,.91f,1},dim{.70f,.77f,.78f,1},blue{.48f,.80f,.95f,1},card{.11f,.18f,.21f,1},gold{.97f,.78f,.42f,1};
constexpr int propertyRows=8;
}
CityClick Ui::cityActionAt(const HudState& h,float px,float py) {
  float x=h.width*.5f-340,y=h.height*.5f-276;
  auto in=[&](float bx,float by,float w,float height){return px>=x+bx && px<x+bx+w && py>=y+by && py<y+by+height;};
  for(int i=0;i<7;++i)if(in(24+i*91,92,86,36))return {CityAction::Page,i};
  if(!h.cityAvailable)return {};
  if(h.cityPage==CityPage::Home) {
    for(int i=0;i<3;++i)if(in(24+i*214,142,204,62))return {CityAction::SelectHome,i};
    if(in(490,213,166,25))return {CityAction::MakeHome,-1};
    if(in(24,244,200,36))return {CityAction::MakeHome,h.homeSelected};
    if(in(240,244,200,36))return {CityAction::Place,h.homeSelected};
    if(in(456,244,200,36))return {CityAction::VisitCabin};
    for(int i=0;i<residentCount;++i)if(in(24+(i%4)*160,315+(i/4)*38,152,32))return {CityAction::Resident,i};
    if(in(24,404,308,32))return {CityAction::MoveHousehold,h.residentSelected};
    if(in(348,404,308,32))return {CityAction::ReturnApartment,h.residentSelected};
    if(in(416,448,240,34))return {CityAction::ParkHome,h.homeSelected};
  } else if(h.cityPage==CityPage::Business) {
    for(int i=0;i<2;++i)if(in(24+i*214,142,204,34))return {CityAction::SelectBusiness,i};
    if(in(24,298,200,36))return {CityAction::BuyRack,h.businessSelected};
    if(in(240,298,200,36))return {CityAction::BuyPower,h.businessSelected};
    if(in(456,298,200,36))return {CityAction::BuyCooling,h.businessSelected};
    for(int i=0;i<4;++i)if(in(24,347+i*32,632,29))return {CityAction::SignContract,i};
    if(in(24,493,168,30))return {CityAction::Servers,h.businessSelected};
  } else if(h.cityPage==CityPage::Bank) {
    for(int i=0;i<3;++i) {
      int amount=i==0 ? 10 : i==1 ? 100 : -1;
      if(in(36+i*96,206,88,34))return {CityAction::Deposit,amount};
      if(in(364+i*96,206,88,34))return {CityAction::Withdraw,amount};
    }
    if(in(24,474,200,36))return {CityAction::Bank};
    if(in(240,474,200,36))return {CityAction::Downtown};
    if(in(456,474,200,36))return {CityAction::Harbor};
  } else if(h.cityPage==CityPage::Garage) {
    for(int i=0;i<10;++i)if(in(24+(i%2)*328,178+(i/2)*54,304,48))return {CityAction::Drive,h.garagePage*10+i};
    if(in(24,474,130,36))return {CityAction::Previous};
    if(in(174,474,332,36))return {CityAction::Garage};
    if(in(526,474,130,36))return {CityAction::Next};
  } else if(h.cityPage==CityPage::Residents) {
    for(int i=0;i<residentCount;++i)if(in(24,151+i*39,212,35))return {CityAction::Resident,i};
    if(in(260,392,396,38))return {CityAction::VisitResident,h.residentSelected};
    if(h.nearResident && in(260,442,188,38))return {CityAction::Chat,h.residentSelected};
    if(h.nearResident && in(468,442,188,38))return {h.cityLife.residents[h.residentSelected].dating ? CityAction::SpendNight : CityAction::Date,h.residentSelected};
    auto& r=h.cityLife.residents[h.residentSelected];
    if(h.nearResident && r.dating && in(260,486,cityChildCount(r) ? 188 : 396,30))return {CityAction::StartFamily,h.residentSelected};
    if(h.nearResident && cityChildCount(r) && in(468,486,188,30))return {CityAction::FeedFamily,h.residentSelected};
  } else if(h.cityPage==CityPage::Places) {
    for(int i=0;i<6;++i)if(in(24+(i%2)*328,171+(i/2)*110,304,94))return {CityAction::Place,i};
  } else {
    for(int i=0;i<propertyRows;++i)if(in(24,195+i*30,632,28)) {
      int index=h.propertyPage*propertyRows+i;
      if(index<int(metroBuildings().size()))return {CityAction::Property,index};
    }
    if(in(24,448,304,32))return {CityAction::Servers,0};
    if(in(352,448,304,32))return {CityAction::Servers,1};
    if(in(24,488,130,28))return {CityAction::Previous};
    if(in(526,488,130,28))return {CityAction::Next};
  }
  return {};
}
void Ui::cityPage(const HudState& h) {
  float x=h.width*.5f-340,y=h.height*.5f-276;
  rectangle(0,0,h.width,h.height,{.025f,.045f,.07f,.68f});
  rectangle(x,y,680,552,{.055f,.095f,.13f,.99f});rectangle(x,y,680,3,blue);
  label("Your city",x+24,y+15,30,ink);
  label("Meet your neighbors, visit your properties, and grow your family.",x+24,y+62,17,dim);
  auto button=[&](float bx,float by,float w,float height,std::string_view title,bool enabled=true,bool selected=false) {
    rectangle(x+bx,y+by,w,height,selected ? blue : enabled ? glm::vec4(.18f,.30f,.36f,1) : card);
    labelCentered(title,x+bx+w*.5f,y+by+(height-22)*.5f,17,selected ? card : enabled ? ink : dim);
  };
  constexpr std::array tabs{"Bank","Garage","Residents","Property","Places","Home","Business"};
  for(int i=0;i<7;++i)button(24+i*91,92,86,36,tabs[i],true,int(h.cityPage)==i);
  auto wrap=[&](std::string_view value,float bx,float by,float width,float size,glm::vec4 color) {
    std::string line,word;
    auto emit=[&]{label(line,x+bx,y+by,size,color);by+=size+6;line.clear();};
    for(std::size_t i=0;i<=value.size();++i) {
      if(i<value.size() && value[i]!=' ') {word+=value[i];continue;}
      if(!line.empty() && readableWidth(line+" "+word,size)>width)emit();
      if(!line.empty())line+=' ';line+=word;word.clear();
    }
    if(!line.empty())emit();
  };
  if(!h.cityAvailable) {wrap("The city needs a clear parcel. Your existing construction has been preserved.",24,160,620,21,ink);return;}
  if(h.cityPage==CityPage::Places) {
    label("Waterfront estates & airport",x+24,y+141,20,ink);
    for(int i=0;i<6;++i) {
      float bx=24+(i%2)*328,by=171+(i/2)*110;
      button(bx,by,304,94," ");
      label(estatePlaces()[i].name,x+bx+12,y+by+8,21,ink);
      wrap(estatePlaces()[i].description,bx+12,by+39,280,15,dim);
    }
    label("Click a destination. All places are connected by road.",x+24,y+508,17,blue);
    labelCentered("L / Esc: return to world",x+340,y+532,14,dim);return;
  }
  if(h.cityPage==CityPage::Home) {
    for(int i=0;i<3;++i) {
      float bx=24+i*214;button(bx,142,204,62,"",true,h.homeSelected==i);
      wrap(estatePlaces()[i].name,bx+12,151,180,18,h.homeSelected==i ? card : ink);
    }
    label("R returns to: "+(h.cityLife.home<0 ? std::string("Farm cabin") : std::string(estatePlaces()[h.cityLife.home].name)),x+24,y+215,18,gold);
    button(490,213,166,25,"Use cabin as home");
    button(24,244,200,36,h.cityLife.home==h.homeSelected ? "This is your home" : "Make my home");
    button(240,244,200,36,"Visit this mansion");button(456,244,200,36,"Visit farm cabin");
    label("Choose a partner to move with their children",x+24,y+289,18,ink);
    for(int i=0;i<residentCount;++i)button(24+(i%4)*160,315+(i/4)*38,152,32,cityResidents()[i].name,true,h.residentSelected==i);
    const auto& r=h.cityLife.residents[h.residentSelected];
    std::string home=r.home<0 ? "Original apartment" : std::string(estatePlaces()[r.home].name);
    int children=cityChildCount(r);
    label(home+(r.dating ? " / "+std::to_string(children)+(children==1 ? " child" : " children") : " / Meet and date in Residents first"),x+24,y+384,15,dim);
    button(24,404,308,32,"Move household here",r.dating && r.home!=h.homeSelected);
    button(348,404,308,32,"Return to original apartment",r.home>=0);
    int parked=h.cityLife.homeCars[h.homeSelected];
    label(parked<0 ? "Parking bay: empty" : std::string(garageCars()[parked].name),x+24,y+445,18,ink);
    label("Current car: "+std::string(garageCars()[h.cityLife.activeCar].name),x+24,y+470,14,dim);
    button(416,448,240,34,"Park current car here");
    wrap(h.cityMessage.empty() ? "B: furnish rooms. E: choose beds, sofas, tables and plants. Changes save with your world." : h.cityMessage,24,494,632,15,blue);
    return;
  }
  if(h.cityPage==CityPage::Business) {
    for(int i=0;i<2;++i)button(24+i*214,142,204,34,i==0 ? "West data center" : "East data center",true,h.businessSelected==i);
    label("Bank: "+std::to_string(h.cityLife.bank),x+460,y+151,17,gold);
    auto s=h.cityLife.dataCenters[h.businessSelected];auto r=dataCenterReport(s);
    constexpr std::array titles{"Revenue / day","Operating costs / day","Profit to bank / day"};
    std::array amounts{r.revenue,r.electricity+r.cooling,r.profit};
    for(int i=0;i<3;++i){float bx=24+i*214;rectangle(x+bx,y+190,204,64,card);label(titles[i],x+bx+10,y+198,16,dim);label(std::to_string(amounts[i])+" coins",x+bx+10,y+219,25,i==2 ? gold : ink);}
    label("Racks: "+std::to_string(r.used)+" used / "+std::to_string(s.racks)+" installed   Power: "+std::to_string(s.power*4)+" racks   Cooling: "+std::to_string(s.cooling*4)+" racks",x+24,y+264,17,ink);
    label("Electricity "+std::to_string(r.electricity)+" + cooling "+std::to_string(r.cooling)+" coins / day",x+24,y+282,14,dim);
    for(int i=0;i<3;++i) {
      auto kind=ServerUpgrade(i);int price=serverUpgradePrice(s,kind),level=i==0 ? s.racks : i==1 ? s.power : s.cooling;
      bool max=level>=(i==0 ? 16 : 4);
      std::string title=i==0 ? "Add rack" : i==1 ? "Power" : "Cooling";
      button(24+i*216,298,200,36,title+(max ? " / maximum" : " / "+std::to_string(price)),!max && h.cityLife.bank>=price && (i!=0 || s.racks<r.capacity));
    }
    for(int i=0;i<4;++i) {
      auto contract=serverContracts()[i];bool signedUp=s.contracts&(1<<i),available=s.racks-r.used>=contract.racks;float by=347+i*32;
      rectangle(x+24,y+by,632,29,signedUp ? glm::vec4(.13f,.26f,.24f,1) : card);
      label(std::string(contract.name)+" / "+std::to_string(contract.racks)+" racks",x+34,y+by+3,16,ink);
      label(std::to_string(contract.revenue)+" / day",x+345,y+by+3,16,gold);
      label(signedUp ? "Active" : available ? "Sign contract" : "Need racks",x+529,y+by+3,15,signedUp || available ? blue : dim);
    }
    button(24,493,168,30,"Visit server room");
    wrap(h.cityMessage.empty() ? "Add racks, then sign a customer. Daily profit pays automatically, including after sleep." : h.cityMessage,210,492,446,14,blue);
    return;
  }
  if(h.cityPage==CityPage::Bank) {
    rectangle(x+24,y+146,304,106,card);rectangle(x+352,y+146,304,106,card);
    label("Wallet",x+36,y+151,16,dim);label("Bank account",x+364,y+151,16,dim);
    label(std::to_string(h.farm.garden.coins)+" coins",x+36,y+172,25,ink);
    label(std::to_string(h.cityLife.bank)+" coins",x+364,y+172,25,gold);
    constexpr std::array deposit{"Put in 10","Put in 100","Put in all"},withdraw{"Take 10","Take 100","Take all"};
    for(int i=0;i<3;++i){button(36+i*96,206,88,34,deposit[i]);button(364+i*96,206,88,34,withdraw[i]);}
    label("Daily income: "+std::to_string(h.rentPerDay)+" rent + "+std::to_string(h.serverPerDay)+" servers",x+24,y+261,18,blue);
    label("Paid automatically at the start of each game day, including after sleep.",x+24,y+287,15,dim);
    label("Recent activity",x+24,y+316,18,ink);
    if(h.cityLife.statement.empty())label("No transactions yet. Deposit coins or wait for the next day.",x+24,y+352,16,dim);
    int row=0;
    for(auto it=h.cityLife.statement.rbegin();it!=h.cityLife.statement.rend() && row<4;++it,++row) {
      constexpr std::array kinds{"Deposit","Withdrawal","Building rent","Server profit","Business upgrade"};
      float by=347+row*26;
      label("Day "+std::to_string(it->day),x+24,y+by,16,dim);
      label(kinds[int(it->kind)],x+130,y+by,16,ink);
      label((it->kind==BankKind::Withdrawal || it->kind==BankKind::BusinessPurchase ? "-" : "+")+std::to_string(it->amount)+" coins",x+460,y+by,16,gold);
    }
    if(!h.cityMessage.empty())label(h.cityMessage,x+24,y+450,15,blue);
    button(24,474,200,36,"Visit bank");button(240,474,200,36,"Visit downtown");button(456,474,200,36,"Visit waterfront");
  } else if(h.cityPage==CityPage::Garage) {
    label("20 owned cars  /  Click a car to drive it out",x+24,y+143,20,ink);
    for(int i=0;i<10;++i) {
      int index=h.garagePage*10+i;const auto& car=garageCars()[index];float bx=24+(i%2)*328,by=178+(i/2)*54;
      rectangle(x+bx,y+by,304,48,index==h.cityLife.activeCar ? glm::vec4(.19f,.32f,.35f,1) : card);
      rectangle(x+bx,y+by,5,48,glm::vec4(car.color,1));
      label(car.name,x+bx+14,y+by+3,18,ink);
      label(car.style,x+bx+14,y+by+27,14,dim);
      label(index==h.cityLife.activeCar ? "Current" : "Drive",x+bx+242,y+by+28,14,blue);
    }
    button(24,474,130,36,"Previous",h.garagePage>0);
    button(174,474,332,36,"Walk into your garage");
    button(526,474,130,36,"Next",h.garagePage<1);
    labelCentered("Page "+std::to_string(h.garagePage+1)+" / 2   -   L: close   -   Shift: boost",x+340,y+520,15,dim);
    return;
  } else if(h.cityPage==CityPage::Residents) {
    for(int i=0;i<residentCount;++i)button(24,151+i*39,212,35,std::string(cityResidents()[i].name)+(h.cityLife.residents[i].dating ? "  /  Girlfriend" : "  /  18"),true,i==h.residentSelected);
    int i=h.residentSelected;auto person=cityResidents()[i];auto state=h.cityLife.residents[i];
    label(std::string(person.name)+", 18",x+260,y+150,28,ink);
    label(state.home<0 ? std::string(metroBuildings()[i].name)+" / Floor "+std::to_string(1+i%4) : std::string(estatePlaces()[state.home].name)+" / Upstairs",x+260,y+187,17,blue);
    int children=cityChildCount(state);
    auto relationship=state.pregnancyDue ? "Expecting a baby / due day "+std::to_string(state.pregnancyDue)
      : children ? "Your girlfriend / "+std::to_string(children)+(children==1 ? " child" : " children")
      : state.dating ? std::string("Your girlfriend") : state.conversations ? std::string("Getting to know each other") : std::string("You haven't met yet");
    label(relationship,x+260,y+215,17,gold);
    wrap("Enjoys "+std::string(person.interest)+".",260,246,385,17,dim);
    std::string guidance=state.pregnancyDue ? "Your baby will arrive here on the due day. Time passes while you play or sleep; menus pause it."
      : children ? (hungryCityChildren(state,h.clock) ? "Time for a feed. Visit, then choose Feed baby or Feed children. Babies rest in the crib and are held while feeding." : "Fed and happy. Visit your family, chat, or spend time together.")
      : state.dating ? "Visit to spend the night together, then wake up here in the morning. Choose Start a family when you both want a baby."
      : "Visit her apartment, then press V to talk. Chat and get to know her before asking her out.";
    wrap(h.cityMessage.empty() ? guidance : h.cityMessage,260,298,386,18,ink);
    button(260,392,396,38,state.home<0 ? "Visit apartment" : "Visit family at mansion");button(260,442,188,38,"Chat",h.nearResident);
    button(468,442,188,38,state.dating ? "Spend the night" : "Ask on a date",h.nearResident);
    if(h.nearResident && state.dating) {
      button(260,486,children ? 188 : 396,30,state.pregnancyDue ? "Baby on the way" : children==3 ? "Family complete" : "Start a family",!state.pregnancyDue && children<3);
      if(children) {
        bool baby=std::ranges::any_of(state.children,[&](auto day){return day && day==h.clock.day;});
        button(468,486,188,30,baby ? "Feed baby" : "Feed children");
      }
    }
    else if(!h.nearResident)label("Conversation is available when you're there.",x+260,y+486,15,dim);
  } else {
    label("You own 36 towers and 2 data centers",x+24,y+142,23,ink);
    label("Income: "+std::to_string(h.rentPerDay+h.serverPerDay)+" coins / day, paid to your bank",x+24,y+173,17,gold);
    for(int row=0;row<propertyRows;++row) {
      int i=h.propertyPage*propertyRows+row;if(i>=int(metroBuildings().size()))break;
      auto b=metroBuildings()[i];float by=195+row*30;
      rectangle(x+24,y+by,632,28,row%2 ? glm::vec4(.09f,.15f,.18f,1) : card);
      label(b.name,x+34,y+by+2,16,ink);
      label(metroOffice(i) ? "Offices" : "Apartments",x+280,y+by+2,15,dim);
      label(std::to_string(b.floors*(metroOffice(i) ? 6 : 4))+" / day",x+444,y+by+2,15,gold);
      label("Visit",x+606,y+by+2,15,blue);
    }
    button(24,448,304,32,"Visit West data center");
    button(352,448,304,32,"Visit East data center");
    button(24,488,130,28,"Previous",h.propertyPage>0);button(526,488,130,28,"Next",(h.propertyPage+1)*propertyRows<int(metroBuildings().size()));
    labelCentered("Owned portfolio  /  Page "+std::to_string(h.propertyPage+1)+" of 5",x+340,y+493,15,dim);
  }
  labelCentered("L / E / Esc: return to the world",x+340,y+525,15,dim);
}
} // namespace bw

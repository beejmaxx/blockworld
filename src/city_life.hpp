#pragma once
#include "metropolis.hpp"
#include "ranch.hpp"

namespace bw {
struct CarSpec {std::string_view name,style;int body;glm::vec3 color;};
struct ResidentSpec {std::string_view name,interest;int age=18;};
std::span<const CarSpec> garageCars();
std::span<const ResidentSpec> cityResidents();
FarmCar parkedCar(const World&,int index);
bool bankTransfer(World&,int amount,bool deposit);
void recordCityTransaction(World&,BankKind,int amount);
glm::vec3 cityResidentHome(const World&,int resident);
glm::vec3 mansionPosition(const World&,int home,glm::vec3 local);
glm::vec3 cityCribPosition(const World&,int resident,int child);
bool visitCityHome(World&,Player&);
bool chooseCityHome(World&,Player&,int home);
std::string moveCityHousehold(World&,const Player&,int resident,int home);
std::string parkHomeCar(World&,const Player&,int home);
void appendMansionNursery(std::vector<Vertex>&,const World&,int home);
struct ServerContract {std::string_view name;int racks,revenue;};
std::span<const ServerContract> serverContracts();
struct DataCenterReport {int used=0,capacity=0,revenue=0,electricity=0,cooling=0,profit=0;};
DataCenterReport dataCenterReport(const DataCenterState&);
enum class ServerUpgrade {Rack,Power,Cooling};
int serverUpgradePrice(const DataCenterState&,ServerUpgrade);
std::string upgradeDataCenter(World&,const Player&,int site,ServerUpgrade);
std::string signServerContract(World&,int site,int contract);
glm::vec3 dataCenterTerminal(Cell,int site);
std::string startCityFamily(World&,int resident);
std::string feedCityFamily(World&,int resident);
int hungryCityChildren(const ResidentState&,const WorldClock&);
glm::vec3 cityChildPosition(const World&,int resident,int child);
bool cityChildFeeding(const World&,int resident,int child);
void updateCityFamilies(World&);
int cityChildCount(const ResidentState&);
glm::vec3 cityWorkerPosition(const World&,int index);
int propertyRentPerDay(const World&);
int serverIncomePerDay(const World&);
void collectCityIncome(World&);
std::string talkToResident(World&,int resident,bool askOut);
std::optional<std::string> partnerNightProblem(const World&,const Player&,int resident);
bool spendNightWithResident(World&,const Player&,int resident);
bool visitResident(World&,Player&,int resident);
bool takeGarageCar(World&,Player&,RideState&,int index);
enum class CityTargetKind { Resident,Car,Bank,Worker,Child,DataCenter };
struct CityTarget {CityTargetKind kind{};int index=0;float distance=0;};
std::optional<CityTarget> targetCity(const World&,const Player&,float reach=4);
std::string cityPrompt(const World&,CityTarget);
bool cityPeopleOverlap(const World&,Box);
std::vector<Vertex> residentMesh(const World&,float time);
inline constexpr std::size_t peopleVertexLimit=120000;
} // namespace bw

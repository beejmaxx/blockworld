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
enum class CityTargetKind { Resident,Car,Bank,Worker,Child };
struct CityTarget {CityTargetKind kind{};int index=0;float distance=0;};
std::optional<CityTarget> targetCity(const World&,const Player&,float reach=4);
std::string cityPrompt(const World&,CityTarget);
bool cityPeopleOverlap(const World&,Box);
std::vector<Vertex> residentMesh(const World&,float time);
inline constexpr std::size_t peopleVertexLimit=120000;
} // namespace bw

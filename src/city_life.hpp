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
int propertyRentPerDay(const World&);
int serverIncomePerDay(const World&);
void collectCityIncome(World&);
std::string talkToResident(World&,int resident,bool askOut);
bool visitResident(World&,Player&,int resident);
bool takeGarageCar(World&,Player&,RideState&,int index);
enum class CityTargetKind { Resident,Car,Bank };
struct CityTarget {CityTargetKind kind{};int index=0;float distance=0;};
std::optional<CityTarget> targetCity(const World&,const Player&,float reach=4);
std::string cityPrompt(const World&,CityTarget);
bool cityPeopleOverlap(const World&,Box);
std::vector<Vertex> residentMesh(const World&,float time);
} // namespace bw

#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace bw {
inline constexpr int garageSize=20,residentCount=8,bankLimit=999999999;
inline constexpr int cityWorkerCount=84;
enum class BankKind { Deposit,Withdrawal,Rent,Servers,BusinessPurchase };
struct BankEntry {BankKind kind{};int amount=0;std::uint32_t day=1;};
struct ResidentState {
  int conversations=0;bool dating=false;
  std::uint32_t pregnancyDue=0;
  std::array<std::uint32_t,3> children{};
  std::array<double,3> lastFed{};
  int home=-1; // -1: original apartment; 0..2: waterfront house.
};
struct DataCenterState {
  int racks=2,power=1,cooling=1,contracts=1;
};
struct CityLifeState {
  int bank=0,activeCar=0;
  int home=-1; // R destination; -1 retains the original farm cabin.
  std::array<int,3> homeCars{-1,-1,-1};
  std::array<DataCenterState,2> dataCenters{};
  std::uint32_t rentDay=0;
  std::array<ResidentState,residentCount> residents{};
  std::vector<BankEntry> statement;
};
} // namespace bw

#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace bw {
inline constexpr int garageSize=20,residentCount=8,bankLimit=999999999;
enum class BankKind { Deposit,Withdrawal,Rent,Servers };
struct BankEntry {BankKind kind{};int amount=0;std::uint32_t day=1;};
struct ResidentState {int conversations=0;bool dating=false;};
struct CityLifeState {
  int bank=0,activeCar=0;
  std::uint32_t rentDay=0;
  std::array<ResidentState,residentCount> residents{};
  std::vector<BankEntry> statement;
};
} // namespace bw

#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string_view>

namespace bw {
struct SkyState {
  glm::vec3 sun, horizon, zenith, ambient;
  float daylight{}, twilight{};
};
struct WorldClock {
  static constexpr double daySeconds=1200.0;
  static constexpr double morning=.30;
  double phase=.40; // Midnight = 0, sunrise = .25, noon = .5, sunset = .75.
  std::uint32_t day=1;
  void advance(double seconds);
  bool canSleep() const { return phase>=.72 || phase<.28; }
  void wakeAtMorning();
  SkyState sky() const;
  std::string_view period() const;
};
} // namespace bw

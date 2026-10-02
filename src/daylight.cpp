#include "daylight.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace bw {
namespace {
float smooth(float a,float b,float v) {
  float t=std::clamp((v-a)/(b-a),0.f,1.f);
  return t*t*(3.f-2.f*t);
}
}
void WorldClock::advance(double seconds) {
  if(!std::isfinite(seconds) || seconds<=0) return;
  double elapsed=phase+seconds/daySeconds;
  double days=std::floor(elapsed);
  day+=std::uint32_t(std::min(days,double(std::numeric_limits<std::uint32_t>::max()-day)));
  phase=elapsed-days;
}
void WorldClock::wakeAtMorning() {
  if(phase>=morning && day<std::numeric_limits<std::uint32_t>::max()) ++day;
  phase=morning;
}
SkyState WorldClock::sky() const {
  float angle=float((phase-.25)*2.0*std::numbers::pi);
  SkyState out;
  out.sun={std::cos(angle),.90f*std::sin(angle),-.43589f*std::sin(angle)};
  out.daylight=smooth(-.13f,.22f,out.sun.y);
  out.twilight=(1.f-smooth(.02f,.32f,std::abs(out.sun.y)))*smooth(-.25f,-.02f,out.sun.y);
  out.horizon=glm::mix(glm::vec3(.055f,.075f,.15f),glm::vec3(.69f,.80f,.81f),out.daylight);
  out.horizon=glm::mix(out.horizon,glm::vec3(.91f,.45f,.27f),out.twilight*.77f);
  out.zenith=glm::mix(glm::vec3(.012f,.021f,.064f),glm::vec3(.30f,.57f,.76f),out.daylight);
  out.zenith=glm::mix(out.zenith,glm::vec3(.27f,.24f,.40f),out.twilight*.55f);
  out.ambient=glm::mix(glm::vec3(.18f,.23f,.36f),glm::vec3(1.04f,1.015f,.96f),out.daylight);
  out.ambient=glm::mix(out.ambient,glm::vec3(.89f,.59f,.44f),out.twilight*.4f);
  return out;
}
std::string_view WorldClock::period() const {
  if(phase<.23 || phase>=.80) return "NIGHT";
  if(phase<.30) return "DAWN";
  if(phase<.48) return "MORNING";
  if(phase<.70) return "AFTERNOON";
  return "SUNSET";
}
} // namespace bw

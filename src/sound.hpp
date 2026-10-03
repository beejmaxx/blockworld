#pragma once
#include "world.hpp"
#include <span>

namespace bw {
enum class Sound { Step,Break,Place,Door,Sleep,Wake,Cluck,Water };

// A bounded, allocation-free mixer after construction. The SDL adapter owns synchronization.
class Soundscape {
public:
  static constexpr int sampleRate=48000;
  Soundscape();
  void play(Sound sound,Block material=Block::Wood,float gain=1.f,float pan=0.f);
  void environment(float daylight,float exposure,bool active,bool muted);
  void engine(bool running,float speed);
  void render(std::span<float> stereo);
private:
  struct Voice { int clip=-1; float cursor{},rate=1,left{},right{}; };
  std::array<std::vector<float>,17> clips_;
  std::array<Voice,24> voices_{};
  std::uint32_t random_=0x123abcefu;
  double time_=0;
  float day_=1,exposure_=1,gain_=0,targetDay_=1,targetExposure_=1,targetGain_=0;
  float windL_=0,windR_=0,birdTimer_=.9f,birdAge_=1,birdLength_=.2f,birdPhase_=0,birdPan_=0;
  float engineGain_=0,engineLoad_=0,enginePhase_=0,engineNoise_=0,targetEngineGain_=0,targetEngineLoad_=0;
  float random();
};
} // namespace bw

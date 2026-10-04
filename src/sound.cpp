#include "sound.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace bw {
namespace {
constexpr float tau=2.f*std::numbers::pi_v<float>;
int surface(Block b) {
  if(b==Block::Wood || b==Block::Planks || b==Block::Workbench || isDoor(b) || isBed(b) || b==Block::Torch || b==Block::Fence || isGate(b)) return 1;
  if(b==Block::Stone || b==Block::StoneSlab || b==Block::Brick || b==Block::Bedrock) return 2;
  if(b==Block::Glass) return 3;
  return 0;
}
float noise(std::uint32_t& state) {
  state^=state<<13; state^=state>>17; state^=state<<5;
  return float(state&0xffffu)/32767.5f-1.f;
}
std::vector<float> effect(int index) {
  bool impact=index<12;
  int action=index/4,material=index%4;
  float duration=impact ? (action==1 ? .24f : .15f) : index==16 ? .5f : index==12 || index==15 ? .42f : index==13 ? 1.35f : 1.65f;
  std::vector<float> out(std::size_t(duration*Soundscape::sampleRate));
  std::uint32_t seed=0x953ba451u+std::uint32_t(index)*31;
  float low=0,phase=0;
  for(std::size_t i=0;i<out.size();++i) {
    float t=float(i)/Soundscape::sampleRate,u=t/duration,n=noise(seed);
    low+=.12f*(n-low);
    float sample=0;
    if(impact) {
      float attack=std::min(t/.003f,1.f),decay=std::exp(-t*(action==1 ? 15.f : 29.f));
      float frequency=material==1 ? 175.f : material==2 ? 340.f : material==3 ? 1750.f : 92.f;
      phase+=tau*frequency*(1.f-.25f*u)/Soundscape::sampleRate;
      float texture=material==0 ? low*.9f+n*.10f : material==1 ? low*.3f+std::sin(phase)*.32f
                        : material==2 ? n*.23f+std::sin(phase)*.16f : std::sin(phase)*.22f+std::sin(phase*1.47f)*.13f;
      if(action==1) texture+=n*.17f*std::max(0.f,std::sin(t*83.f));
      sample=texture*attack*decay*(action==0 ? .65f : action==1 ? 1.f : .80f);
    } else if(index==12) {
      phase+=tau*(100.f+24.f*std::sin(t*31.f))/Soundscape::sampleRate;
      sample=(std::sin(phase)*.17f+std::sin(phase*2)*.06f+low*.16f)*std::sin(std::numbers::pi_v<float>*u);
      sample+=low*.9f*std::exp(-t*90.f)*std::min(t/.002f,1.f);
    } else if(index==15) {
      float age=std::fmod(t,.19f),envelope=std::min(age/.006f,1.f)*std::exp(-age*30);
      phase+=tau*(310.f+340.f*std::exp(-age*25))/Soundscape::sampleRate;
      sample=(std::sin(phase)*.27f+std::sin(phase*1.9f)*.09f+low*.24f)*envelope;
    } else if(index==16) {
      float pulse=.6f+.4f*std::sin(t*95.f);
      phase+=tau*(1200.f+700.f*std::sin(t*43.f))/Soundscape::sampleRate;
      sample=(low*.62f+n*.055f+std::sin(phase)*.045f*pulse)*std::min(t/.02f,1.f);
    } else {
      constexpr std::array notes{261.63f,329.63f,392.f};
      for(int note=0;note<3;++note) {
        float age=t-note*.16f;
        if(age<0) continue;
        float frequency=notes[note]*(index==14 ? 2.f : 1.f);
        sample+=(std::sin(tau*frequency*age)+.20f*std::sin(tau*frequency*2*age))
                  *.10f*std::min(age/.018f,1.f)*std::exp(-age*3.8f);
      }
    }
    out[i]=sample*(1.f-u)*(1.f-u);
  }
  return out;
}
}
Soundscape::Soundscape() {
  for(int i=0;i<int(clips_.size());++i) clips_[i]=effect(i);
}
float Soundscape::random() { return noise(random_)*.5f+.5f; }
void Soundscape::play(Sound sound,Block material,float gain,float pan) {
  int index=sound==Sound::Door ? 12 : sound==Sound::Sleep ? 13 : sound==Sound::Wake ? 14 : sound==Sound::Cluck ? 15 : sound==Sound::Water ? 16 : int(sound)*4+surface(material);
  auto* voice=&voices_[0];
  for(auto& v : voices_) {
    if(v.clip<0) { voice=&v; break; }
    if(v.cursor>voice->cursor) voice=&v;
  }
  pan=std::clamp(pan,-1.f,1.f); gain=std::clamp(gain,0.f,1.5f);
  *voice={index,0,sound==Sound::Sleep || sound==Sound::Wake ? 1.f : .93f+random()*.14f,
          gain*std::sqrt((1.f-pan)*.5f),gain*std::sqrt((1.f+pan)*.5f)};
}
void Soundscape::environment(float daylight,float exposure,bool active,bool muted) {
  targetDay_=std::clamp(daylight,0.f,1.f); targetExposure_=std::clamp(exposure,0.f,1.f);
  targetGain_=active && !muted ? .75f : 0.f;
}
void Soundscape::engine(bool running,float speed) {
  targetEngineGain_=running ? 1.f : 0.f;
  float load=running && std::isfinite(speed) ? std::clamp(std::abs(speed)/carBoostSpeed,0.f,1.f) : 0.f;
  // Short drops in revs suggest automatic gear changes as the GT2 accelerates.
  targetEngineLoad_=load==0 ? 0 : .18f+.5f*load+.30f*std::fmod(load*4.99f,1.f);
}
void Soundscape::render(std::span<float> stereo) {
  constexpr float step=1.f/sampleRate;
  for(std::size_t i=0;i+1<stereo.size();i+=2) {
    day_+=(targetDay_-day_)*.00006f;
    exposure_+=(targetExposure_-exposure_)*.00008f;
    gain_+=(targetGain_-gain_)*.003f; // Click-free mute/pause in a few milliseconds.
    windL_+=.009f*(random()*2.f-1.f-windL_);
    windR_+=.008f*(random()*2.f-1.f-windR_);
    float breeze=.36f+.12f*std::sin(float(time_*.37))+.08f*std::sin(float(time_*.83));
    float left=windL_*breeze,right=windR_*breeze;
    birdTimer_-=step; birdAge_+=step;
    if(birdTimer_<=0) {
      birdLength_=.15f+random()*.18f; birdAge_=0; birdPhase_=0; birdPan_=random();
      birdTimer_=.8f+random()*4.0f;
    }
    if(birdAge_<birdLength_) {
      float u=birdAge_/birdLength_,envelope=std::sin(std::numbers::pi_v<float>*u);
      birdPhase_+=tau*(2300.f+1400.f*std::sin(u*3.8f)+210.f*std::sin(u*30.f))*step;
      float bird=std::sin(birdPhase_)*envelope*envelope*.052f*day_;
      left+=bird*(1.f-birdPan_); right+=bird*birdPan_;
    }
    float chirp=float(std::fmod(time_,1.3));
    if(chirp<.45f) {
      float pulse=std::max(0.f,std::sin(chirp*tau*20.f));
      float cricket=std::sin(float(std::fmod(time_*2850.,1.))*tau)*pulse*pulse*.019f*(1.f-day_);
      left+=cricket*.6f; right+=cricket;
    }
    left*=exposure_; right*=exposure_;
    // Smooth RPM and volume changes avoid clicks on entry, braking, and exit.
    engineGain_+=(targetEngineGain_-engineGain_)*.0004f;
    engineLoad_+=(targetEngineLoad_-engineLoad_)*(targetEngineLoad_>engineLoad_ ? .00008f : .000045f);
    float firing=42.f+engineLoad_*92.f+std::sin(float(time_*11.))*1.1f;
    enginePhase_+=tau*firing*step;
    if(enginePhase_>=tau) enginePhase_-=tau;
    engineNoise_+=.035f*(random()*2.f-1.f-engineNoise_);
    float motor=.64f*std::sin(enginePhase_)+.24f*std::sin(enginePhase_*2.f+.3f)+.12f*std::sin(enginePhase_*3.f);
    motor=(motor+engineNoise_*.16f)*engineGain_*(.12f+.09f*engineLoad_);
    left+=motor; right+=motor*.97f;
    for(auto& voice : voices_) {
      if(voice.clip<0) continue;
      const auto& clip=clips_[voice.clip];
      auto cursor=std::size_t(voice.cursor);
      if(cursor+1>=clip.size()) { voice.clip=-1; continue; }
      float value=std::lerp(clip[cursor],clip[cursor+1],voice.cursor-float(cursor));
      left+=value*voice.left; right+=value*voice.right; voice.cursor+=voice.rate;
    }
    stereo[i]=gain_*left/(1.f+std::abs(left)); stereo[i+1]=gain_*right/(1.f+std::abs(right));
    time_+=step;
  }
}
} // namespace bw

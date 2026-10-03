#pragma once
#include "sound.hpp"
#include <SDL3/SDL.h>
#include <atomic>

namespace bw {
class Audio {
public:
  Audio();
  ~Audio();
  Audio(const Audio&)=delete;
  Audio& operator=(const Audio&)=delete;
  void play(Sound sound,Block material=Block::Wood,float gain=1.f,float pan=0.f);
  void environment(float daylight,float exposure,bool active,bool muted);
  void engine(bool running,float speed);
  bool available() const { return stream_ && !failed_.load(); }
  std::uint64_t framesRendered() const { return frames_.load(); }
private:
  Soundscape mixer_;
  SDL_AudioStream* stream_{};
  std::atomic<bool> failed_=false;
  std::atomic<std::uint64_t> frames_=0;
  static void SDLCALL feed(void* userdata,SDL_AudioStream* stream,int additional,int total);
};
} // namespace bw

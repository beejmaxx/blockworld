#include "audio.hpp"
#include <algorithm>
#include <iostream>

namespace bw {
Audio::Audio() {
  if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    SDL_AudioSpec spec{SDL_AUDIO_F32,2,Soundscape::sampleRate};
    stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,feed,this);
    if(stream_ && !SDL_ResumeAudioStreamDevice(stream_)) { SDL_DestroyAudioStream(stream_); stream_=nullptr; }
  }
  if(stream_) std::cout<<"Audio: "<<SDL_GetCurrentAudioDriver()<<" / stereo procedural soundscape\n";
  else std::cerr<<"Audio unavailable: "<<SDL_GetError()<<" (continuing silently)\n";
}
Audio::~Audio() { if(stream_) SDL_DestroyAudioStream(stream_); }
void Audio::play(Sound sound,Block material,float gain,float pan) {
  if(!stream_) return;
  SDL_LockAudioStream(stream_); mixer_.play(sound,material,gain,pan); SDL_UnlockAudioStream(stream_);
}
void Audio::environment(float daylight,float exposure,bool active,bool muted) {
  if(!stream_) return;
  SDL_LockAudioStream(stream_); mixer_.environment(daylight,exposure,active,muted); SDL_UnlockAudioStream(stream_);
}
void Audio::engine(bool running,float speed) {
  if(!stream_) return;
  SDL_LockAudioStream(stream_); mixer_.engine(running,speed); SDL_UnlockAudioStream(stream_);
}
void SDLCALL Audio::feed(void* userdata,SDL_AudioStream* stream,int additional,int) {
  auto& audio=*static_cast<Audio*>(userdata);
  std::array<float,1024> samples{};
  while(additional>0) {
    int frames=std::min(512,(additional+7)/8);
    audio.mixer_.render(std::span(samples.data(),std::size_t(frames)*2));
    if(!SDL_PutAudioStreamData(stream,samples.data(),frames*8)) { audio.failed_=true; return; }
    audio.frames_+=std::uint64_t(frames); additional-=frames*8;
  }
}
} // namespace bw

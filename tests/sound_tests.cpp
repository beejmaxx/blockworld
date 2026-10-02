#include "sound.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace bw;
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
double energy(const std::vector<float>& samples) {
  double sum=0; for(auto sample : samples) { check(std::isfinite(sample) && std::abs(sample)<=1,"finite bounded audio"); sum+=sample*sample; }
  return sum/double(samples.size());
}
void writeWav(const std::filesystem::path& path,const std::vector<float>& samples) {
  if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path,std::ios::binary);
  auto u16=[&](std::uint16_t n) { out.put(char(n&255)); out.put(char(n>>8)); };
  auto u32=[&](std::uint32_t n) { u16(std::uint16_t(n)); u16(std::uint16_t(n>>16)); };
  auto bytes=std::uint32_t(samples.size()*2);
  out.write("RIFF",4); u32(36+bytes); out.write("WAVEfmt ",8); u32(16); u16(1); u16(2);
  u32(Soundscape::sampleRate); u32(Soundscape::sampleRate*4); u16(4); u16(16); out.write("data",4); u32(bytes);
  for(float f : samples) u16(std::uint16_t(std::int16_t(std::clamp(f,-1.f,1.f)*32767.f)));
  check(bool(out),"write audio preview");
}
void preview(const std::filesystem::path& path) {
  Soundscape sound;
  std::vector<float> result,block(Soundscape::sampleRate); // Half-second chunks.
  for(int i=0;i<40;++i) {
    sound.environment(i<22 || i>=34 ? 1.f : 0.f,1.f,true,false);
    if(i<8) sound.play(Sound::Step,i<4 ? Block::Grass : Block::Planks);
    if(i==9) sound.play(Sound::Break,Block::Stone);
    if(i==11) sound.play(Sound::Place,Block::Wood);
    if(i==13) sound.play(Sound::Break,Block::Glass);
    if(i==15) sound.play(Sound::Door);
    if(i==18 || i==20) sound.play(Sound::Cluck);
    if(i==29) sound.play(Sound::Sleep);
    if(i==34) sound.play(Sound::Wake);
    sound.render(block); result.insert(result.end(),block.begin(),block.end());
  }
  writeWav(path,result); std::cout<<"Audio preview: "<<path<<'\n';
}
}
int main(int argc,char** argv) {
  try {
    std::vector<float> buffer(Soundscape::sampleRate*2),grass,wood;
    for(auto material : {Block::Grass,Block::Planks,Block::Stone,Block::Glass}) {
      Soundscape mixer; mixer.environment(1,0,true,false); mixer.render(buffer);
      mixer.play(Sound::Step,material); mixer.render(buffer);
      check(energy(buffer)>1e-7,"material footstep is audible");
      if(material==Block::Grass) grass=buffer;
      if(material==Block::Planks) wood=buffer;
    }
    check(grass!=wood,"walking on wood and grass sounds different");
    Soundscape mixer; mixer.environment(1,1,true,false); mixer.render(buffer);
    check(energy(buffer)>1e-6,"day ambience is audible");
    for(int i=0;i<100;++i) mixer.play(Sound::Break,Block::Stone);
    mixer.render(buffer); energy(buffer); // Voice stealing and clipping protection under a burst.
    mixer.environment(0,1,true,false); mixer.render(buffer); mixer.render(buffer);
    check(energy(buffer)>1e-6,"night ambience is audible");
    mixer.environment(0,1,true,true); mixer.render(buffer); mixer.render(buffer);
    check(energy(buffer)<1e-12,"mute fades to silence");
    mixer.environment(1,1,true,false); mixer.render(buffer);
    check(energy(buffer)>1e-6,"unmute restores ambience");
    mixer.environment(1,1,false,false); mixer.render(buffer); mixer.render(buffer);
    check(energy(buffer)<1e-12,"pause fades to silence");
    for(auto event : {Sound::Break,Sound::Place,Sound::Door,Sound::Sleep,Sound::Wake,Sound::Cluck,Sound::Water}) {
      Soundscape single; single.environment(1,0,true,false); single.render(buffer);
      single.play(event); single.render(buffer); check(energy(buffer)>1e-7,"interaction sound is audible");
    }
    if(argc==2) preview(argv[1]);
    std::cout<<"PASS sound: materials, interactions, ambience, bounded mixing, mute, pause\n";
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}

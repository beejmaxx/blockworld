#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace bw {
struct PerformanceStats {
  double fps=0,frameMs=0,p95Ms=0,cpuPercent=0;
  bool cpuAvailable=false,memoryAvailable=false;
  std::uint64_t footprint=0,resident=0,peakResident=0,meshBytes=0;
  std::size_t chunks=0,meshes=0,triangles=0,edits=0,count=0;
  std::array<float,120> history{}; // Oldest to newest, milliseconds.
};
class Diagnostics {
public:
  void frame(double seconds);
  PerformanceStats snapshot() const;
private:
  std::array<float,120> frames_{};
  std::size_t next_=0,count_=0;
  double sampleElapsed_=.5,previousCpu_=0;
  bool sampledCpu_=false;
  PerformanceStats process_;
};
}

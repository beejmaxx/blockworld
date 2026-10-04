#include "diagnostics.hpp"
#include <algorithm>
#include <cmath>
#include <mach/mach.h>
#include <sys/resource.h>

namespace bw {
void Diagnostics::frame(double seconds) {
  if(!std::isfinite(seconds) || seconds<=0)return;
  frames_[next_]=float(seconds*1000);next_=(next_+1)%frames_.size();count_=std::min(count_+1,frames_.size());
  sampleElapsed_+=seconds;
  if(sampleElapsed_<.5)return;
  rusage usage{};
  if(getrusage(RUSAGE_SELF,&usage)==0) {
    double cpu=double(usage.ru_utime.tv_sec+usage.ru_stime.tv_sec)+double(usage.ru_utime.tv_usec+usage.ru_stime.tv_usec)/1e6;
    process_.cpuAvailable=sampledCpu_;
    if(sampledCpu_)process_.cpuPercent=std::max(0.,(cpu-previousCpu_)/sampleElapsed_*100.);
    previousCpu_=cpu;sampledCpu_=true;
  } else {process_.cpuAvailable=false;sampledCpu_=false;}
  task_vm_info_data_t info{};mach_msg_type_number_t size=TASK_VM_INFO_COUNT;
  process_.memoryAvailable=task_info(mach_task_self(),TASK_VM_INFO,reinterpret_cast<task_info_t>(&info),&size)==KERN_SUCCESS && size>=TASK_VM_INFO_REV1_COUNT;
  if(process_.memoryAvailable) {
    process_.footprint=info.phys_footprint;process_.resident=info.resident_size;process_.peakResident=info.resident_size_peak;
  }
  sampleElapsed_=0;
}
PerformanceStats Diagnostics::snapshot() const {
  auto out=process_;out.count=count_;
  double total=0;std::array<float,120> ordered{};
  for(std::size_t i=0;i<count_;++i) {
    auto value=frames_[(next_+frames_.size()-count_+i)%frames_.size()];
    out.history[i]=value;ordered[i]=value;total+=value;
  }
  if(count_) {
    out.frameMs=total/double(count_);out.fps=1000./out.frameMs;
    std::sort(ordered.begin(),ordered.begin()+count_);
    out.p95Ms=ordered[std::size_t(std::ceil(double(count_)*.95))-1];
  }
  return out;
}
}

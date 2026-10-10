#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <random>
#include "../Source/dsp/CloudReverb.h"

static double runRms(double send,double decay,double cloud,double size,long blocks,bool report){
  const double sr=48000; const int blk=512;
  lunara::CloudReverb r; r.prepare(sr);
  juce::AudioBuffer<float> b(2,blk);
  std::mt19937 rng(7); std::uniform_real_distribution<float> u(-0.15f,0.15f);
  double sq=0; long n=0; float peak=0;
  for(long k=0;k<blocks;k++){
    for(int c=0;c<2;c++){auto*p=b.getWritePointer(c);for(int i=0;i<blk;i++)p[i]=u(rng);}
    r.process(b.getWritePointer(0),b.getWritePointer(1),blk,(float)size,(float)decay,0.5f,0.4f,(float)cloud,(float)send);
    if(report && k%500==0){ float pk=0; for(int c=0;c<2;c++){const float*p=b.getReadPointer(c);for(int i=0;i<blk;i++)pk=std::max(pk,std::abs(p[i]));}
      std::cout<<"  block "<<k<<" peak="<<pk<<"\n"; }
    if(k>=blocks-200){ for(int c=0;c<2;c++){const float*p=b.getReadPointer(c);for(int i=0;i<blk;i++){sq+=(double)p[i]*p[i];n++;peak=std::max(peak,std::abs(p[i]));}} }
  }
  if(report) std::cout<<"  final peak="<<peak<<"\n";
  return std::sqrt(sq/(double)n);
}
int main(){
  double dry=runRms(0.0,0.6,0.5,0.6,400,false);
  std::cout<<"input(no reverb) rms="<<dry<<"\n";
  std::cout<<"log params (decay=0.68,cloud=1.0,send=0.64) long run:\n";
  std::cout<<"  rms="<<runRms(0.64,0.68,1.0,0.6,3000,true)<<"\n";
}

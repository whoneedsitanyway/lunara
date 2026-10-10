#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <cmath>
#include <random>
#include "../Source/dsp/CloudReverb.h"

static double runRms(double send,double decay,double cloud){
  const double sr=48000; const int blk=512;
  lunara::CloudReverb r; r.prepare(sr);
  juce::AudioBuffer<float> b(2,blk);
  std::mt19937 rng(7); std::uniform_real_distribution<float> u(-0.3f,0.3f);
  double sq=0; long n=0;
  for(int k=0;k<400;k++){
    for(int c=0;c<2;c++){auto*p=b.getWritePointer(c);for(int i=0;i<blk;i++)p[i]=u(rng);}
    r.process(b.getWritePointer(0),b.getWritePointer(1),blk,0.6f,(float)decay,0.5f,0.4f,(float)cloud,(float)send);
    if(k>=200){ for(int c=0;c<2;c++){const float*p=b.getReadPointer(c);for(int i=0;i<blk;i++){sq+=(double)p[i]*p[i];n++;}} }
  }
  return std::sqrt(sq/(double)n);
}
int main(){
  double dry=runRms(0.0,0.6,0.5);
  std::cout<<"input(no reverb) rms="<<dry<<"\n";
  std::cout<<"send=0.5 -> rms="<<runRms(0.5,0.6,0.5)<<"\n";
  std::cout<<"send=1.0 -> rms="<<runRms(1.0,0.6,0.5)<<"\n";
  std::cout<<"send=1.2 -> rms="<<runRms(1.2,0.6,0.5)<<"\n";
  std::cout<<"decay=1.0-> rms="<<runRms(1.0,1.0,0.5)<<"\n";
}

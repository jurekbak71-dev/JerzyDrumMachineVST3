#include <JuceHeader.h>
#include "DrumEngine.h"
#include <iostream>
#include <cmath>
int main(){
 try{
  std::cerr<<"SMOKE: main entered\n"<<std::flush;
  DrumEngine e;
  std::cerr<<"SMOKE: engine constructed\n"<<std::flush;
  e.prepare(48000.0);
  std::cerr<<"SMOKE: prepared\n"<<std::flush;
  e.setHost(120.0,false);
  e.trigger(0,1.0f);
  juce::AudioBuffer<float> b(2,8192);
  e.process(b);
  std::cerr<<"SMOKE: processed direct trigger\n"<<std::flush;
  double energy=0.0;
  for(int ch=0;ch<2;ch++)for(int i=0;i<b.getNumSamples();i++){auto x=b.getSample(ch,i);if(!std::isfinite(x)){std::cerr<<"FAIL: non-finite audio\n"<<std::flush;return 4;}energy+=std::abs(x);}
  std::cerr<<"SMOKE: energy="<<energy<<"\n"<<std::flush;
  if(!(energy>1.0)){std::cerr<<"FAIL: engine produced silence\n"<<std::flush;return 2;}
  e.gain[0]=0.42f;e.panorama[0]=-0.25f;
  auto state=e.saveState();
  std::cerr<<"SMOKE: state saved\n"<<std::flush;
  DrumEngine copy;copy.prepare(48000.0);copy.loadState(state);
  std::cerr<<"SMOKE: state restored\n"<<std::flush;
  if(std::abs(copy.gain[0]-0.42f)>.001f||std::abs(copy.panorama[0]+0.25f)>.001f){std::cerr<<"FAIL: state restore mismatch\n"<<std::flush;return 3;}
  std::cerr<<"PASS: audio and state smoke test\n"<<std::flush;
  return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL exception: "<<e.what()<<"\n"<<std::flush;return 10;}
 catch(...){std::cerr<<"FAIL unknown exception\n"<<std::flush;return 11;}
}
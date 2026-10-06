#include <JuceHeader.h>
#include "DrumEngine.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <string>
int main(){
 std::ofstream trace("smoke-diagnostics.txt",std::ios::out|std::ios::trunc);
 auto log=[&](const std::string& message){trace<<message<<'\n';trace.flush();std::cerr<<message<<'\n'<<std::flush;};
 try{
  log("SMOKE: main entered");
  DrumEngine e;
  log("SMOKE: engine constructed");
  e.prepare(48000.0);
  log("SMOKE: prepared");
  e.setHost(120.0,false);
  e.trigger(0,1.0f);
  juce::AudioBuffer<float> b(2,8192);
  e.process(b);
  log("SMOKE: processed direct trigger");
  double energy=0.0;
  for(int ch=0;ch<2;ch++)for(int i=0;i<b.getNumSamples();i++){auto x=b.getSample(ch,i);if(!std::isfinite(x)){log("FAIL: non-finite audio");return 4;}energy+=std::abs(x);}
  log(std::string("SMOKE: energy=")+std::to_string(energy));
  if(!(energy>1.0)){log("FAIL: engine produced silence");return 2;}
  e.gain[0]=0.42f;e.panorama[0]=-0.25f;
  auto state=e.saveState();
  log("SMOKE: state saved");
  DrumEngine copy;copy.prepare(48000.0);copy.loadState(state);
  log("SMOKE: state restored");
  if(std::abs(copy.gain[0]-0.42f)>.001f||std::abs(copy.panorama[0]+0.25f)>.001f){log("FAIL: state restore mismatch");return 3;}
  log("PASS: audio and state smoke test");
  return 0;
 }catch(const std::exception&e){log(std::string("FAIL exception: ")+e.what());return 10;}
 catch(...){log("FAIL unknown exception");return 11;}
}
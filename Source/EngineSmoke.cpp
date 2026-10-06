#include <JuceHeader.h>
#include "DrumEngine.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <string>
#include <memory>
int main(){
 std::ofstream trace("smoke-diagnostics.txt",std::ios::out|std::ios::trunc);
 auto log=[&](const std::string& message){trace<<message<<'\n';trace.flush();std::cerr<<message<<'\n'<<std::flush;};
 try{
  log("SMOKE: main entered");
  auto e=std::make_unique<DrumEngine>();
  log("SMOKE: engine constructed");
  e->prepare(48000.0);
  log("SMOKE: prepared");
  e->setHost(120.0,false);
  e->trigger(0,1.0f);
  juce::AudioBuffer<float> b(2,8192);
  e->process(b);
  log("SMOKE: processed direct trigger");
  double energy=0.0;
  for(int ch=0;ch<2;ch++)for(int i=0;i<b.getNumSamples();i++){auto x=b.getSample(ch,i);if(!std::isfinite(x)){log("FAIL: non-finite audio");return 4;}energy+=std::abs(x);}
  log(std::string("SMOKE: energy=")+std::to_string(energy));
  if(!(energy>1.0)){log("FAIL: engine produced silence");return 2;}
  auto renderAnalog=[](int kind,int parameter,float value){
   VoiceDSP voice;voice.prepare(48000.0);voice.rng.setSeed(0x12345678);
   voice.tune=.2f;voice.decay=.35f;voice.tone=.25f;voice.character=.2f;
   if(parameter==0)voice.tune=value;else if(parameter==1)voice.decay=value;else if(parameter==2)voice.tone=value;else voice.character=value;
   voice.trigger(.8f,kind);double sum=0.0;
   for(int i=0;i<4096;i++){const float x=voice.process();sum+=std::abs(x);}
   return sum;
  };
  for(int kind=0;kind<4;kind++)for(int parameter=0;parameter<4;parameter++){
   const double low=renderAnalog(kind,parameter,.2f),high=renderAnalog(kind,parameter,.8f);
   if(std::abs(low-high)<.01){log("FAIL: analog control does not change rendered voice "+std::to_string(kind)+" parameter "+std::to_string(parameter));return 8;}
  }
  log("SMOKE: all kick/snare/tom/hat controls change rendered audio");
  e->triggerSynthNote(69,.8f);
  if(e->getSynthMidiNote()!=69){log("FAIL: synth MIDI pitch was not applied");return 5;}
  e->pattern(0).step[11][0].note=72;e->pattern(0).step[11][0].on=true;
  e->setSongEntry(0,0,0);e->setSongEntry(1,1,2);
  e->gain[0]=0.42f;e->panorama[0]=-0.25f;
  auto state=e->saveState();
  log("SMOKE: state saved");
  auto copy=std::make_unique<DrumEngine>();copy->prepare(48000.0);copy->loadState(state);
  log("SMOKE: state restored");
  if(std::abs(copy->gain[0]-0.42f)>.001f||std::abs(copy->panorama[0]+0.25f)>.001f){log("FAIL: state restore mismatch");return 3;}
  if(copy->pattern(0).step[11][0].note!=72){log("FAIL: synth step note did not survive state restore");return 6;}
  if(copy->getSongLength()!=2||copy->getSongEntry(1).pattern!=1||copy->getSongEntry(1).section!=2){log("FAIL: song chain did not survive state restore");return 7;}
  log("PASS: audio, synth MIDI pitch and state smoke test");
  return 0;
 }catch(const std::exception&e){log(std::string("FAIL exception: ")+e.what());return 10;}
 catch(...){log("FAIL unknown exception");return 11;}
}

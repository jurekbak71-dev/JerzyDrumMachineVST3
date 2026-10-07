#include <JuceHeader.h>
#include "DrumEngine.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

int main()
{
 std::ofstream trace("smoke-diagnostics.txt",std::ios::out|std::ios::trunc);
 auto log=[&](const std::string& message){trace<<message<<'\n';trace.flush();std::cerr<<message<<'\n'<<std::flush;};
 try
 {
  DrumEngine engine;engine.prepare(48000.0);engine.setHost(120.0,false);engine.trigger(0,1.0f);
  juce::AudioBuffer<float> audio(2,8192);engine.process(audio);double energy=0.0;
  for(int channel=0;channel<2;channel++)for(int sample=0;sample<audio.getNumSamples();sample++){const float value=audio.getSample(channel,sample);if(!std::isfinite(value)){log("FAIL: non-finite output");return 1;}energy+=std::abs(value);}
  if(energy<=1.0){log("FAIL: engine produced silence");return 2;}

  auto renderDrum=[](int kind,int parameter,float value){VoiceDSP voice;voice.prepare(48000.0);voice.tune=.2f;voice.decay=.35f;voice.tone=.25f;voice.character=.2f;float* target=parameter==0?&voice.tune:(parameter==1?&voice.decay:(parameter==2?&voice.tone:&voice.character));*target=value;voice.trigger(.8f,kind);double sum=0.0;for(int i=0;i<8192;i++)sum+=std::abs(voice.process());return sum;};
  for(int kind=0;kind<4;kind++)for(int parameter=0;parameter<4;parameter++){const double low=renderDrum(kind,parameter,.2f),high=renderDrum(kind,parameter,.8f);if(std::abs(low-high)<.01){log("FAIL: drum control did not change audio");return 3;}}

  VoiceDSP synth;synth.prepare(48000.0);synth.configureSynth(.1f,1000.0f,1.0f,100.0f,20000.0f,0.0f,0,0,false,0.0f,0.0f,0,false);synth.synthNoteOn(60,1.0f);
  int firstCrossing=-1,lastCrossing=-1,crossings=0;float previous=0.0f;
  for(int i=0;i<48000;i++){const float sample=synth.processSynth();if(previous<=0.0f&&sample>0.0f){if(firstCrossing<0)firstCrossing=i;lastCrossing=i;++crossings;}previous=sample;}
  const double measuredHz=(crossings>1)?(crossings-1)*48000.0/(lastCrossing-firstCrossing):0.0;
  if(std::abs(measuredHz-261.625565)>1.5){log("FAIL: MIDI note 60 is not tuned to C4");return 4;}

  DrumEngine rhythm;rhythm.prepare(48000.0);rhythm.setHost(120.0,false);rhythm.pattern(0).length=64;rhythm.setTrackLength(0,0,3);rhythm.setTrackLength(0,1,5);rhythm.setPreviewPlaying(true);juce::AudioBuffer<float> rhythmAudio(2,36001);rhythm.process(rhythmAudio);
  if(rhythm.getTrackStep(0)!=0||rhythm.getTrackStep(1)!=1){log("FAIL: independent track loops did not advance at separate lengths");return 5;}

  engine.pattern(0).step[11][0].note=72;engine.pattern(0).step[11][0].on=true;engine.setTrackLength(0,0,7);engine.setSongEntry(0,0,0,8);engine.setSongEntry(1,1,2,3);engine.gain[0]=.42f;engine.panorama[0]=-.25f;
  auto state=engine.saveState();DrumEngine restored;restored.prepare(48000.0);restored.loadState(state);
  if(std::abs(restored.gain[0]-.42f)>.001f||std::abs(restored.panorama[0]+.25f)>.001f||restored.pattern(0).step[11][0].note!=72||restored.pattern(0).trackLength[0]!=7){log("FAIL: engine state did not restore");return 6;}
  if(restored.getSongLength()!=2||restored.getSongEntry(0).bars!=8||restored.getSongEntry(1).pattern!=1||restored.getSongEntry(1).section!=2){log("FAIL: song chain did not restore section lengths");return 7;}
  log("PASS: audio finite, drum controls audible, C4 tuned, polyrhythm advances independently, state round-trip");return 0;
 }
 catch(const std::exception& error){log(std::string("FAIL exception: ")+error.what());return 10;}
 catch(...){log("FAIL unknown exception");return 11;}
}

#include <JuceHeader.h>
#include "DrumEngine.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
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
  juce::ScopedJuceInitialiser_GUI guiInitialiser;
  auto engine=std::make_unique<DrumEngine>();engine->prepare(48000.0);engine->setHost(120.0,false);engine->trigger(0,1.0f);
  juce::AudioBuffer<float> audio(2,8192);engine->process(audio);double energy=0.0;
  for(int channel=0;channel<2;channel++)for(int sample=0;sample<audio.getNumSamples();sample++){const float value=audio.getSample(channel,sample);if(!std::isfinite(value)){log("FAIL: non-finite output");return 1;}energy+=std::abs(value);}
  if(energy<=1.0){log("FAIL: engine produced silence");return 2;}
  if(engine->getChannelMeter(0)<=0.0f){log("FAIL: mixer channel meter did not report activity");return 17;}

  auto renderDrum=[](int kind,int parameter,float value){VoiceDSP voice;voice.prepare(48000.0);voice.tune=.2f;voice.decay=.35f;voice.tone=.25f;voice.character=.2f;float* target=parameter==0?&voice.tune:(parameter==1?&voice.decay:(parameter==2?&voice.tone:&voice.character));*target=value;voice.trigger(.8f,kind);double sum=0.0;for(int i=0;i<8192;i++)sum+=std::abs(voice.process());return sum;};
  for(int kind=0;kind<8;kind++)for(int parameter=0;parameter<4;parameter++){const double low=renderDrum(kind,parameter,.2f),high=renderDrum(kind,parameter,.8f);if(std::abs(low-high)<.01){log("FAIL: analog/digital drum control did not change audio");return 3;}}
  auto renderHatTail=[](int kind){VoiceDSP voice;voice.prepare(48000.0);voice.tune=.5f;voice.decay=.5f;voice.tone=.55f;voice.character=.65f;voice.trigger(.85f,kind);double sum=0;for(int i=0;i<96000;i++){const float x=voice.process();if(i>=24000)sum+=std::abs(x);}return sum;};
  const double closedHatTail=renderHatTail(3),openHatTail=renderHatTail(5);if(openHatTail<closedHatTail*4.0||openHatTail<10.0){log("FAIL: open hat did not have a clearly longer tail than closed hat");return 20;}

  VoiceDSP synth;synth.prepare(48000.0);synth.configureSynth(.1f,1000.0f,1.0f,100.0f,20000.0f,0.0f,0,0,false,0.0f,0.0f,0,false);synth.synthNoteOn(60,1.0f);
  int firstCrossing=-1,lastCrossing=-1,crossings=0;float previous=0.0f;
  for(int i=0;i<48000;i++){const float sample=synth.processSynth();if(previous<=0.0f&&sample>0.0f){if(firstCrossing<0)firstCrossing=i;lastCrossing=i;++crossings;}previous=sample;}
  const double measuredHz=(crossings>1)?(crossings-1)*48000.0/(lastCrossing-firstCrossing):0.0;
  if(std::abs(measuredHz-261.625565)>1.5){log("FAIL: MIDI note 60 is not tuned to C4");return 4;}
  synth.configureSynth(.1f,1000.0f,1.0f,100.0f,20000.0f,.2f,1,2,true,.7f,50.0f,2,true);synth.synthNoteOn(127,1.0f);
  for(int i=0;i<4096;i++)if(!std::isfinite(synth.processSynth())){log("FAIL: high synth notes destabilized oscillator/filter");return 8;}

  auto rhythm=std::make_unique<DrumEngine>();rhythm->prepare(48000.0);rhythm->setHost(120.0,false);rhythm->pattern(0).length=64;rhythm->setTrackLength(0,0,3);rhythm->setTrackLength(0,1,5);rhythm->setPreviewPlaying(true);juce::AudioBuffer<float> rhythmAudio(2,36001);rhythm->process(rhythmAudio);
  if(rhythm->getTrackStep(0)!=0||rhythm->getTrackStep(1)!=1){log("FAIL: independent track loops did not advance at separate lengths");return 5;}
  auto triplets=std::make_unique<DrumEngine>();triplets->prepare(48000.0);triplets->setHost(120.0,false);triplets->pattern(0).length=64;triplets->setTripletSubdivision(true);triplets->setPreviewPlaying(true);juce::AudioBuffer<float> tripletAudio(2,24000);triplets->process(tripletAudio);if(triplets->getCurrentStep()!=5){log("FAIL: triplet grid did not advance six divisions per beat");return 18;}
  auto preview=std::make_unique<DrumEngine>();preview->prepare(48000.0);preview->setHost(120.0,false);preview->setReverbEnabled(false);preview->setDelayEnabled(false);preview->previewSynthNote(60,1.0f);juce::AudioBuffer<float> previewAudio(2,129600);preview->process(previewAudio);double previewTail=0;for(int c=0;c<2;c++)for(int i=110400;i<129600;i++)previewTail+=std::abs(previewAudio.getSample(c,i));if(previewTail>.1){log("FAIL: synth click preview did not end after its short gate and release tail");return 19;}
  auto quickKey=std::make_unique<DrumEngine>();quickKey->prepare(48000.0);quickKey->setHost(120.0,false);quickKey->requestSynthNoteOn(60,1.0f);quickKey->requestSynthNoteOff(60);juce::AudioBuffer<float> quickKeyAudio(2,512);quickKey->process(quickKeyAudio);double quickKeyEnergy=0;for(int c=0;c<2;c++)for(int i=0;i<512;i++)quickKeyEnergy+=std::abs(quickKeyAudio.getSample(c,i));if(quickKeyEnergy<.1){log("FAIL: rapid GUI synth note-on/note-off produced no audio");return 25;}
  auto filteredEnergy=[](float position){DrumEngine engine;engine.prepare(48000.0);engine.setMasterFilter(position);engine.setHost(120.0,false);engine.trigger(0,1.0f);juce::AudioBuffer<float> audio(2,8192);engine.process(audio);double energy=0;for(int c=0;c<2;c++)for(int i=0;i<audio.getNumSamples();i++)energy+=std::abs(audio.getSample(c,i));return energy;};const double filterNeutral=filteredEnergy(.5f),filterHighPass=filteredEnergy(1.0f);if(!(filterNeutral>filterHighPass*1.5)){log("FAIL: master DJ high-pass did not change the signal");return 26;}

  auto drummer=std::make_unique<DrumEngine>();drummer->prepare(48000.0);drummer->setVirtualDrummerSettings(1,2,.65f,.2f,.25f,4,4,8);drummer->setVirtualDrummer(true);
  auto&electronic=drummer->pattern(0);for(int s:{0,4,8,12})if(!electronic.step[0][s].on){log("FAIL: electronic drummer lost four-on-the-floor kick anchors");return 9;}
  for(int s:{4,12})if(!electronic.step[1][s].on){log("FAIL: virtual drummer lost backbeat anchors");return 12;}
  auto before=electronic.step;drummer->setVirtualDrummerSettings(1,2,.65f,.9f,.8f,4,4,8);if(drummer->pattern(0).step[0][0].on!=before[0][0].on||drummer->pattern(0).step[1][4].on!=before[1][4].on){log("FAIL: drummer controls destabilized core groove");return 13;}

  engine->pattern(0).length=64;engine->pattern(0).step[11][63].note=72;engine->pattern(0).step[11][63].on=true;engine->setTrackLength(0,0,7);engine->setTripletSubdivision(true);engine->setSynthTranspose(-2);engine->masterMid=.62f;engine->setSongEntry(0,0,0,8);engine->setSongEntry(1,1,2,3);engine->gain[0]=.42f;engine->panorama[0]=-.25f;
  auto state=engine->saveState();auto restored=std::make_unique<DrumEngine>();restored->prepare(48000.0);restored->loadState(state);
  if(std::abs(restored->gain[0]-.42f)>.001f||std::abs(restored->panorama[0]+.25f)>.001f||restored->pattern(0).step[11][63].note!=72||restored->pattern(0).length!=64||restored->pattern(0).trackLength[0]!=7||!restored->isTripletSubdivision()||restored->getSynthTranspose()!=-2||std::abs(restored->masterMid-.62f)>.001f){log("FAIL: engine state did not restore");return 6;}
  if(restored->getSongLength()!=2||restored->getSongEntry(0).bars!=8||restored->getSongEntry(1).pattern!=1||restored->getSongEntry(1).section!=2){log("FAIL: song chain did not restore section lengths");return 7;}

  auto processor=std::make_unique<JerzyDrumMachineAudioProcessor>();processor->prepareToPlay(48000.0,512);juce::AudioBuffer<float> pluginAudio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);processor->processBlock(pluginAudio,midi);double pluginEnergy=0;for(int i=0;i<pluginAudio.getNumSamples();i++)pluginEnergy+=std::abs(pluginAudio.getSample(0,i))+std::abs(pluginAudio.getSample(1,i));if(pluginEnergy<.01){log("FAIL: plug-in did not play MIDI channel 1 C4");return 14;}
  std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());auto visibleChildren=[&]{int count=0;for(int i=0;i<editor->getNumChildComponents();++i)if(editor->getChildComponent(i)->isVisible())++count;return count;};if(visibleChildren()<6||visibleChildren()>100){log("FAIL: editor did not isolate the initial page controls");return 15;}
  bool joinedBanks=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(editor->getChildComponent(i));button&&button->getComponentID()=="joinBanks64"){joinedBanks=true;break;}if(auto*editorView=dynamic_cast<JerzyDrumMachineAudioProcessorEditor*>(editor.get()))editorView->joinBanksTo64Steps();else joinedBanks=false;if(!joinedBanks||processor->engine.pattern(0).length!=64){log("FAIL: GUI bank join did not make a 64-step pattern");return 21;}for(int v=0;v<DrumEngine::voices;v++)if(processor->engine.pattern(0).trackLength[(size_t)v]!=64){log("FAIL: joined 64-step pattern did not extend default instrument tracks");return 22;}
  for(const auto*pageName:{"SEQ","SOUND","MIX","FX","DRUMMER","SONG"}){bool clicked=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(editor->getChildComponent(i));button&&button->getButtonText()==pageName){button->triggerClick();clicked=true;break;}if(!clicked||visibleChildren()>100){log("FAIL: page navigation exposed overlapping layouts");return 16;}}
  auto auditionProcessor=std::make_unique<JerzyDrumMachineAudioProcessor>();auditionProcessor->prepareToPlay(48000.0,512);std::unique_ptr<juce::AudioProcessorEditor> auditionEditor(auditionProcessor->createEditor());bool synthClicked=false;for(int i=0;i<auditionEditor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(auditionEditor->getChildComponent(i));button&&button->getButtonText()=="SYNTH"){button->triggerClick();synthClicked=true;break;}if(!synthClicked){log("FAIL: could not click synth preview in the editor");return 23;}juce::AudioBuffer<float> auditionAudio(2,512);juce::MidiBuffer noMidi;double auditionEnergy=0;for(int block=0;block<48;block++){auditionProcessor->processBlock(auditionAudio,noMidi);for(int c=0;c<2;c++)for(int i=0;i<auditionAudio.getNumSamples();i++)auditionEnergy+=std::abs(auditionAudio.getSample(c,i));}if(auditionEnergy<1.0){log("FAIL: synth GUI audition produced no plug-in output");return 24;}
  log("PASS: analog/digital controls audible, open/closed hat tails distinct, short synth preview and GUI synth audition audible, octave tuning and C4 stable, mixer meters active, 64-step bank join and triplet state work, plug-in MIDI audible, page layouts isolated, drummer anchors stable, polyrhythm advances independently, state round-trip");return 0;
 }
 catch(const std::exception& error){log(std::string("FAIL exception: ")+error.what());return 10;}
 catch(...){log("FAIL unknown exception");return 11;}
}

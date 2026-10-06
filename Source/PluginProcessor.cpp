#include "PluginProcessor.h"
#include "PluginEditor.h"
JerzyDrumMachineAudioProcessor::JerzyDrumMachineAudioProcessor()
 : AudioProcessor(BusesProperties()
      .withOutput("Master", juce::AudioChannelSet::stereo(), true)
      .withOutput("Kick", juce::AudioChannelSet::stereo(), false)
      .withOutput("Snare", juce::AudioChannelSet::stereo(), false)
      .withOutput("Tom", juce::AudioChannelSet::stereo(), false)
      .withOutput("Metal Hat", juce::AudioChannelSet::stereo(), false)
      .withOutput("FM Perc", juce::AudioChannelSet::stereo(), false)
      .withOutput("Phase Perc", juce::AudioChannelSet::stereo(), false)
      .withOutput("Wave Metal", juce::AudioChannelSet::stereo(), false)
      .withOutput("Noise Reso", juce::AudioChannelSet::stereo(), false)
      .withOutput("Sample 1", juce::AudioChannelSet::stereo(), false)
      .withOutput("Sample 2", juce::AudioChannelSet::stereo(), false)
      .withOutput("Sample 3", juce::AudioChannelSet::stereo(), false)
      .withOutput("Synth", juce::AudioChannelSet::stereo(), false)),
   apvts(*this, nullptr, "STATE", layout()) {}
juce::AudioProcessorValueTreeState::ParameterLayout JerzyDrumMachineAudioProcessor::layout(){
 std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
 p.push_back(std::make_unique<juce::AudioParameterFloat>("drive","Master Drive",juce::NormalisableRange<float>(0.5f,3.0f),1.15f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("swing","Swing",0.0f,0.75f,0.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("density","Generate Density",0.0f,1.0f,0.5f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("variation","Generate Variation",0.0f,1.0f,0.25f));
 for(int i=0;i<12;i++){
  auto prefix="ch"+juce::String(i)+"_";
  auto voice="v"+juce::String(i)+"_";
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"gain","Ch "+juce::String(i+1)+" Level",0.0f,1.5f,1.0f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"pan","Ch "+juce::String(i+1)+" Pan",-1.0f,1.0f,0.0f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"filter","Ch "+juce::String(i+1)+" Filter",0.0f,1.0f,0.0f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"drive","Ch "+juce::String(i+1)+" Drive",0.0f,1.0f,0.0f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"rev","Ch "+juce::String(i+1)+" Reverb Send",0.0f,1.0f,0.1f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(prefix+"del","Ch "+juce::String(i+1)+" Delay Send",0.0f,1.0f,0.0f));
  p.push_back(std::make_unique<juce::AudioParameterBool>(prefix+"mute","Ch "+juce::String(i+1)+" Mute",false));
  p.push_back(std::make_unique<juce::AudioParameterBool>(prefix+"solo","Ch "+juce::String(i+1)+" Solo",false));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(voice+"tune","Voice "+juce::String(i+1)+" Tune",0.0f,1.0f,0.5f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(voice+"decay","Voice "+juce::String(i+1)+" Decay",0.0f,1.0f,0.5f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(voice+"tone","Voice "+juce::String(i+1)+" Tone",0.0f,1.0f,0.5f));
  p.push_back(std::make_unique<juce::AudioParameterFloat>(voice+"character","Voice "+juce::String(i+1)+" Character",0.0f,1.0f,0.5f));
 }
 p.push_back(std::make_unique<juce::AudioParameterFloat>("rev_size","Reverb Size",0.0f,1.0f,0.45f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("rev_damp","Reverb Damping",0.0f,1.0f,0.55f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_beats","Delay Time Beats",juce::NormalisableRange<float>(0.125f,2.0f,0.125f),0.75f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_fb","Delay Feedback",0.0f,0.88f,0.36f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_mix","Delay Mix",0.0f,1.0f,0.45f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_bass","Master Bass",0.4f,1.8f,1.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_treble","Master Treble",0.4f,1.8f,1.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_comp","Master Compressor",0.6f,3.0f,1.35f));
 return {p.begin(),p.end()};
}
void JerzyDrumMachineAudioProcessor::prepareToPlay(double s,int){engine.prepare(s);}
bool JerzyDrumMachineAudioProcessor::isBusesLayoutSupported(const BusesLayout&l)const{
 if(l.getMainOutputChannelSet()!=juce::AudioChannelSet::stereo())return false;
 for(auto const& bus:l.outputBuses)if(!bus.isDisabled()&&bus!=juce::AudioChannelSet::stereo())return false;
 return true;
}
void JerzyDrumMachineAudioProcessor::processBlock(juce::AudioBuffer<float>&b,juce::MidiBuffer&m){
 juce::ScopedNoDenormals no;
 auto param=[this](const juce::String&id){if(auto*v=apvts.getRawParameterValue(id))return v->load();return 0.0f;};
 engine.drive=param("drive");engine.setSwing(param("swing"));
 for(int i=0;i<12;i++){
  auto prefix="ch"+juce::String(i)+"_";auto voice="v"+juce::String(i)+"_";
  engine.gain[i]=param(prefix+"gain");engine.panorama[i]=param(prefix+"pan");engine.channelFilter[i]=param(prefix+"filter");engine.channelDrive[i]=param(prefix+"drive");engine.reverbSend[i]=param(prefix+"rev");engine.delaySend[i]=param(prefix+"del");engine.mute[i]=param(prefix+"mute")>.5f;engine.solo[i]=param(prefix+"solo")>.5f;
  engine.setVoiceParam(i,0,param(voice+"tune"));engine.setVoiceParam(i,1,param(voice+"decay"));engine.setVoiceParam(i,2,param(voice+"tone"));engine.setVoiceParam(i,3,param(voice+"character"));
 }
 engine.setReverb(param("rev_size"),param("rev_damp"));engine.setDelay(param("delay_beats"),param("delay_fb"),param("delay_mix"));engine.setMaster(param("master_bass"),param("master_treble"),param("master_comp"));
 double bpm=120; bool playing=true; if(auto*ph=getPlayHead()){if(auto pos=ph->getPosition()){if(auto v=pos->getBpm())bpm=*v;playing=pos->getIsPlaying();if(auto q=pos->getPpqPosition())engine.setHostPpq(*q);}}
 engine.setHost(bpm,playing);
 for(const auto meta:m){auto msg=meta.getMessage();if(msg.isNoteOn()){int note=msg.getNoteNumber();if(note>=36&&note<48)engine.trigger(note-36,msg.getFloatVelocity());else if(note>=60&&note<92)engine.requestPattern(note-60);}}
 auto master=getBusBuffer(b,false,0);
 std::array<juce::AudioBuffer<float>,DrumEngine::voices> stemViews;
 std::array<juce::AudioBuffer<float>*,DrumEngine::voices> stems{};
 for(int i=0;i<DrumEngine::voices;i++){stemViews[(size_t)i]=getBusBuffer(b,false,i+1);stems[(size_t)i]=&stemViews[(size_t)i];}
 engine.process(master,&stems); m.clear();
}
void JerzyDrumMachineAudioProcessor::getStateInformation(juce::MemoryBlock&d){auto state=apvts.copyState();state.addChild(engine.saveState(),-1,nullptr);auto xml=state.createXml();copyXmlToBinary(*xml,d);}
void JerzyDrumMachineAudioProcessor::setStateInformation(const void*d,int n){if(auto x=getXmlFromBinary(d,n)){auto state=juce::ValueTree::fromXml(*x);if(auto eng=state.getChildWithName("ENGINE");eng.isValid())engine.loadState(eng);apvts.replaceState(state);}}
void JerzyDrumMachineAudioProcessor::syncParametersFromEngine(){
 auto set=[this](const juce::String&id,float value){if(auto*par=apvts.getParameter(id))par->setValueNotifyingHost(par->convertTo0to1(value));};
 set("drive",engine.drive);
 for(int i=0;i<12;i++){auto prefix="ch"+juce::String(i)+"_";auto voice="v"+juce::String(i)+"_";set(prefix+"gain",engine.gain[i]);set(prefix+"pan",engine.panorama[i]);set(prefix+"filter",engine.channelFilter[i]);set(prefix+"drive",engine.channelDrive[i]);set(prefix+"rev",engine.reverbSend[i]);set(prefix+"del",engine.delaySend[i]);set(prefix+"mute",engine.mute[i]?1.0f:0.0f);set(prefix+"solo",engine.solo[i]?1.0f:0.0f);set(voice+"tune",engine.getVoiceParam(i,0));set(voice+"decay",engine.getVoiceParam(i,1));set(voice+"tone",engine.getVoiceParam(i,2));set(voice+"character",engine.getVoiceParam(i,3));}
 set("rev_size",engine.reverbSize);set("rev_damp",engine.reverbDamping);set("delay_beats",engine.delayBeats);set("delay_fb",engine.delayFeedback);set("delay_mix",engine.delayMix);set("master_bass",engine.masterBass);set("master_treble",engine.masterTreble);set("master_comp",engine.masterComp);
}
juce::AudioProcessorEditor* JerzyDrumMachineAudioProcessor::createEditor(){return new JerzyDrumMachineAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new JerzyDrumMachineAudioProcessor();}

#include "PluginProcessor.h"
#include "PluginEditor.h"
JerzyDrumMachineAudioProcessor::JerzyDrumMachineAudioProcessor()
 : AudioProcessor(BusesProperties()
      .withOutput("Master", juce::AudioChannelSet::stereo(), true)
      .withOutput("Kick", juce::AudioChannelSet::stereo(), false)
      .withOutput("Snare", juce::AudioChannelSet::stereo(), false)
      .withOutput("Tom", juce::AudioChannelSet::stereo(), false)
      .withOutput("Closed Hat", juce::AudioChannelSet::stereo(), false)
      .withOutput("FM Perc", juce::AudioChannelSet::stereo(), false)
      .withOutput("Open Hat", juce::AudioChannelSet::stereo(), false)
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
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_attack","Synth Attack ms",juce::NormalisableRange<float>(0.1f,1000.0f,.1f,.35f),8.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_decay","Synth Decay ms",juce::NormalisableRange<float>(1.0f,3000.0f,.1f,.4f),180.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_sustain","Synth Sustain",0.0f,1.0f,.72f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_release","Synth Release ms",juce::NormalisableRange<float>(1.0f,5000.0f,.1f,.4f),140.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_cutoff","Synth Filter Cutoff Hz",juce::NormalisableRange<float>(20.0f,20000.0f,.1f,.25f),12000.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_resonance","Synth Filter Resonance",0.0f,.98f,.12f));
 p.push_back(std::make_unique<juce::AudioParameterChoice>("synth_wave1","Synth Oscillator 1",juce::StringArray{"SINE","SAW","SQUARE","TRIANGLE"},0));
 p.push_back(std::make_unique<juce::AudioParameterChoice>("synth_wave2","Synth Oscillator 2",juce::StringArray{"SINE","SAW","SQUARE","TRIANGLE"},1));
 p.push_back(std::make_unique<juce::AudioParameterBool>("synth_osc2","Synth Oscillator 2 On",true));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_osc2mix","Synth Oscillator Mix",0.0f,1.0f,.35f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("synth_detune","Synth Oscillator Detune cents",-50.0f,50.0f,7.0f));
 p.push_back(std::make_unique<juce::AudioParameterInt>("synth_octave","Synth Oscillator Octave",-2,2,0));
 p.push_back(std::make_unique<juce::AudioParameterChoice>("synth_transpose","Synth Keyboard Octave",juce::StringArray{"-2 OCT","-1 OCT","0 OCT","+1 OCT","+2 OCT"},2));
 p.push_back(std::make_unique<juce::AudioParameterChoice>("seq_division","Sequencer Division",juce::StringArray{"1/16","1/16 TRIPLET"},0));
 p.push_back(std::make_unique<juce::AudioParameterBool>("synth_filter","Synth Filter On",true));
 p.push_back(std::make_unique<juce::AudioParameterBool>("artifact_enable","Seed Randomize",false));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("artifact_amount","Artifact Amount",0.0f,1.0f,.25f));
 p.push_back(std::make_unique<juce::AudioParameterInt>("artifact_seed","Artifact Seed",1,999999,70271));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("rev_size","Reverb Size",0.0f,1.0f,0.45f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("rev_damp","Reverb Damping",0.0f,1.0f,0.55f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("rev_mix","Reverb Return",0.0f,1.0f,0.35f));
 p.push_back(std::make_unique<juce::AudioParameterBool>("reverb_on","Reverb On",true));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_beats","Delay Time Beats",juce::NormalisableRange<float>(0.125f,2.0f,0.125f),0.75f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_fb","Delay Feedback",0.0f,0.88f,0.36f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("delay_mix","Delay Mix",0.0f,1.0f,0.45f));
 p.push_back(std::make_unique<juce::AudioParameterBool>("delay_on","Delay On",true));
 p.push_back(std::make_unique<juce::AudioParameterBool>("delay_pingpong","Delay Ping Pong",true));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_bass","Isolator Low",0.0f,1.8f,1.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_mid","Isolator Mid",0.0f,1.8f,1.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_treble","Isolator High",0.0f,1.8f,1.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("master_comp","Master Compressor",0.6f,3.0f,1.35f));
 p.push_back(std::make_unique<juce::AudioParameterBool>("comp_on","Compressor On",true));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("comp_threshold","Compressor Threshold dB",-36.0f,0.0f,-18.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("comp_ratio","Compressor Ratio",juce::NormalisableRange<float>(1.0f,20.0f),3.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("comp_attack","Compressor Attack ms",juce::NormalisableRange<float>(0.1f,100.0f),10.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("comp_release","Compressor Release ms",juce::NormalisableRange<float>(10.0f,500.0f),120.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("comp_boost","Compressor Input Boost dB",0.0f,24.0f,0.0f));
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
 engine.setReverb(param("rev_size"),param("rev_damp"),param("rev_mix"));engine.setReverbEnabled(param("reverb_on")>.5f);
 engine.setDelay(param("delay_beats"),param("delay_fb"),param("delay_mix"));engine.setDelayEnabled(param("delay_on")>.5f);engine.setDelayPingPong(param("delay_pingpong")>.5f);
 engine.setMaster(param("master_bass"),param("master_mid"),param("master_treble"),param("master_comp"));
 engine.setTripletSubdivision((int)param("seq_division")==1);engine.setSynthTranspose((int)param("synth_transpose")-2);engine.setCompressor(param("comp_on")>.5f,param("comp_threshold"),param("comp_ratio"),param("comp_attack"),param("comp_release"));
 engine.setCompressorBoost(param("comp_boost"));
 engine.configureSynth(param("synth_attack"),param("synth_decay"),param("synth_sustain"),param("synth_release"),param("synth_cutoff"),param("synth_resonance"),(int)param("synth_wave1"),(int)param("synth_wave2"),param("synth_osc2")>.5f,param("synth_osc2mix"),param("synth_detune"),(int)param("synth_octave"),param("synth_filter")>.5f);
 engine.setArtifactSettings(param("artifact_enable")>.5f,param("artifact_amount"),(uint32_t)param("artifact_seed"));
 double bpm=120; bool playing=true; if(auto*ph=getPlayHead()){if(auto pos=ph->getPosition()){if(auto v=pos->getBpm())bpm=*v;playing=pos->getIsPlaying();if(auto q=pos->getPpqPosition())engine.setHostPpq(*q);}}
 engine.setHost(bpm,playing);
 for(const auto meta:m){auto msg=meta.getMessage();const int note=msg.getNoteNumber();if(msg.isNoteOff()){if(msg.getChannel()==2||(msg.getChannel()==1&&(note<36||note>=48)))engine.releaseSynthNote(note);}else if(msg.isNoteOn()){if(msg.getChannel()==2)engine.triggerSynthNote(note,msg.getFloatVelocity());else if(msg.getChannel()==16&&note>=60&&note<92)engine.requestPattern(note-60);else if(msg.getChannel()==1&&note>=36&&note<48)engine.trigger(note-36,msg.getFloatVelocity());else engine.triggerSynthNote(note,msg.getFloatVelocity());}}
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
 set("rev_size",engine.reverbSize);set("rev_damp",engine.reverbDamping);set("rev_mix",engine.reverbMix);set("reverb_on",engine.reverbEnabled?1.0f:0.0f);
 set("delay_beats",engine.delayBeats);set("delay_fb",engine.delayFeedback);set("delay_mix",engine.delayMix);set("delay_on",engine.delayEnabled?1.0f:0.0f);set("delay_pingpong",engine.delayPingPong?1.0f:0.0f);
 set("master_bass",engine.masterBass);set("master_mid",engine.masterMid);set("master_treble",engine.masterTreble);set("master_comp",engine.masterComp);
 set("comp_on",engine.compressorEnabled?1.0f:0.0f);set("comp_threshold",engine.compressorThresholdDb);set("comp_ratio",engine.compressorRatio);set("comp_attack",engine.compressorAttackMs);set("comp_release",engine.compressorReleaseMs);
 set("comp_boost",engine.compressorBoostDb);
 set("artifact_enable",engine.isSeedRandomizeEnabled()?1.0f:0.0f);set("artifact_amount",engine.getArtifactAmount());set("artifact_seed",(float)engine.getArtifactSeed());
 set("synth_attack",engine.synthAttackMs);set("synth_decay",engine.synthDecayMs);set("synth_sustain",engine.synthSustain);set("synth_release",engine.synthReleaseMs);set("synth_cutoff",engine.synthCutoff);set("synth_resonance",engine.synthResonance);set("synth_wave1",(float)engine.synthWave1);set("synth_wave2",(float)engine.synthWave2);set("synth_osc2",engine.synthOsc2?1.0f:0.0f);set("synth_osc2mix",engine.synthOsc2Mix);set("synth_detune",engine.synthDetuneCents);set("synth_octave",(float)engine.synthOctave2);set("synth_transpose",(float)(engine.getSynthTranspose()+2));set("seq_division",engine.isTripletSubdivision()?1.0f:0.0f);set("synth_filter",engine.synthFilter?1.0f:0.0f);
}
juce::AudioProcessorEditor* JerzyDrumMachineAudioProcessor::createEditor(){return new JerzyDrumMachineAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new JerzyDrumMachineAudioProcessor();}

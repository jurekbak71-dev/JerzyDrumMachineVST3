#include "PluginProcessor.h"
#include "PluginEditor.h"
JerzyDrumMachineAudioProcessor::JerzyDrumMachineAudioProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),apvts(*this,nullptr,"STATE",layout()){}
juce::AudioProcessorValueTreeState::ParameterLayout JerzyDrumMachineAudioProcessor::layout(){
 std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
 p.push_back(std::make_unique<juce::AudioParameterFloat>("drive","Master Drive",juce::NormalisableRange<float>(0.5f,3.0f),1.15f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("swing","Swing",0.0f,0.75f,0.0f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("density","Generate Density",0.0f,1.0f,0.5f));
 p.push_back(std::make_unique<juce::AudioParameterFloat>("variation","Generate Variation",0.0f,1.0f,0.25f));
 return {p.begin(),p.end()};
}
void JerzyDrumMachineAudioProcessor::prepareToPlay(double s,int){engine.prepare(s);}
bool JerzyDrumMachineAudioProcessor::isBusesLayoutSupported(const BusesLayout&l)const{return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();}
void JerzyDrumMachineAudioProcessor::processBlock(juce::AudioBuffer<float>&b,juce::MidiBuffer&m){
 juce::ScopedNoDenormals no; engine.drive=apvts.getRawParameterValue("drive")->load(); engine.setSwing(apvts.getRawParameterValue("swing")->load());
 double bpm=120; bool playing=true; if(auto*ph=getPlayHead()){if(auto pos=ph->getPosition()){if(auto v=pos->getBpm())bpm=*v;playing=pos->getIsPlaying();}}
 engine.setHost(bpm,playing);
 for(const auto meta:m){auto msg=meta.getMessage();if(msg.isNoteOn()){int note=msg.getNoteNumber();if(note>=36&&note<48)engine.trigger(note-36,msg.getFloatVelocity());else if(note>=60&&note<92)engine.setPattern(note-60);}}
 engine.process(b); m.clear();
}
void JerzyDrumMachineAudioProcessor::getStateInformation(juce::MemoryBlock&d){auto xml=apvts.copyState().createXml();copyXmlToBinary(*xml,d);}
void JerzyDrumMachineAudioProcessor::setStateInformation(const void*d,int n){if(auto x=getXmlFromBinary(d,n))apvts.replaceState(juce::ValueTree::fromXml(*x));}
juce::AudioProcessorEditor* JerzyDrumMachineAudioProcessor::createEditor(){return new JerzyDrumMachineAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new JerzyDrumMachineAudioProcessor();}

#pragma once
#include <JuceHeader.h>
#include "DrumEngine.h"
class JerzyDrumMachineAudioProcessor : public juce::AudioProcessor {
public:
 JerzyDrumMachineAudioProcessor(); ~JerzyDrumMachineAudioProcessor() override=default;
 void prepareToPlay(double,int)override; void releaseResources()override{}; bool isBusesLayoutSupported(const BusesLayout&)const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
 juce::AudioProcessorEditor* createEditor()override; bool hasEditor()const override{return true;}
 const juce::String getName()const override{return "Jerzy Drum Machine";} bool acceptsMidi()const override{return true;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 2.0;} int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;} void setCurrentProgram(int)override{}; const juce::String getProgramName(int)override{return{};} void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override; void setStateInformation(const void*,int)override; void syncParametersFromEngine();
 DrumEngine engine; juce::AudioProcessorValueTreeState apvts;
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};

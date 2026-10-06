#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class JerzyDrumMachineAudioProcessorEditor:public juce::AudioProcessorEditor,private juce::Timer{
public: explicit JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&); void paint(juce::Graphics&)override; void resized()override;
private: void timerCallback()override{repaint();} JerzyDrumMachineAudioProcessor&p; int page=0,selected=0;
 juce::TextButton seq{"SEQ"},sound{"SOUND"},mix{"MIX"},generate{"GENERATE"},mutate{"MUTATE"},fill{"FILL"};
 std::array<juce::TextButton,12> instruments; std::array<juce::TextButton,16> steps; juce::TextButton loadSample{"LOAD WAV"}; std::unique_ptr<juce::FileChooser> chooser; std::array<juce::Slider,12> channelGain,channelPan,revSend,delSend; juce::Slider masterDrive;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyDrumMachineAudioProcessorEditor)
};

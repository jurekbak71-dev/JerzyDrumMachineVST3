#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class JerzyDrumMachineAudioProcessorEditor:public juce::AudioProcessorEditor,private juce::Timer{
public: explicit JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&); void paint(juce::Graphics&)override; void resized()override;
private: void timerCallback()override{repaint();} JerzyDrumMachineAudioProcessor&p; int page=0,selected=0;
 juce::TextButton seq{"SEQ"},sound{"SOUND"},mix{"MIX"},generate{"GENERATE"},mutate{"MUTATE"},fill{"FILL"};
 std::array<juce::TextButton,12> instruments; std::array<juce::TextButton,16> steps; std::array<juce::TextButton,4> banks; juce::TextButton loadSample{"LOAD WAV"}; std::unique_ptr<juce::FileChooser> chooser; std::array<juce::Slider,12> channelGain,channelPan,revSend,delSend; juce::Slider masterDrive; std::array<juce::Slider,4> soundParam; juce::Slider stepVelocity,stepProbability,stepRatchet; juce::ToggleButton stepAccent{"ACCENT"},stepFlam{"FLAM"}; juce::Slider patternLength,stepMicro; juce::ComboBox patternSelect,changeMode; std::array<juce::Slider,8> genParam; juce::TextButton saveKit{"SAVE KIT"},loadKit{"LOAD KIT"}; std::unique_ptr<juce::FileChooser> kitChooser; int bank=0,selectedStep=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyDrumMachineAudioProcessorEditor)
};

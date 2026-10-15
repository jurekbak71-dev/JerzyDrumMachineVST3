#pragma once
#include <JuceHeader.h>
#include "ComposerEngine.h"
#include <array>
#include <limits>

class JerzyComposerProcessor final: public juce::AudioProcessor {
public:
    JerzyComposerProcessor();
    ~JerzyComposerProcessor() override = default;
    void prepareToPlay(double sampleRate,int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
    }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override {return true;}
    const juce::String getName() const override {return "Jerzy Composer";}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return true;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {return 0.0;}
    int getNumPrograms() override {return 1;}
    int getCurrentProgram() override {return 0;}
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override {return {};}
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioProcessorValueTreeState state;
    juce::MidiBuffer output;
    std::array<double,128> activeUntil{};
    std::array<bool,128> held{};
    double rate=44100.0,localPpq=0.0,lastEndPpq=std::numeric_limits<double>::quiet_NaN();
    bool wasPlaying=false;
    void stopNotes(int channel,juce::MidiBuffer& target,int sampleOffset=0) noexcept;
    void fire(int note,int velocity,int channel,double atPpq,double lengthPpq,
              double startPpq,double endPpq,double samplesPerQuarter,int numSamples);
    float get(const char* name) const noexcept {
        auto* p=state.getRawParameterValue(name);
        return p?p->load():0.0f;
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyComposerProcessor)
};

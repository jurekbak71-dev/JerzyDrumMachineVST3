#pragma once
#include <JuceHeader.h>
#include "ComposerEngine.h"
#include <array>
#include <limits>

class JerzyComposerProcessor final : public juce::AudioProcessor {
public:
    JerzyComposerProcessor();
    ~JerzyComposerProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo();
    }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Jerzy Composer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorValueTreeState& parameters() noexcept { return state; }
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::AudioProcessorValueTreeState state;
    juce::MidiBuffer output;
    std::array<double,128> activeUntil{};
    std::array<bool,128> held{};
    double rate=44100., localPpq=0., lastEndPpq=std::numeric_limits<double>::quiet_NaN();
    int lastChannel=1;
    bool wasPlaying=false;
    void stopNotes(juce::MidiBuffer& target, int offset=0) noexcept;
    void fire(const jerzy_composer::NoteEvent&, int channel, double absoluteBeat,
              double startPpq, double endPpq, double samplesPerQuarter, int samples);
    jerzy_composer::Settings settings(double barLength) const noexcept;
    float get(const char* id) const noexcept {
        if(auto* p=state.getRawParameterValue(id))return p->load();
        return 0.f;
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyComposerProcessor)
};

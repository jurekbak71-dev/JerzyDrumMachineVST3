#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
class JerzyComposerProcessor;

class JerzyComposerEditor final : public juce::AudioProcessorEditor {
public:
    explicit JerzyComposerEditor(JerzyComposerProcessor&);
    ~JerzyComposerEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Choice {
        juce::Label label;
        juce::ComboBox control;
        std::unique_ptr<ComboAttachment> attachment;
    };
    struct Control {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };
    JerzyComposerProcessor& processor;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Control>> controls;
    juce::ToggleButton enabled;
    std::unique_ptr<ButtonAttachment> enabledAttachment;
    juce::TextButton newIdea;
    juce::Label instructions;
    void addChoice(const char* param, const juce::String& title,
                   const juce::StringArray& values);
    void addControl(const char* param,const juce::String& title);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyComposerEditor)
};

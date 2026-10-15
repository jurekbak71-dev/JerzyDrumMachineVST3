#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace {
constexpr auto textColor=juce::Colour(0xffebf0f5);
constexpr auto muted=juce::Colour(0xffa7b4c2);
constexpr auto accent=juce::Colour(0xff5fd6bf);
constexpr auto dark=juce::Colour(0xff182734);
void labelStyle(juce::Label& label,bool small=false) {
    label.setColour(juce::Label::textColourId,small?muted:textColor);
    label.setFont(juce::Font(juce::FontOptions(small?12.5f:14.0f)));
    label.setJustificationType(juce::Justification::centredLeft);
}
}

JerzyComposerEditor::JerzyComposerEditor(JerzyComposerProcessor& p)
    : juce::AudioProcessorEditor(&p),processor(p) {
    // Native JUCE components scale and resize without a heavyweight browser UI.
    addChoice("mode","PARTIA",{"Perkusja","Bas","Akordy","Arpeggio","Melodia"});
    addChoice("tonic","TONIKA / DŹWIĘK",{"C","Cis / Des","D","Dis / Es","E","F",
                                     "Fis / Ges","G","Gis / As","A","Ais / B","H"});
    addChoice("octave","REJESTR / OKTAWA",{"C2 - nisko","C3","C4","C5","C6 - wysoko"});
    addChoice("scale","SKALA",{"Moll","Dur","Dorycka","Pentatonika moll",
                              "Moll harmoniczna","Frygijska","Miksolidyjska"});
    addChoice("progression","PROGRESJA",{"Jedna tonika","i - VI - III - VII","i - iv - V - i",
                                         "I - V - vi - IV","ii - V - I - vi",
                                         "i - VII - VI - VII","i - III - VII - iv"});
    addChoice("style","STYL / GROOVE",{"Rock","Dub / reggae","Acid / 303","Funk",
                                     "Ambient","House","Breakbeat","Lo-fi hip-hop"});
    addChoice("division","PODZIAŁ RYTMICZNY",{"1/8","1/16","1/16T - triole",
                                            "1/32","1/8T - triole"});
    addChoice("color","RODZAJ AKORDU",{"Trójdźwięk","Septymowy","Sus2 / Sus4","Add9"});
    addChoice("voicing","UKŁAD HARMONII",{"Blisko","Szeroko","Płynne przejścia"});
    addChoice("direction","RUCH ARPEGGIO",{"W górę","W dół","Góra / dół"});
    addControl("density","GĘSTOŚĆ NUT");
    addControl("swing","SWING / KOŁYSANIE");
    addControl("human","NATURALNOŚĆ");
    addControl("variation","WARIACJE FRAZY");
    addControl("gate","DŁUGOŚĆ NUT");
    addControl("octaves","OKTAWY ARP");
    addControl("seed","NOWY WARIANT / SEED");
    addControl("channel","KANAŁ MIDI");
    enabled.setButtonText("GENERUJ MIDI");
    enabled.setColour(juce::ToggleButton::textColourId,textColor);
    enabled.setColour(juce::ToggleButton::tickColourId,accent);
    addAndMakeVisible(enabled);
    // Keep attachment alive for the lifetime of the toggle, not temporary.
    static_assert(sizeof(ButtonAttachment)>0,"JUCE attachments available");
    enabled.onClick=[this] {
        if(auto* param=processor.parameters().getParameter("enabled")) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(enabled.getToggleState()?1.f:0.f);
            param->endChangeGesture();
        }
    };
    if(auto* state=processor.parameters().getRawParameterValue("enabled"))
        enabled.setToggleState(state->load()>=.5f,juce::dontSendNotification);
    newIdea.setButtonText("NOWY POMYSŁ");
    newIdea.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff22564f));
    newIdea.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
    newIdea.onClick=[this] {
        if(auto* param=processor.parameters().getParameter("seed")) {
            auto next=juce::Random::getSystemRandom().nextInt(1000000);
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1((float)next));
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(newIdea);
    instructions.setText("FL STUDIO: Wrapper MIDI Output port 10  →  instrument MIDI Input port 10.  "
                         "Start/Stop w FL Studio steruje generatorem.",
                         juce::dontSendNotification);
    labelStyle(instructions,true);
    instructions.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(instructions);
    setResizable(true,true);
    setResizeLimits(790,645,1280,1000);
    setSize(980,735);
}

void JerzyComposerEditor::addChoice(const char* param,const juce::String& title,
                                   const juce::StringArray& values) {
    auto c=std::make_unique<Choice>();
    c->label.setText(title,juce::dontSendNotification);
    labelStyle(c->label,true);
    c->control.addItemList(values,1);
    c->control.setColour(juce::ComboBox::backgroundColourId,dark);
    c->control.setColour(juce::ComboBox::textColourId,textColor);
    c->control.setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff354b5c));
    c->control.setColour(juce::ComboBox::arrowColourId,accent);
    addAndMakeVisible(c->label);
    addAndMakeVisible(c->control);
    c->attachment=std::make_unique<ComboAttachment>(processor.parameters(),param,c->control);
    choices.push_back(std::move(c));
}

void JerzyComposerEditor::addControl(const char* param,const juce::String& title) {
    auto c=std::make_unique<Control>();
    c->label.setText(title,juce::dontSendNotification);
    labelStyle(c->label,true);
    c->slider.setSliderStyle(juce::Slider::LinearHorizontal);
    c->slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,22);
    c->slider.setColour(juce::Slider::trackColourId,accent);
    c->slider.setColour(juce::Slider::backgroundColourId,juce::Colour(0xff334553));
    c->slider.setColour(juce::Slider::thumbColourId,juce::Colour(0xffdafff6));
    c->slider.setColour(juce::Slider::textBoxTextColourId,textColor);
    c->slider.setColour(juce::Slider::textBoxBackgroundColourId,dark);
    c->slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    addAndMakeVisible(c->label);
    addAndMakeVisible(c->slider);
    c->attachment=std::make_unique<SliderAttachment>(processor.parameters(),param,c->slider);
    controls.push_back(std::move(c));
}

void JerzyComposerEditor::paint(juce::Graphics& g) {
    const auto bounds=getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff101b26),0,0,juce::Colour(0xff1d3240),bounds.getRight(),bounds.getBottom(),false));
    g.fillRect(bounds);
    g.setColour(accent);
    g.fillRoundedRectangle(20.f,18.f,4.f,54.f,2.f);
    g.setColour(textColor);
    g.setFont(juce::Font(juce::FontOptions(28.f,juce::Font::bold)));
    g.drawText("JERZY COMPOSER",38,18,getWidth()-370,40,juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(13.f)));
    g.drawText("MIDI COMPOSER   •   v0.4 DEVELOPMENT   •   WINDOWS VST3",
               40,55,getWidth()-390,24,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff425765));
    g.drawHorizontalLine(92,20.f,float(getWidth()-20));
    g.setColour(muted);
    g.setFont(juce::Font(juce::FontOptions(12.5f,juce::Font::bold)));
    g.drawText("HARMONIA I MELODIA",24,100,getWidth()/2-36,19,juce::Justification::centredLeft);
    g.drawText("RYTM I ARANŻACJA",getWidth()/2+10,100,getWidth()/2-34,19,
               juce::Justification::centredLeft);
    const int controlTop=125;
    const int bottomStart=getHeight()-260;
    g.setColour(juce::Colour(0xff213543));
    g.fillRoundedRectangle(18.f,float(controlTop-3),float(getWidth()/2-29),
                           float(bottomStart-controlTop-8),10.f);
    g.fillRoundedRectangle(float(getWidth()/2+5),float(controlTop-3),
                           float(getWidth()/2-23),float(bottomStart-controlTop-8),10.f);
    g.setColour(muted);
    g.drawText("EKSPRESJA I PARAMETRY MIDI",24,bottomStart-27,getWidth()-48,22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff213543));
    g.fillRoundedRectangle(18.f,float(bottomStart),float(getWidth()-36),176.f,10.f);
    g.setColour(juce::Colour(0xff425765));
    g.drawHorizontalLine(getHeight()-58,20.f,float(getWidth()-20));
}

void JerzyComposerEditor::resized() {
    const int w=getWidth(),h=getHeight();
    enabled.setBounds(w-342,28,152,30);
    newIdea.setBounds(w-180,26,160,36);
    const int colGap=32;
    const int colWidth=(w-54-colGap)/2;
    const int x0=27,x1=x0+colWidth+colGap;
    const int start=131;
    const int lower=h-260;
    const int step=juce::jlimit(46,62,(lower-start-15)/5);
    for(int i=0;i<static_cast<int>(choices.size());++i) {
        auto& c=*choices[static_cast<std::size_t>(i)];
        const int column=i<5?0:1;
        const int row=i<5?i:i-5;
        const int x=column?x1:x0;
        const int y=start+row*step;
        c.label.setBounds(x,y,colWidth,18);
        c.control.setBounds(x,y+18,colWidth,juce::jmin(32,step-21));
    }
    const int bottom=h-260;
    const int knobWidth=(w-62)/4;
    for(int i=0;i<static_cast<int>(controls.size());++i) {
        auto& c=*controls[static_cast<std::size_t>(i)];
        const int x=27+(i%4)*knobWidth;
        const int y=bottom+14+(i/4)*79;
        c.label.setBounds(x,y,knobWidth-8,22);
        c.slider.setBounds(x,y+24,knobWidth-10,33);
    }
    instructions.setBounds(24,h-51,w-48,36);
}

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CopperLookAndFeel : public juce::LookAndFeel_V4 {
public:
 CopperLookAndFeel(){
  setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd7a45d));
  setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffd58a));
  setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff17110e));
  setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff7b492d));
  setColour(juce::ComboBox::textColourId, juce::Colour(0xffe1b46f));
  setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffc78345));
  setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe1b46f));
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff100d0b));
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff65402a));
 }
 void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override {
  auto r=b.getLocalBounds().toFloat().reduced(1.0f);
  const bool on=b.getToggleState();
  juce::Colour top=on?juce::Colour(0xff6d3b20):juce::Colour(0xff211713);
  juce::Colour bottom=on?juce::Colour(0xff3b2116):juce::Colour(0xff0d0b0a);
  if(over){top=top.brighter(.08f);bottom=bottom.brighter(.05f);}
  if(down){top=top.darker(.15f);bottom=bottom.darker(.1f);}
  g.setGradientFill(juce::ColourGradient(top,r.getTopLeft(),bottom,r.getBottomLeft(),false));
  g.fillRoundedRectangle(r,4.0f);
  g.setColour(on?juce::Colour(0xffd98b3f):juce::Colour(0xff6b422c));
  g.drawRoundedRectangle(r,4.0f,on?1.8f:1.0f);
  if(on){g.setColour(juce::Colour(0x44ffb34c));g.drawRoundedRectangle(r.reduced(2),3.0f,2.0f);}
 }
 void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override {
  auto b=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(8);
  float d=juce::jmin(b.getWidth(),b.getHeight());auto c=b.getCentre();juce::Rectangle<float> k(c.x-d*.5f,c.y-d*.5f,d,d);
  g.setColour(juce::Colour(0xff080706));g.fillEllipse(k.translated(2,3));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff473329),k.getTopLeft(),juce::Colour(0xff15100e),k.getBottomRight(),false));g.fillEllipse(k);
  g.setColour(juce::Colour(0xff8b5a36));g.drawEllipse(k,1.5f);
  auto a=start+pos*(end-start);juce::Path p;p.startNewSubPath(c);p.lineTo(c+juce::Point<float>(std::sin(a),-std::cos(a))*d*.38f);
  g.setColour(juce::Colour(0xffefb762));g.strokePath(p,juce::PathStrokeType(2.2f));
 }
 void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float min,float max,const juce::Slider::SliderStyle style,juce::Slider& s) override {
  if(style==juce::Slider::LinearHorizontal){
   float cy=y+h*.5f;g.setColour(juce::Colour(0xff17110e));g.fillRoundedRectangle((float)x,cy-3,(float)w,6,3);
   g.setColour(juce::Colour(0xff71442c));g.fillRoundedRectangle((float)x,cy-3,juce::jmax(0.0f,pos-x),6,3);
   g.setColour(juce::Colour(0xffd9994f));g.fillEllipse(pos-6,cy-6,12,12);return;
  }
  juce::LookAndFeel_V4::drawLinearSlider(g,x,y,w,h,pos,min,max,style,s);
 }
 void drawComboBox(juce::Graphics& g,int w,int h,bool down,int bx,int by,int bw,int bh,juce::ComboBox& box) override {
  auto r=juce::Rectangle<float>(0,0,(float)w,(float)h).reduced(1);g.setColour(juce::Colour(0xff15100d));g.fillRoundedRectangle(r,3);g.setColour(juce::Colour(0xff6f452d));g.drawRoundedRectangle(r,3,1);
  juce::Path a;a.startNewSubPath((float)bx+3,(float)by+6);a.lineTo((float)bx+bw*.5f,(float)by+bh-4);a.lineTo((float)bx+bw-3,(float)by+6);g.setColour(down?juce::Colour(0xffffcf7a):juce::Colour(0xffc98746));g.strokePath(a,juce::PathStrokeType(1.5f));box.setColour(juce::ComboBox::textColourId,juce::Colour(0xffe0b06b));
 }
 void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool over,bool down) override {
  auto r=b.getLocalBounds().toFloat().reduced(2);g.setColour(juce::Colour(0xff15100d));g.fillRoundedRectangle(r,3);g.setColour(b.getToggleState()?juce::Colour(0xffd78b3f):juce::Colour(0xff65402a));g.drawRoundedRectangle(r,3,b.getToggleState()?2.0f:1.0f);
  if(b.getToggleState()){g.setColour(juce::Colour(0xfff1b95f));g.fillEllipse(8,(float)b.getHeight()*.5f-3,6,6);}
  g.setColour(b.getToggleState()?juce::Colour(0xffffd089):juce::Colour(0xffc69a62));g.setFont(12);g.drawText(b.getButtonText(),18,0,b.getWidth()-20,b.getHeight(),juce::Justification::centredLeft);
 }
};

class JerzyDrumMachineAudioProcessorEditor:public juce::AudioProcessorEditor,private juce::Timer{
public:
 explicit JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&);
 ~JerzyDrumMachineAudioProcessorEditor() override;
 void paint(juce::Graphics&)override;
 void resized()override;
private:
 void timerCallback()override;
 void syncStepControls();
 void syncVisibleSteps();
 void syncSongSlots();
 void attachSoundParameters();
 JerzyDrumMachineAudioProcessor&p; CopperLookAndFeel copper; int page=0,selected=0;
 juce::TextButton seq{"SEQ"},sound{"SOUND"},mix{"MIX"},drummer{"DRUMMER"},songPage{"SONG"},run{"RUN"},generate{"GENERATE"},mutate{"MUTATE"},fill{"FILL"};
 std::array<juce::TextButton,12> instruments; std::array<bool,12> instrumentHover{}; std::array<juce::TextButton,16> steps; std::array<juce::TextButton,4> banks;
 juce::TextButton loadSample{"LOAD WAV"}; std::unique_ptr<juce::FileChooser> chooser; juce::ToggleButton showSynthKeyboard{"SHOW MIDI KEYBOARD"}; std::array<juce::TextButton,12> synthKeys;
 std::array<juce::Slider,12> channelGain,channelPan,channelFilter,channelDrive,revSend,delSend; std::array<juce::TextButton,12> channelMute,channelSolo; juce::Slider masterDrive; std::array<juce::Slider,8> fxParam;
 using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment; using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::array<std::unique_ptr<SliderAttachment>,12> gainAtt,panAtt,filterAtt,driveAtt,revAtt,delAtt; std::array<std::unique_ptr<ButtonAttachment>,12> muteAtt,soloAtt;
 std::array<std::unique_ptr<SliderAttachment>,4> soundAtt; std::array<std::unique_ptr<SliderAttachment>,8> fxAtt; std::unique_ptr<SliderAttachment> masterDriveAtt;
 std::array<juce::Slider,4> soundParam; juce::Slider stepVelocity,stepProbability,stepRatchet;
 juce::ToggleButton stepAccent{"ACCENT"},stepFlam{"FLAM"}; juce::Slider patternLength,stepMicro;
 juce::ComboBox patternSelect,changeMode,stepNote; std::array<juce::Slider,8> genParam;
 juce::ToggleButton drummerEnable{"AUTO DRUMMER"}; std::array<juce::Slider,3> drummerParam; juce::ComboBox drummerStyle,hatDivision,phraseLength;
 std::array<juce::TextButton,16> songSlots; juce::ComboBox songPatternSelect,songSectionSelect; juce::TextButton songAdd{"ADD / UPDATE"},songRemove{"REMOVE SLOT"},songPlay{"PLAY SONG"}; int selectedSongSlot=0;
 juce::TextButton saveKit{"SAVE KIT"},loadKit{"LOAD KIT"}; std::unique_ptr<juce::FileChooser> kitChooser;
 int bank=0,selectedStep=0;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JerzyDrumMachineAudioProcessorEditor)
};

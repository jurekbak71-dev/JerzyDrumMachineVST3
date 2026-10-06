#include "PluginEditor.h"
static const char* names[]={"KICK","SNARE","TOM","METAL HAT","FM PERC","PHASE PERC","WAVE METAL","NOISE RESO","SAMPLE 1","SAMPLE 2","SAMPLE 3","SYNTH"};

JerzyDrumMachineAudioProcessorEditor::JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&x):AudioProcessorEditor(&x),p(x){
 setLookAndFeel(&copper);setSize(1280,800);setResizable(true,true);setResizeLimits(1080,700,1920,1080);
 for(auto*b:{&seq,&sound,&mix,&run,&generate,&mutate,&fill,&loadSample,&saveKit,&loadKit})addAndMakeVisible(*b);
 seq.setClickingTogglesState(true);sound.setClickingTogglesState(true);mix.setClickingTogglesState(true);seq.setToggleState(true,juce::dontSendNotification);
 auto setPage=[this](int pg){page=pg;seq.setToggleState(pg==0,juce::dontSendNotification);sound.setToggleState(pg==1,juce::dontSendNotification);mix.setToggleState(pg==2,juce::dontSendNotification);resized();repaint();};
 seq.onClick=[setPage]{setPage(0);};sound.onClick=[setPage]{setPage(1);};mix.onClick=[setPage]{setPage(2);};
 run.setClickingTogglesState(true);run.onClick=[this]{p.engine.setPreviewPlaying(run.getToggleState());run.setButtonText(run.getToggleState()?"STOP":"RUN");};

 for(int i=0;i<12;i++){
  instruments[i].setButtonText(names[i]);instruments[i].setClickingTogglesState(true);addAndMakeVisible(instruments[i]);
  instruments[i].onStateChange=[this,i]{const bool over=instruments[i].isMouseOver();if(over&&!instrumentHover[(size_t)i])p.engine.trigger(i,.85f);instrumentHover[(size_t)i]=over;};
  instruments[i].onClick=[this,i]{p.engine.trigger(i,.85f);selected=i;for(int k=0;k<12;k++)instruments[k].setToggleState(k==selected,juce::dontSendNotification);attachSoundParameters();syncVisibleSteps();syncStepControls();resized();repaint();};
 }
 instruments[0].setToggleState(true,juce::dontSendNotification);

 for(int b=0;b<4;b++){
  banks[b].setButtonText("BANK "+juce::String(b+1));banks[b].setClickingTogglesState(true);addAndMakeVisible(banks[b]);
  banks[b].onClick=[this,b]{bank=b;for(int k=0;k<4;k++)banks[k].setToggleState(k==bank,juce::dontSendNotification);syncVisibleSteps();repaint();};
 }
 banks[0].setToggleState(true,juce::dontSendNotification);

 for(int s=0;s<16;s++){
  steps[s].setButtonText(juce::String(s+1));steps[s].setClickingTogglesState(false);addAndMakeVisible(steps[s]);
  steps[s].onClick=[this,s]{selectedStep=bank*16+s;auto&st=p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep];st.on=!st.on;steps[s].setToggleState(st.on,juce::dontSendNotification);syncStepControls();repaint();};
 }

 for(int i=0;i<8;i++){addAndMakeVisible(genParam[i]);genParam[i].setRange(0,1,.01);genParam[i].setValue(i==4?.85:(i==5?.9:(i==6?.65:(i==7?.35:.25))));genParam[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);genParam[i].setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);}
 auto updateGen=[this]{p.engine.setGenerator((float)genParam[0].getValue(),(float)genParam[1].getValue(),(float)genParam[2].getValue(),(float)genParam[3].getValue(),(float)genParam[4].getValue(),(float)genParam[5].getValue(),(float)genParam[6].getValue(),(float)genParam[7].getValue());};
 for(auto&s:genParam)s.onValueChange=updateGen;updateGen();
 generate.onClick=[this]{p.engine.generate(p.apvts.getRawParameterValue("density")->load(),p.apvts.getRawParameterValue("variation")->load());syncVisibleSteps();repaint();};
 mutate.onClick=[this]{p.engine.mutate(p.apvts.getRawParameterValue("variation")->load());syncVisibleSteps();repaint();};
 fill.onClick=[this]{p.engine.fill();syncVisibleSteps();repaint();};

 loadSample.onClick=[this]{if(selected<8||selected>10)return;chooser=std::make_unique<juce::FileChooser>("Load WAV sample",juce::File{},"*.wav;*.aif;*.aiff");auto flags=juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles;chooser->launchAsync(flags,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(file.existsAsFile())p.engine.loadSample(selected-8,file);repaint();});};

 for(int i=0;i<12;i++){
  for(auto*s:{&channelGain[i],&channelPan[i],&channelFilter[i],&channelDrive[i],&revSend[i],&delSend[i]}){addAndMakeVisible(*s);s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);}
  addAndMakeVisible(channelMute[i]);addAndMakeVisible(channelSolo[i]);channelMute[i].setButtonText("M");channelSolo[i].setButtonText("S");channelMute[i].setClickingTogglesState(true);channelSolo[i].setClickingTogglesState(true);
  channelGain[i].setRange(0,1.5,.01);channelGain[i].setValue(p.engine.gain[i]);channelGain[i].onValueChange=[this,i]{p.engine.gain[i]=(float)channelGain[i].getValue();};
  channelPan[i].setRange(-1,1,.01);channelPan[i].setValue(p.engine.panorama[i]);channelPan[i].onValueChange=[this,i]{p.engine.panorama[i]=(float)channelPan[i].getValue();};
  channelFilter[i].setRange(0,1,.01);channelFilter[i].setValue(p.engine.channelFilter[i]);channelFilter[i].onValueChange=[this,i]{p.engine.channelFilter[i]=(float)channelFilter[i].getValue();};
  channelDrive[i].setRange(0,1,.01);channelDrive[i].setValue(p.engine.channelDrive[i]);channelDrive[i].onValueChange=[this,i]{p.engine.channelDrive[i]=(float)channelDrive[i].getValue();};
  revSend[i].setRange(0,1,.01);revSend[i].setValue(p.engine.reverbSend[i]);revSend[i].onValueChange=[this,i]{p.engine.reverbSend[i]=(float)revSend[i].getValue();};
  delSend[i].setRange(0,1,.01);delSend[i].setValue(p.engine.delaySend[i]);delSend[i].onValueChange=[this,i]{p.engine.delaySend[i]=(float)delSend[i].getValue();};
  channelMute[i].setToggleState(p.engine.mute[i],juce::dontSendNotification);channelSolo[i].setToggleState(p.engine.solo[i],juce::dontSendNotification);
  channelMute[i].onClick=[this,i]{p.engine.mute[i]=channelMute[i].getToggleState();};channelSolo[i].onClick=[this,i]{p.engine.solo[i]=channelSolo[i].getToggleState();};
 }
 addAndMakeVisible(masterDrive);masterDrive.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);masterDrive.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,18);masterDrive.setRange(.5,3,.01);masterDrive.setValue(p.engine.drive);masterDrive.onValueChange=[this]{p.engine.drive=(float)masterDrive.getValue();};
 for(auto&s:fxParam){addAndMakeVisible(s);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,54,18);}
 fxParam[0].setRange(0,1,.01);fxParam[0].setValue(p.engine.reverbSize);
 fxParam[1].setRange(0,1,.01);fxParam[1].setValue(p.engine.reverbDamping);
 fxParam[2].setRange(.125,2.0,.125);fxParam[2].setValue(p.engine.delayBeats);
 fxParam[3].setRange(0,.88,.01);fxParam[3].setValue(p.engine.delayFeedback);
 fxParam[4].setRange(0,1,.01);fxParam[4].setValue(p.engine.delayMix);
 fxParam[5].setRange(.4,1.8,.01);fxParam[5].setValue(p.engine.masterBass);
 fxParam[6].setRange(.4,1.8,.01);fxParam[6].setValue(p.engine.masterTreble);
 fxParam[7].setRange(.6,3.0,.01);fxParam[7].setValue(p.engine.masterComp);
 fxParam[0].onValueChange=[this]{p.engine.setReverb((float)fxParam[0].getValue(),(float)fxParam[1].getValue());};
 fxParam[1].onValueChange=[this]{p.engine.setReverb((float)fxParam[0].getValue(),(float)fxParam[1].getValue());};
 auto updDelay=[this]{p.engine.setDelay((float)fxParam[2].getValue(),(float)fxParam[3].getValue(),(float)fxParam[4].getValue());};fxParam[2].onValueChange=updDelay;fxParam[3].onValueChange=updDelay;fxParam[4].onValueChange=updDelay;
 auto updMaster=[this]{p.engine.setMaster((float)fxParam[5].getValue(),(float)fxParam[6].getValue(),(float)fxParam[7].getValue());};fxParam[5].onValueChange=updMaster;fxParam[6].onValueChange=updMaster;fxParam[7].onValueChange=updMaster;
 for(int i=0;i<12;i++){auto prefix="ch"+juce::String(i)+"_";gainAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"gain",channelGain[i]);panAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"pan",channelPan[i]);filterAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"filter",channelFilter[i]);driveAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"drive",channelDrive[i]);revAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"rev",revSend[i]);delAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"del",delSend[i]);muteAtt[i]=std::make_unique<ButtonAttachment>(p.apvts,prefix+"mute",channelMute[i]);soloAtt[i]=std::make_unique<ButtonAttachment>(p.apvts,prefix+"solo",channelSolo[i]);}
 masterDriveAtt=std::make_unique<SliderAttachment>(p.apvts,"drive",masterDrive);
 static const char*fxIds[]={"rev_size","rev_damp","delay_beats","delay_fb","delay_mix","master_bass","master_treble","master_comp"};for(int i=0;i<8;i++)fxAtt[i]=std::make_unique<SliderAttachment>(p.apvts,fxIds[i],fxParam[i]);

 for(auto*s:{&stepVelocity,&stepProbability,&stepRatchet,&stepMicro}){addAndMakeVisible(*s);s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,56,20);}
 addAndMakeVisible(stepAccent);addAndMakeVisible(stepFlam);addAndMakeVisible(patternLength);addAndMakeVisible(patternSelect);addAndMakeVisible(changeMode);
 stepVelocity.setRange(0,1,.01);stepProbability.setRange(0,1,.01);stepRatchet.setRange(1,4,1);stepMicro.setRange(0,1,.01);
 patternLength.setSliderStyle(juce::Slider::LinearHorizontal);patternLength.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,20);patternLength.setRange(1,64,1);patternLength.setValue(p.engine.pattern(p.engine.getPattern()).length);
 changeMode.addItem("IMMEDIATE",1);changeMode.addItem("NEXT BEAT",2);changeMode.addItem("NEXT BAR",3);changeMode.addItem("END PATTERN",4);changeMode.setSelectedId((int)p.engine.getChangeMode()+1);
 for(int i=0;i<32;i++)patternSelect.addItem("PATTERN "+juce::String(i+1)+"  /  "+juce::MidiMessage::getMidiNoteName(60+i,true,true,3),i+1);patternSelect.setSelectedId(p.engine.getPattern()+1);
 stepVelocity.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].velocity=(float)stepVelocity.getValue();};
 stepProbability.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].probability=(float)stepProbability.getValue();};
 stepRatchet.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].ratchet=(int)stepRatchet.getValue();};
 stepMicro.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].micro=(float)stepMicro.getValue();};
 stepAccent.onClick=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].accent=stepAccent.getToggleState();};
 stepFlam.onClick=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].flam=stepFlam.getToggleState();};
 changeMode.onChange=[this]{p.engine.setChangeMode((DrumEngine::ChangeMode)(changeMode.getSelectedId()-1));};
 patternLength.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).length=(int)patternLength.getValue();};
 patternSelect.onChange=[this]{p.engine.setPattern(patternSelect.getSelectedId()-1);patternLength.setValue(p.engine.pattern(p.engine.getPattern()).length,juce::dontSendNotification);syncVisibleSteps();syncStepControls();repaint();};

 for(int i=0;i<4;i++){addAndMakeVisible(soundParam[i]);soundParam[i].setRange(0,1,.01);soundParam[i].setValue(p.engine.getVoiceParam(selected,i));soundParam[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);soundParam[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,56,18);soundParam[i].onValueChange=[this,i]{p.engine.setVoiceParam(selected,i,(float)soundParam[i].getValue());};}attachSoundParameters();

 saveKit.onClick=[this]{kitChooser=std::make_unique<juce::FileChooser>("Save Jerzy Drum Kit",juce::File{},"*.jdmkit");kitChooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(file.getFileExtension().isEmpty())file=file.withFileExtension(".jdmkit");if(auto xml=p.engine.saveState().createXml())xml->writeTo(file);});};
 loadKit.onClick=[this]{kitChooser=std::make_unique<juce::FileChooser>("Load Jerzy Drum Kit",juce::File{},"*.jdmkit");kitChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(auto xml=juce::XmlDocument::parse(file)){p.engine.loadState(juce::ValueTree::fromXml(*xml));p.syncParametersFromEngine();attachSoundParameters();syncVisibleSteps();syncStepControls();repaint();}});};

 syncVisibleSteps();syncStepControls();startTimerHz(20);
}

JerzyDrumMachineAudioProcessorEditor::~JerzyDrumMachineAudioProcessorEditor(){setLookAndFeel(nullptr);}

void JerzyDrumMachineAudioProcessorEditor::attachSoundParameters(){static const char* suffix[]={"tune","decay","tone","character"};for(int i=0;i<4;i++){soundAtt[i].reset();soundAtt[i]=std::make_unique<SliderAttachment>(p.apvts,"v"+juce::String(selected)+"_"+suffix[i],soundParam[i]);}}
void JerzyDrumMachineAudioProcessorEditor::syncVisibleSteps(){for(int s=0;s<16;s++)steps[s].setToggleState(p.engine.pattern(p.engine.getPattern()).step[selected][bank*16+s].on,juce::dontSendNotification);}
void JerzyDrumMachineAudioProcessorEditor::syncStepControls(){auto&st=p.engine.pattern(p.engine.getPattern()).step[selected][juce::jlimit(0,63,selectedStep)];stepVelocity.setValue(st.velocity,juce::dontSendNotification);stepProbability.setValue(st.probability,juce::dontSendNotification);stepRatchet.setValue(st.ratchet,juce::dontSendNotification);stepMicro.setValue(st.micro,juce::dontSendNotification);stepAccent.setToggleState(st.accent,juce::dontSendNotification);stepFlam.setToggleState(st.flam,juce::dontSendNotification);}
void JerzyDrumMachineAudioProcessorEditor::timerCallback(){repaint();}

static void panel(juce::Graphics&g,juce::Rectangle<float>r){
 g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1b1310),r.getTopLeft(),juce::Colour(0xff0b0908),r.getBottomRight(),false));g.fillRoundedRectangle(r,7);
 g.setColour(juce::Colour(0xff5e3928));g.drawRoundedRectangle(r,7,1.2f);g.setColour(juce::Colour(0x33210f08));g.drawRoundedRectangle(r.reduced(4),5,1);
}

void JerzyDrumMachineAudioProcessorEditor::paint(juce::Graphics&g){
 auto all=getLocalBounds().toFloat();g.setGradientFill(juce::ColourGradient(juce::Colour(0xff120e0c),all.getTopLeft(),juce::Colour(0xff050505),all.getBottomRight(),false));g.fillRect(all);
 g.setColour(juce::Colour(0xff6d422d));g.drawRoundedRectangle(all.reduced(7),9,2);
 g.setColour(juce::Colour(0xffa36536));g.drawLine(18,70,(float)getWidth()-18,70,1);
 g.setColour(juce::Colour(0xffd5984f));g.setFont(juce::Font(27.0f,juce::Font::bold));g.drawText("JERZY",24,15,110,34,juce::Justification::centredLeft);
 g.setColour(juce::Colour(0xffe3b66f));g.setFont(juce::Font(22.0f,juce::Font::plain));g.drawText("DRUM MACHINE",132,16,220,32,juce::Justification::centredLeft);
 g.setColour(juce::Colour(0xff8a5a39));g.setFont(11);g.drawText("ANALOG / DIGITAL / SAMPLE RHYTHM INSTRUMENT",25,47,330,16,juce::Justification::centredLeft);
 g.setColour(juce::Colour(0xffc88948));g.setFont(13);g.drawText(page==0?"SEQUENCER / PATTERN":page==1?"SOUND / KIT":"MIXER / EFFECTS",24,82,240,24,juce::Justification::centredLeft);

 if(page==0){
  panel(g,{18,112,160,(float)getHeight()-132});panel(g,{188,112,(float)getWidth()-206,382});panel(g,{188,505,(float)getWidth()-206,138});panel(g,{18,654,(float)getWidth()-36,126});
 g.setColour(juce::Colour(0xff9b6a43));g.setFont(11);g.drawText("INSTRUMENTS",30,120,130,18,juce::Justification::centredLeft);g.drawText("STEP PARAMETERS",204,512,140,18,juce::Justification::centredLeft);g.drawText("GENERATIVE GROOVE",34,660,160,18,juce::Justification::centredLeft);
  static const char* labels[]={"VELOCITY","PROBABILITY","RATCHET","MICROTIMING"};for(int i=0;i<4;i++)g.drawText(labels[i],205,539+i*26,92,20,juce::Justification::centredLeft);
  g.drawText("PATTERN LENGTH",690,539,112,20,juce::Justification::centredLeft);g.drawText("PATTERN / MIDI NOTE",665,568,137,20,juce::Justification::centredLeft);g.drawText("CHANGE MODE",696,597,106,20,juce::Justification::centredLeft);
  static const char*gn[]={"COMPLEX","SYNCOP","HUMAN","CHAOS","KICK STAB","SNARE STAB","HAT ACT","PERC ACT"};for(int i=0;i<8;i++)g.drawText(gn[i],32+i*((getWidth()-64)/8),744,(getWidth()-64)/8,18,juce::Justification::centred);
 int cur=p.engine.getCurrentStep();if(cur>=bank*16&&cur<bank*16+16){auto rr=steps[cur-bank*16].getBounds().toFloat().expanded(2);g.setColour(juce::Colour(0xffffc66a));g.drawRoundedRectangle(rr,4,2);}
 }
 if(page==1){
  panel(g,{18,112,(float)getWidth()-36,62});panel(g,{78,196,(float)getWidth()-156,390});panel(g,{18,605,(float)getWidth()-36,96});
  g.setColour(juce::Colour(0xffe0ad67));g.setFont(30);g.drawText(names[selected],260,215,getWidth()-520,42,juce::Justification::centred);
  g.setColour(juce::Colour(0xff8e603f));g.setFont(12);g.drawText(selected<4?"ANALOG MODEL":selected<8?"DIGITAL SYNTHESIS":selected<11?"SAMPLE ENGINE":"SYNTH VOICE",260,255,getWidth()-520,22,juce::Justification::centred);
  static const char*spn[]={"TUNE","DECAY","TONE","CHARACTER"};for(int i=0;i<4;i++)g.drawText(spn[i],310+i*165,455,130,22,juce::Justification::centred);
  if(selected>=8&&selected<=10){g.setColour(juce::Colour(0xffd39b55));g.drawText(p.engine.getSampleName(selected-8).isEmpty()?"NO SAMPLE LOADED":p.engine.getSampleName(selected-8),310,292,getWidth()-620,24,juce::Justification::centred);}
  g.setColour(juce::Colour(0xff755039));g.setFont(11);g.drawText("KIT / SAMPLE FILES",34,613,150,18,juce::Justification::centredLeft);
 }
 if(page==2){
  panel(g,{18,112,(float)getWidth()-36,498});panel(g,{18,625,(float)getWidth()-36,120});
  g.setColour(juce::Colour(0xff9d6b43));g.setFont(11);g.drawText("LEVEL",24,180,72,18,juce::Justification::centred);g.drawText("PAN",24,238,72,18,juce::Justification::centred);g.drawText("FILTER",24,296,72,18,juce::Justification::centred);g.drawText("DRIVE",24,354,72,18,juce::Justification::centred);g.drawText("REVERB",24,412,72,18,juce::Justification::centred);g.drawText("DELAY",24,470,72,18,juce::Justification::centred);g.drawText("M / S",24,530,72,18,juce::Justification::centred);
  int cw=(getWidth()-120)/12;g.setColour(juce::Colour(0xffd7a35f));for(int i=0;i<12;i++)g.drawFittedText(names[i],92+i*cw,132,cw-4,26,juce::Justification::centred,2);
  g.setColour(juce::Colour(0xff8f603d));g.drawText("GLOBAL FX / MASTER",32,633,200,20,juce::Justification::centredLeft);
 static const char*fxn[]={"REV SIZE","DAMP","DLY TIME","FEEDBACK","DLY MIX","BASS","TREBLE","COMP"};for(int i=0;i<8;i++){g.setColour(juce::Colour(0xffc18a4e));g.drawText(fxn[i],210+i*105,638,96,18,juce::Justification::centred);}
 g.setColour(juce::Colour(0xffd5a25d));g.drawText("DRIVE",getWidth()-132,638,90,18,juce::Justification::centred);
 }
}

void JerzyDrumMachineAudioProcessorEditor::resized(){
 auto w=getWidth();auto h=getHeight();seq.setBounds(w/2-220,18,100,38);sound.setBounds(w/2-110,18,100,38);mix.setBounds(w/2,18,100,38);run.setVisible(page==0);run.setBounds(w/2+110,18,94,38);
 bool isSeq=page==0,isSound=page==1,isMix=page==2;
 for(int i=0;i<12;i++){
  instruments[i].setVisible(isSeq||isSound);
  if(isSeq)instruments[i].setBounds(30,145+i*38,136,30);
  else if(isSound){int iw=(w-70)/12;instruments[i].setBounds(35+i*iw,126,iw-5,34);}
 }
 for(int b=0;b<4;b++){banks[b].setVisible(isSeq);banks[b].setBounds(205+b*105,126,96,30);}
 int stepW=juce::jmax(38,(w-250)/16);int stepSize=juce::jmin(60,stepW-5);for(int s=0;s<16;s++){steps[s].setVisible(isSeq);steps[s].setBounds(202+s*stepW,172,stepSize,stepSize);}
 stepVelocity.setVisible(isSeq);stepProbability.setVisible(isSeq);stepRatchet.setVisible(isSeq);stepMicro.setVisible(isSeq);stepAccent.setVisible(isSeq);stepFlam.setVisible(isSeq);patternLength.setVisible(isSeq);patternSelect.setVisible(isSeq);changeMode.setVisible(isSeq);
 stepVelocity.setBounds(300,536,250,22);stepProbability.setBounds(300,562,250,22);stepRatchet.setBounds(300,588,250,22);stepMicro.setBounds(300,614,250,22);stepAccent.setBounds(565,541,90,28);stepFlam.setBounds(565,580,90,28);patternLength.setBounds(805,536,210,22);patternSelect.setBounds(805,564,235,26);changeMode.setBounds(805,594,235,26);
 generate.setVisible(isSeq);mutate.setVisible(isSeq);fill.setVisible(isSeq);generate.setBounds(w-356,663,108,34);mutate.setBounds(w-240,663,100,34);fill.setBounds(w-132,663,88,34);
 int gw=(w-64)/8;for(int i=0;i<8;i++){genParam[i].setVisible(isSeq);genParam[i].setBounds(28+i*gw,681,gw,70);}

 loadSample.setVisible(isSound&&selected>=8&&selected<=10);loadSample.setBounds(w/2-72,326,144,36);saveKit.setVisible(isSound);loadKit.setVisible(isSound);saveKit.setBounds(34,642,112,34);loadKit.setBounds(154,642,112,34);
 for(int i=0;i<4;i++){soundParam[i].setVisible(isSound);soundParam[i].setBounds(305+i*165,338,140,120);}

 int cw=(w-120)/12;for(int i=0;i<12;i++){int x=92+i*cw;channelGain[i].setVisible(isMix);channelPan[i].setVisible(isMix);channelFilter[i].setVisible(isMix);channelDrive[i].setVisible(isMix);revSend[i].setVisible(isMix);delSend[i].setVisible(isMix);channelMute[i].setVisible(isMix);channelSolo[i].setVisible(isMix);channelGain[i].setBounds(x,156,cw-4,58);channelPan[i].setBounds(x,214,cw-4,58);channelFilter[i].setBounds(x,272,cw-4,58);channelDrive[i].setBounds(x,330,cw-4,58);revSend[i].setBounds(x,388,cw-4,58);delSend[i].setBounds(x,446,cw-4,58);int bw=juce::jmax(20,(cw-10)/2);channelMute[i].setBounds(x+2,518,bw,27);channelSolo[i].setBounds(x+bw+5,518,bw,27);}
 for(int i=0;i<8;i++){fxParam[i].setVisible(isMix);fxParam[i].setBounds(210+i*105,660,96,72);}masterDrive.setVisible(isMix);masterDrive.setBounds(w-130,660,90,72);
}

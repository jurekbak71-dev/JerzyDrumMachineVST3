#include "PluginEditor.h"
static const char* names[]={"KICK","SNARE","TOM","METAL HAT","FM PERC","PHASE PERC","WAVE METAL","NOISE RESO","SAMPLE 1","SAMPLE 2","SAMPLE 3","SYNTH"};
static const std::array<juce::Colour,12> channelColours={{juce::Colour(0xffe06a3b),juce::Colour(0xffd94d48),juce::Colour(0xffd6a23d),juce::Colour(0xffd7c548),juce::Colour(0xff45b6ab),juce::Colour(0xff9a72d2),juce::Colour(0xff568dd1),juce::Colour(0xffb064c6),juce::Colour(0xff62a66d),juce::Colour(0xff73ad76),juce::Colour(0xff438e80),juce::Colour(0xff55a9de)}};

JerzyDrumMachineAudioProcessorEditor::JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&x):AudioProcessorEditor(&x),p(x){
 setLookAndFeel(&copper);setSize(1280,800);setResizable(true,true);setResizeLimits(1080,700,1920,1080);
 for(auto*b:{&seq,&sound,&mix,&fxPage,&drummer,&songPage,&run,&generate,&mutate,&fill,&loadSample,&saveKit,&loadKit,&songAdd,&songRemove,&songPlay,&randomizeSeed})addAndMakeVisible(*b);addAndMakeVisible(showSynthKeyboard);
 for(auto*b:{&seq,&sound,&mix,&fxPage,&drummer,&songPage})b->setClickingTogglesState(true);seq.setToggleState(true,juce::dontSendNotification);
 auto setPage=[this](int pg){page=pg;seq.setToggleState(pg==0,juce::dontSendNotification);sound.setToggleState(pg==1,juce::dontSendNotification);mix.setToggleState(pg==2,juce::dontSendNotification);fxPage.setToggleState(pg==3,juce::dontSendNotification);drummer.setToggleState(pg==4,juce::dontSendNotification);songPage.setToggleState(pg==5,juce::dontSendNotification);resized();repaint();};
 seq.onClick=[setPage]{setPage(0);};sound.onClick=[setPage]{setPage(1);};mix.onClick=[setPage]{setPage(2);};fxPage.onClick=[setPage]{setPage(3);};drummer.onClick=[setPage]{setPage(4);};songPage.onClick=[setPage]{setPage(5);};
 run.setClickingTogglesState(true);run.onClick=[this]{p.engine.setPreviewPlaying(run.getToggleState());run.setButtonText(run.getToggleState()?"STOP":"RUN");};
 drummerEnable.setClickingTogglesState(true);drummerEnable.onClick=[this]{p.engine.setVirtualDrummer(drummerEnable.getToggleState());syncVisibleSteps();repaint();};addAndMakeVisible(drummerEnable);
 for(auto&s:drummerParam){addAndMakeVisible(s);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,60,20);s.setRange(0,1,.01);}
 drummerParam[0].setValue(.55);drummerParam[1].setValue(.2);drummerParam[2].setValue(.25);
 for(auto*s:{&drummerStyle,&hatDivision,&phraseLength,&drummerFillEvery,&drummerBreakEvery})addAndMakeVisible(*s);
 drummerStyle.addItem("ROCK",1);drummerStyle.addItem("ELECTRONIC",2);drummerStyle.addItem("BREAKBEAT",3);drummerStyle.addItem("FUNK",4);drummerStyle.addItem("MINIMAL",5);drummerStyle.setSelectedId(1);
 hatDivision.addItem("1/4 HATS",1);hatDivision.addItem("1/8 HATS",2);hatDivision.addItem("1/16 HATS",3);hatDivision.setSelectedId(2);
 phraseLength.addItem("2 BARS",1);phraseLength.addItem("4 BARS",2);phraseLength.addItem("8 BARS",3);phraseLength.setSelectedId(2);
 for(int i=0;i<5;i++){const int bars[]={1,2,4,8,16};drummerFillEvery.addItem("FILL / "+juce::String(bars[i])+" BARS",i+1);drummerBreakEvery.addItem("BREAK / "+juce::String(bars[i])+" BARS",i+1);}drummerFillEvery.setSelectedId(3);drummerBreakEvery.setSelectedId(4);
 auto updateDrummer=[this]{const int fillBars[]={1,2,4,8,16},breakBars[]={1,2,4,8,16};p.engine.setVirtualDrummerSettings(drummerStyle.getSelectedId()-1,hatDivision.getSelectedId()==1?4:(hatDivision.getSelectedId()==2?2:1),(float)drummerParam[0].getValue(),(float)drummerParam[1].getValue(),(float)drummerParam[2].getValue(),phraseLength.getSelectedId()==1?2:(phraseLength.getSelectedId()==3?8:4),fillBars[juce::jlimit(0,4,drummerFillEvery.getSelectedId()-1)],breakBars[juce::jlimit(0,4,drummerBreakEvery.getSelectedId()-1)]);};
 for(auto&s:drummerParam)s.onValueChange=updateDrummer;drummerStyle.onChange=updateDrummer;hatDivision.onChange=updateDrummer;phraseLength.onChange=updateDrummer;drummerFillEvery.onChange=updateDrummer;drummerBreakEvery.onChange=updateDrummer;updateDrummer();

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
 showSynthKeyboard.setClickingTogglesState(true);showSynthKeyboard.setToggleState(true,juce::dontSendNotification);showSynthKeyboard.onClick=[this]{resized();repaint();};
 for(int i=0;i<24;i++){auto&key=synthKeys[(size_t)i];const int note=48+i;key.setButtonText(juce::MidiMessage::getMidiNoteName(note,true,true,4));addAndMakeVisible(key);key.onNoteDown=[this,note]{p.engine.triggerSynthNote(note,.85f);};key.onNoteUp=[this,note]{p.engine.releaseSynthNote(note);};const int semitone=i%12;const bool black=semitone==1||semitone==3||semitone==6||semitone==8||semitone==10;key.setColour(juce::TextButton::buttonColourId,black?juce::Colour(0xff171413):juce::Colour(0xffd2c6ae));key.setColour(juce::TextButton::buttonOnColourId,black?juce::Colour(0xffb56b32):juce::Colour(0xffefb95e));key.setColour(juce::TextButton::textColourOffId,black?juce::Colour(0xffc9a874):juce::Colour(0xff22180f));key.setColour(juce::TextButton::textColourOnId,juce::Colour(0xffffe0a0));}
 for(int octave=0;octave<2;octave++)for(int semitone:{1,3,6,8,10})synthKeys[(size_t)(octave*12+semitone)].toFront(false);
 for(int i=0;i<DrumEngine::maxSongEntries;i++){auto&slot=songSlots[(size_t)i];addAndMakeVisible(slot);slot.onClick=[this,i]{selectedSongSlot=i;auto entry=p.engine.getSongEntry(i);songPatternSelect.setSelectedId(i<p.engine.getSongLength()?entry.pattern+1:p.engine.getPattern()+1,juce::dontSendNotification);songSectionSelect.setSelectedId(i<p.engine.getSongLength()?entry.section+1:1,juce::dontSendNotification);songBars.setSelectedId(i<p.engine.getSongLength()?entry.bars:4,juce::dontSendNotification);syncSongSlots();};}
 for(int i=0;i<DrumEngine::patterns;i++)songPatternSelect.addItem("PATTERN "+juce::String(i+1),i+1);songPatternSelect.setSelectedId(p.engine.getPattern()+1);
 songSectionSelect.addItem("INTRO",1);songSectionSelect.addItem("VERSE",2);songSectionSelect.addItem("CHORUS",3);songSectionSelect.addItem("BRIDGE",4);songSectionSelect.addItem("BREAK",5);songSectionSelect.addItem("TRANSITION",6);songSectionSelect.addItem("OUTRO",7);
 for(int i=1;i<=32;i++)songBars.addItem(juce::String(i)+" BARS",i);songBars.setSelectedId(4);
 addAndMakeVisible(songPatternSelect);addAndMakeVisible(songSectionSelect);addAndMakeVisible(songBars);
 songAdd.onClick=[this]{p.engine.setSongEntry(selectedSongSlot,songPatternSelect.getSelectedId()-1,songSectionSelect.getSelectedId()-1,songBars.getSelectedId());selectedSongSlot=juce::jmin(DrumEngine::maxSongEntries-1,selectedSongSlot+1);syncSongSlots();};
 songRemove.onClick=[this]{p.engine.removeSongEntry(selectedSongSlot);selectedSongSlot=juce::jmin(selectedSongSlot,juce::jmax(0,p.engine.getSongLength()-1));syncSongSlots();};
 songPlay.setClickingTogglesState(true);songPlay.onClick=[this]{const bool requested=songPlay.getToggleState();p.engine.setSongPlaying(requested);const bool playing=p.engine.isSongPlaying();songPlay.setToggleState(playing,juce::dontSendNotification);songPlay.setButtonText(playing?"STOP SONG":"PLAY SONG");p.engine.setPreviewPlaying(playing);syncSongSlots();};

 for(auto*s:{&synthWave1,&synthWave2}){addAndMakeVisible(*s);s->addItem("SINE",1);s->addItem("SAW",2);s->addItem("SQUARE",3);s->addItem("TRIANGLE",4);}
 synthWave1.setSelectedId(1);synthWave2.setSelectedId(2);addAndMakeVisible(synthOsc2Enable);addAndMakeVisible(synthFilterEnable);synthOsc2Enable.setClickingTogglesState(true);synthFilterEnable.setClickingTogglesState(true);
 const char* synthIds[]={"synth_attack","synth_decay","synth_sustain","synth_release","synth_cutoff","synth_resonance","synth_osc2mix","synth_detune"};
 const float synthRanges[][2]={{.1f,1000},{1,3000},{0,1},{1,5000},{20,20000},{0,.98f},{0,1},{-50,50}};
 const float synthDefaults[]={8,180,.72f,140,12000,.12f,.35f,7};
 for(int i=0;i<8;i++){auto&s=synthParam[(size_t)i];addAndMakeVisible(s);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,58,18);s.setRange(synthRanges[i][0],synthRanges[i][1],i==2||i==5||i==6? .01: .1);s.setValue(synthDefaults[i]);}
 synthWaveAtt[0]=std::make_unique<ComboAttachment>(p.apvts,"synth_wave1",synthWave1);synthWaveAtt[1]=std::make_unique<ComboAttachment>(p.apvts,"synth_wave2",synthWave2);
 for(int i=0;i<8;i++)synthAtt[(size_t)i]=std::make_unique<SliderAttachment>(p.apvts,synthIds[i],synthParam[(size_t)i]);
 synthButtonAtt[0]=std::make_unique<ButtonAttachment>(p.apvts,"synth_osc2",synthOsc2Enable);synthButtonAtt[1]=std::make_unique<ButtonAttachment>(p.apvts,"synth_filter",synthFilterEnable);
 addAndMakeVisible(artifactEnable);artifactEnable.setClickingTogglesState(true);artifactAmount.setRange(0,1,.01);artifactAmount.setValue(.25);artifactAmount.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);artifactAmount.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,18);addAndMakeVisible(artifactAmount);
 artifactSeed.setRange(1,999999,1);artifactSeed.setValue(70271);artifactSeed.setSliderStyle(juce::Slider::LinearHorizontal);artifactSeed.setTextBoxStyle(juce::Slider::TextBoxRight,false,72,20);addAndMakeVisible(artifactSeed);
 artifactAmountAtt=std::make_unique<SliderAttachment>(p.apvts,"artifact_amount",artifactAmount);artifactSeedAtt=std::make_unique<SliderAttachment>(p.apvts,"artifact_seed",artifactSeed);synthButtonAtt[2]=std::make_unique<ButtonAttachment>(p.apvts,"artifact_enable",artifactEnable);
 randomizeSeed.onClick=[this]{static juce::Random seedRandom;artifactSeed.setValue(seedRandom.nextInt(999999)+1);};
 addAndMakeVisible(trackLength);trackLength.setSliderStyle(juce::Slider::LinearHorizontal);trackLength.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,20);trackLength.setRange(1,64,1);trackLength.setValue(p.engine.pattern(p.engine.getPattern()).trackLength[(size_t)selected]);trackLength.onValueChange=[this]{p.engine.setTrackLength(p.engine.getPattern(),selected,(int)trackLength.getValue());};

 for(int i=0;i<8;i++){addAndMakeVisible(genParam[i]);genParam[i].setRange(0,1,.01);genParam[i].setValue(i==4?.85:(i==5?.9:(i==6?.65:(i==7?.35:.25))));genParam[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);genParam[i].setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);}
 auto updateGen=[this]{p.engine.setGenerator((float)genParam[0].getValue(),(float)genParam[1].getValue(),(float)genParam[2].getValue(),(float)genParam[3].getValue(),(float)genParam[4].getValue(),(float)genParam[5].getValue(),(float)genParam[6].getValue(),(float)genParam[7].getValue());};
 for(auto&s:genParam)s.onValueChange=updateGen;updateGen();
 generate.onClick=[this]{p.engine.generate(p.apvts.getRawParameterValue("density")->load(),p.apvts.getRawParameterValue("variation")->load());syncVisibleSteps();repaint();};
 mutate.onClick=[this]{p.engine.mutate(p.apvts.getRawParameterValue("variation")->load());syncVisibleSteps();repaint();};
 fill.onClick=[this]{p.engine.fill();syncVisibleSteps();repaint();};

 loadSample.onClick=[this]{if(selected<8||selected>10)return;chooser=std::make_unique<juce::FileChooser>("Load WAV sample",juce::File{},"*.wav;*.aif;*.aiff");auto flags=juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles;chooser->launchAsync(flags,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(file.existsAsFile())p.engine.loadSample(selected-8,file);repaint();});};

 for(int i=0;i<12;i++){
  for(auto*s:{&channelGain[i],&channelPan[i],&channelFilter[i],&channelDrive[i],&revSend[i],&delSend[i]}){addAndMakeVisible(*s);s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);s->setColour(juce::Slider::rotarySliderFillColourId,channelColours[(size_t)i]);}
  addAndMakeVisible(channelMute[i]);addAndMakeVisible(channelSolo[i]);channelMute[i].setButtonText("M");channelSolo[i].setButtonText("S");channelMute[i].setClickingTogglesState(true);channelSolo[i].setClickingTogglesState(true);
  channelGain[i].setRange(0,1.5,.01);channelGain[i].setValue(p.engine.gain[i]);channelGain[i].onValueChange=[this,i]{p.engine.gain[i]=(float)channelGain[i].getValue();};
  channelPan[i].setRange(-1,1,.01);channelPan[i].setValue(p.engine.panorama[i]);channelPan[i].onValueChange=[this,i]{p.engine.panorama[i]=(float)channelPan[i].getValue();};
  channelFilter[i].setRange(0,1,.01);channelFilter[i].setValue(p.engine.channelFilter[i]);channelFilter[i].onValueChange=[this,i]{p.engine.channelFilter[i]=(float)channelFilter[i].getValue();};
  channelDrive[i].setRange(0,1,.01);channelDrive[i].setValue(p.engine.channelDrive[i]);channelDrive[i].onValueChange=[this,i]{p.engine.channelDrive[i]=(float)channelDrive[i].getValue();};
  revSend[i].setRange(0,1,.01);revSend[i].setValue(p.engine.reverbSend[i]);revSend[i].onValueChange=[this,i]{p.engine.reverbSend[i]=(float)revSend[i].getValue();};
  delSend[i].setRange(0,1,.01);delSend[i].setValue(p.engine.delaySend[i]);delSend[i].onValueChange=[this,i]{p.engine.delaySend[i]=(float)delSend[i].getValue();};
  channelMute[i].setToggleState(p.engine.mute[i],juce::dontSendNotification);channelSolo[i].setToggleState(p.engine.solo[i],juce::dontSendNotification);
  channelMute[i].setColour(juce::TextButton::buttonColourId,juce::Colour(0xff3a1111));channelMute[i].setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd53932));channelMute[i].setColour(juce::TextButton::textColourOffId,juce::Colour(0xffffb0a6));channelMute[i].setColour(juce::TextButton::textColourOnId,juce::Colours::white);
  channelSolo[i].setColour(juce::TextButton::buttonColourId,juce::Colour(0xff102a46));channelSolo[i].setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff347ed1));channelSolo[i].setColour(juce::TextButton::textColourOffId,juce::Colour(0xffa7d3ff));channelSolo[i].setColour(juce::TextButton::textColourOnId,juce::Colours::white);
  channelMute[i].onClick=[this,i]{p.engine.mute[i]=channelMute[i].getToggleState();};channelSolo[i].onClick=[this,i]{p.engine.solo[i]=channelSolo[i].getToggleState();};
 }
 addAndMakeVisible(masterDrive);masterDrive.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);masterDrive.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,18);masterDrive.setRange(.5,3,.01);masterDrive.setValue(p.engine.drive);masterDrive.onValueChange=[this]{p.engine.drive=(float)masterDrive.getValue();};
 for(auto&s:fxParam){addAndMakeVisible(s);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,18);}
 reverbEnable.setClickingTogglesState(true);delayEnable.setClickingTogglesState(true);delayPingPong.setClickingTogglesState(true);compressorEnable.setClickingTogglesState(true);
 for(auto*b:{&reverbEnable,&delayEnable,&delayPingPong,&compressorEnable})addAndMakeVisible(*b);
 fxParam[0].setRange(0,1,.01);fxParam[0].setValue(p.engine.reverbSize);fxParam[1].setRange(0,1,.01);fxParam[1].setValue(p.engine.reverbDamping);fxParam[2].setRange(0,1,.01);fxParam[2].setValue(p.engine.reverbMix);
 fxParam[3].setRange(.125,2.0,.125);fxParam[3].setValue(p.engine.delayBeats);fxParam[4].setRange(0,.88,.01);fxParam[4].setValue(p.engine.delayFeedback);fxParam[5].setRange(0,1,.01);fxParam[5].setValue(p.engine.delayMix);
 fxParam[6].setRange(.4,1.8,.01);fxParam[6].setValue(p.engine.masterBass);fxParam[7].setRange(.4,1.8,.01);fxParam[7].setValue(p.engine.masterTreble);
 fxParam[8].setRange(-36,0,.1);fxParam[8].setValue(p.engine.compressorThresholdDb);fxParam[9].setRange(1,20,.1);fxParam[9].setValue(p.engine.compressorRatio);
 fxParam[10].setRange(.1,100,.1);fxParam[10].setValue(p.engine.compressorAttackMs);fxParam[11].setRange(10,500,1);fxParam[11].setValue(p.engine.compressorReleaseMs);fxParam[12].setRange(.6,3,.01);fxParam[12].setValue(p.engine.masterComp);
 fxParam[13].setRange(0,24,.1);fxParam[13].setValue(p.engine.compressorBoostDb);
 reverbEnable.setToggleState(p.engine.reverbEnabled,juce::dontSendNotification);delayEnable.setToggleState(p.engine.delayEnabled,juce::dontSendNotification);delayPingPong.setToggleState(p.engine.delayPingPong,juce::dontSendNotification);compressorEnable.setToggleState(p.engine.compressorEnabled,juce::dontSendNotification);
 fxParam[0].onValueChange=[this]{p.engine.setReverb((float)fxParam[0].getValue(),(float)fxParam[1].getValue(),(float)fxParam[2].getValue());};fxParam[1].onValueChange=fxParam[0].onValueChange;fxParam[2].onValueChange=fxParam[0].onValueChange;
 auto updDelay=[this]{p.engine.setDelay((float)fxParam[3].getValue(),(float)fxParam[4].getValue(),(float)fxParam[5].getValue());};fxParam[3].onValueChange=updDelay;fxParam[4].onValueChange=updDelay;fxParam[5].onValueChange=updDelay;
 auto updMaster=[this]{p.engine.setMaster((float)fxParam[6].getValue(),(float)fxParam[7].getValue(),(float)fxParam[12].getValue());};fxParam[6].onValueChange=updMaster;fxParam[7].onValueChange=updMaster;fxParam[12].onValueChange=updMaster;
 auto updComp=[this]{p.engine.setCompressor(compressorEnable.getToggleState(),(float)fxParam[8].getValue(),(float)fxParam[9].getValue(),(float)fxParam[10].getValue(),(float)fxParam[11].getValue());};for(int i=8;i<=11;i++)fxParam[(size_t)i].onValueChange=updComp;compressorEnable.onClick=updComp;
 fxParam[13].onValueChange=[this]{p.engine.setCompressorBoost((float)fxParam[13].getValue());};
 reverbEnable.onClick=[this]{p.engine.setReverbEnabled(reverbEnable.getToggleState());};delayEnable.onClick=[this]{p.engine.setDelayEnabled(delayEnable.getToggleState());};delayPingPong.onClick=[this]{p.engine.setDelayPingPong(delayPingPong.getToggleState());};
 for(int i=0;i<12;i++){auto prefix="ch"+juce::String(i)+"_";gainAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"gain",channelGain[i]);panAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"pan",channelPan[i]);filterAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"filter",channelFilter[i]);driveAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"drive",channelDrive[i]);revAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"rev",revSend[i]);delAtt[i]=std::make_unique<SliderAttachment>(p.apvts,prefix+"del",delSend[i]);muteAtt[i]=std::make_unique<ButtonAttachment>(p.apvts,prefix+"mute",channelMute[i]);soloAtt[i]=std::make_unique<ButtonAttachment>(p.apvts,prefix+"solo",channelSolo[i]);}
 masterDriveAtt=std::make_unique<SliderAttachment>(p.apvts,"drive",masterDrive);
 static const char*fxIds[]={"rev_size","rev_damp","rev_mix","delay_beats","delay_fb","delay_mix","master_bass","master_treble","comp_threshold","comp_ratio","comp_attack","comp_release","master_comp","comp_boost"};for(int i=0;i<14;i++)fxAtt[(size_t)i]=std::make_unique<SliderAttachment>(p.apvts,fxIds[i],fxParam[(size_t)i]);
 fxButtonAtt[0]=std::make_unique<ButtonAttachment>(p.apvts,"reverb_on",reverbEnable);fxButtonAtt[1]=std::make_unique<ButtonAttachment>(p.apvts,"delay_on",delayEnable);fxButtonAtt[2]=std::make_unique<ButtonAttachment>(p.apvts,"delay_pingpong",delayPingPong);fxButtonAtt[3]=std::make_unique<ButtonAttachment>(p.apvts,"comp_on",compressorEnable);

 for(auto*s:{&stepVelocity,&stepProbability,&stepRatchet,&stepMicro}){addAndMakeVisible(*s);s->setSliderStyle(juce::Slider::LinearHorizontal);s->setTextBoxStyle(juce::Slider::TextBoxRight,false,56,20);}addAndMakeVisible(stepNote);
 addAndMakeVisible(stepAccent);addAndMakeVisible(stepFlam);addAndMakeVisible(patternLength);addAndMakeVisible(patternSelect);addAndMakeVisible(changeMode);
 stepVelocity.setRange(0,1,.01);stepProbability.setRange(0,1,.01);stepRatchet.setRange(1,4,1);stepMicro.setRange(0,1,.01);
 for(int n=60;n<72;n++)stepNote.addItem(juce::MidiMessage::getMidiNoteName(n,true,true,4),n-59);
 patternLength.setSliderStyle(juce::Slider::LinearHorizontal);patternLength.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,20);patternLength.setRange(1,64,1);patternLength.setValue(p.engine.pattern(p.engine.getPattern()).length);
 changeMode.addItem("IMMEDIATE",1);changeMode.addItem("NEXT BEAT",2);changeMode.addItem("NEXT BAR",3);changeMode.addItem("END PATTERN",4);changeMode.setSelectedId((int)p.engine.getChangeMode()+1);
 for(int i=0;i<32;i++)patternSelect.addItem("PATTERN "+juce::String(i+1)+"  /  "+juce::MidiMessage::getMidiNoteName(60+i,true,true,3),i+1);patternSelect.setSelectedId(p.engine.getPattern()+1);
 stepVelocity.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].velocity=(float)stepVelocity.getValue();};
 stepProbability.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].probability=(float)stepProbability.getValue();};
 stepRatchet.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].ratchet=(int)stepRatchet.getValue();};
 stepMicro.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].micro=(float)stepMicro.getValue();};
 stepAccent.onClick=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].accent=stepAccent.getToggleState();};
 stepFlam.onClick=[this]{p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].flam=stepFlam.getToggleState();};
 stepNote.onChange=[this]{if(stepNote.getSelectedId()>0)p.engine.pattern(p.engine.getPattern()).step[selected][selectedStep].note=59+stepNote.getSelectedId();};
 changeMode.onChange=[this]{p.engine.setChangeMode((DrumEngine::ChangeMode)(changeMode.getSelectedId()-1));};
 patternLength.onValueChange=[this]{p.engine.pattern(p.engine.getPattern()).length=(int)patternLength.getValue();};
 patternSelect.onChange=[this]{p.engine.setPattern(patternSelect.getSelectedId()-1);patternLength.setValue(p.engine.pattern(p.engine.getPattern()).length,juce::dontSendNotification);syncVisibleSteps();syncStepControls();repaint();};

 for(int i=0;i<4;i++){addAndMakeVisible(soundParam[i]);soundParam[i].setRange(0,1,.01);soundParam[i].setValue(p.engine.getVoiceParam(selected,i));soundParam[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);soundParam[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,56,18);soundParam[i].onValueChange=[this,i]{p.engine.setVoiceParam(selected,i,(float)soundParam[i].getValue());};}attachSoundParameters();

 saveKit.onClick=[this]{kitChooser=std::make_unique<juce::FileChooser>("Save Jerzy Drum Kit",juce::File{},"*.jdmkit");kitChooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(file.getFileExtension().isEmpty())file=file.withFileExtension(".jdmkit");if(auto xml=p.engine.saveState().createXml())xml->writeTo(file);});};
 loadKit.onClick=[this]{kitChooser=std::make_unique<juce::FileChooser>("Load Jerzy Drum Kit",juce::File{},"*.jdmkit");kitChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser&fc){auto file=fc.getResult();if(auto xml=juce::XmlDocument::parse(file)){p.engine.loadState(juce::ValueTree::fromXml(*xml));p.syncParametersFromEngine();attachSoundParameters();syncVisibleSteps();syncStepControls();repaint();}});};

 syncVisibleSteps();syncStepControls();syncSongSlots();resized();startTimerHz(20);
}

JerzyDrumMachineAudioProcessorEditor::~JerzyDrumMachineAudioProcessorEditor(){setLookAndFeel(nullptr);}

void JerzyDrumMachineAudioProcessorEditor::attachSoundParameters(){static const char* suffix[]={"tune","decay","tone","character"};for(int i=0;i<4;i++){soundAtt[i].reset();soundAtt[i]=std::make_unique<SliderAttachment>(p.apvts,"v"+juce::String(selected)+"_"+suffix[i],soundParam[i]);}}
void JerzyDrumMachineAudioProcessorEditor::syncVisibleSteps(){for(int s=0;s<16;s++)steps[s].setToggleState(p.engine.pattern(p.engine.getPattern()).step[selected][bank*16+s].on,juce::dontSendNotification);}
void JerzyDrumMachineAudioProcessorEditor::syncSongSlots(){static const char*sections[]={"INTRO","VERSE","CHORUS","BRIDGE","BREAK","TRANSITION","OUTRO"};const int length=p.engine.getSongLength();for(int i=0;i<DrumEngine::maxSongEntries;i++){auto&button=songSlots[(size_t)i];if(i<length){auto entry=p.engine.getSongEntry(i);button.setButtonText(juce::String(i+1).paddedLeft('0',2)+"  "+sections[juce::jlimit(0,6,entry.section)]+" · "+juce::String(entry.bars)+" BARS\nPATTERN "+juce::String(entry.pattern+1).paddedLeft('0',2));}else button.setButtonText(juce::String(i+1).paddedLeft('0',2)+"\nEMPTY");button.setToggleState(i==(p.engine.isSongPlaying()?p.engine.getSongPosition():selectedSongSlot),juce::dontSendNotification);}}
void JerzyDrumMachineAudioProcessorEditor::syncStepControls(){auto&st=p.engine.pattern(p.engine.getPattern()).step[selected][juce::jlimit(0,63,selectedStep)];stepVelocity.setValue(st.velocity,juce::dontSendNotification);stepProbability.setValue(st.probability,juce::dontSendNotification);stepRatchet.setValue(st.ratchet,juce::dontSendNotification);stepMicro.setValue(st.micro,juce::dontSendNotification);stepAccent.setToggleState(st.accent,juce::dontSendNotification);stepFlam.setToggleState(st.flam,juce::dontSendNotification);stepNote.setSelectedId(st.note-59,juce::dontSendNotification);}
void JerzyDrumMachineAudioProcessorEditor::timerCallback(){if(page==0&&p.engine.isVirtualDrummerEnabled())syncVisibleSteps();if(p.engine.isVirtualDrummerEnabled())drummerEnable.setToggleState(true,juce::dontSendNotification);if(page==0){const int pattern=p.engine.getPattern();patternLength.setValue(p.engine.pattern(pattern).length,juce::dontSendNotification);trackLength.setValue(p.engine.pattern(pattern).trackLength[(size_t)selected],juce::dontSendNotification);if(p.engine.isSongPlaying())patternSelect.setSelectedId(pattern+1,juce::dontSendNotification);}if(p.engine.isSongPlaying()){songPlay.setToggleState(true,juce::dontSendNotification);songPlay.setButtonText("STOP SONG");}else if(songPlay.getToggleState()){songPlay.setToggleState(false,juce::dontSendNotification);songPlay.setButtonText("PLAY SONG");}if(page==5)syncSongSlots();repaint();}

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
 g.setColour(juce::Colour(0xffc88948));g.setFont(13);g.drawText(page==0?"SEQUENCER / PATTERN":page==1?"SOUND / KIT":page==2?"MIXER / CHANNELS":page==3?"FX / EFFECT RACK":page==4?"VIRTUAL DRUMMER":"SONG / ARRANGEMENT",24,82,280,24,juce::Justification::centredLeft);

 if(page==0){
  const int paramY=juce::jmax(395,getHeight()-283),grooveY=getHeight()-146;
  panel(g,{18,112,160.0f,(float)getHeight()-132});panel(g,{188,112,(float)getWidth()-206,382});panel(g,{188,(float)paramY-31,(float)getWidth()-206,138});panel(g,{18,(float)grooveY,(float)getWidth()-36,126});
  g.setColour(juce::Colour(0xff9b6a43));g.setFont(11);g.drawText("INSTRUMENTS",30,120,130,18,juce::Justification::centredLeft);g.drawText("STEP PARAMETERS",204,paramY-25,140,18,juce::Justification::centredLeft);g.drawText("GENERATIVE GROOVE",34,grooveY+8,160,18,juce::Justification::centredLeft);
  static const char* labels[]={"VELOCITY","PROBABILITY","RATCHET","MICROTIMING"};for(int i=0;i<4;i++)g.drawText(labels[i],205,paramY+i*26,92,20,juce::Justification::centredLeft);
  g.drawText("TRACK LOOP",690,paramY,112,20,juce::Justification::centredLeft);g.drawText("MASTER STEPS",690,paramY+26,112,20,juce::Justification::centredLeft);g.drawText("PATTERN",690,paramY+52,112,20,juce::Justification::centredLeft);g.drawText("CHANGE MODE",690,paramY+78,112,20,juce::Justification::centredLeft);
  if(selected==11)g.drawText("NOTE",1040,paramY+78,60,20,juce::Justification::centredLeft);
  static const char*gn[]={"COMPLEX","SYNCOP","HUMAN","CHAOS","KICK STAB","SNARE STAB","HAT ACT","PERC ACT"};for(int i=0;i<8;i++)g.drawText(gn[i],32+i*((getWidth()-64)/8),grooveY+105,(getWidth()-64)/8,18,juce::Justification::centred);
 int cur=p.engine.getTrackStep(selected);if(cur>=bank*16&&cur<bank*16+16){auto rr=steps[cur-bank*16].getBounds().toFloat().expanded(2);g.setColour(juce::Colour(0xffffc66a));g.drawRoundedRectangle(rr,4,2);}
 }
 if(page==1){
  panel(g,{18,112,(float)getWidth()-36,62});panel(g,{78,196,(float)getWidth()-156,390});panel(g,{18,605,(float)getWidth()-36,96});
  g.setColour(juce::Colour(0xffe0ad67));g.setFont(30);g.drawText(selected==11?"MONO SYNTH":names[selected],260,215,getWidth()-520,42,juce::Justification::centred);
  g.setColour(juce::Colour(0xff8e603f));g.setFont(12);g.drawText(selected<4?"ANALOG DRUM CIRCUIT":selected<8?"DIGITAL DRUM ENGINE":selected<11?"SAMPLE ENGINE":"MONOPHONIC · DUAL OSCILLATOR · ADSR · RESONANT FILTER",260,255,getWidth()-520,22,juce::Justification::centred);
  static const char* generic[] = {"TUNE","DECAY","TONE","CHARACTER"};
  static const char* voiceLabels[8][4]={{"TUNE","DECAY","PUNCH","DRIVE"},{"TUNE","DECAY","SNAPPY","BODY"},{"TUNE","DECAY","DETUNE","TONE"},{"TUNE","DECAY","BRIGHTNESS","METAL"},{"CARRIER","DECAY","FM AMOUNT","MOD RATIO"},{"PITCH","DECAY","PHASE","RATIO"},{"PITCH","DECAY","BRIGHTNESS","RING"},{"PITCH","DECAY","FILTER","RESONANCE"}};
  if(selected!=11){for(int i=0;i<4;i++)g.drawText(selected<8?voiceLabels[selected][i]:generic[i],310+i*165,455,130,22,juce::Justification::centred);}
  else{static const char* synthLabels[]={"ATTACK ms","DECAY ms","SUSTAIN","RELEASE ms","CUTOFF Hz","RESONANCE","OSC 2 MIX","DETUNE cents"};g.setColour(juce::Colour(0xffb89b78));g.setFont(10);for(int i=0;i<8;i++)g.drawText(synthLabels[i],48+i*((getWidth()-96)/8),286,(getWidth()-96)/8,20,juce::Justification::centred);g.setColour(juce::Colour(0xffe0ad67));g.setFont(11);g.drawText("C4 / MIDI 60 = 261.626 Hz",44,477,getWidth()-88,18,juce::Justification::centredLeft);}
  if(selected>=8&&selected<=10){g.setColour(juce::Colour(0xffd39b55));g.drawText(p.engine.getSampleName(selected-8).isEmpty()?"NO SAMPLE LOADED":p.engine.getSampleName(selected-8),310,292,getWidth()-620,24,juce::Justification::centred);}
  g.setColour(juce::Colour(0xff755039));g.setFont(11);g.drawText("KIT / SAMPLE FILES",34,613,150,18,juce::Justification::centredLeft);if(selected==11&&showSynthKeyboard.getToggleState())g.drawText("MIDI KEYBOARD · C3–B4",44,477,getWidth()-88,18,juce::Justification::centredLeft);
 }
 if(page==2){
  panel(g,{18,112,(float)getWidth()-36,(float)getHeight()-132});
  g.setFont(11);g.setColour(juce::Colour(0xff9d6b43));g.drawText("LEVEL",24,180,62,18,juce::Justification::centred);g.drawText("PAN",24,238,62,18,juce::Justification::centred);g.drawText("FILTER",24,296,62,18,juce::Justification::centred);g.drawText("DRIVE",24,354,62,18,juce::Justification::centred);g.drawText("REVERB",24,412,62,18,juce::Justification::centred);g.drawText("DELAY",24,470,62,18,juce::Justification::centred);g.drawText("M / S",24,530,62,18,juce::Justification::centred);
  const int cw=(getWidth()-120)/12;
  for(int i=0;i<12;i++){
   const int x=92+i*cw;const auto colour=channelColours[(size_t)i];
   g.setColour(colour.withAlpha(.075f));g.fillRoundedRectangle((float)x-2,122,(float)cw-3,500,4.0f);
   g.setColour(colour.withAlpha(.55f));g.drawRoundedRectangle((float)x-2,122,(float)cw-3,500,4.0f,1.0f);
   g.setColour(colour);g.fillRoundedRectangle((float)x+3,130,(float)cw-13,4,2.0f);
   g.setFont(juce::Font(10.5f,juce::Font::bold));g.drawFittedText(names[i],x,137,cw-5,20,juce::Justification::centred,2);
  }
  g.setColour(juce::Colour(0xff8f603d));g.setFont(11);g.drawText("CHANNEL STRIPS  ·  MUTE RED  /  SOLO BLUE",34,getHeight()-93,380,20,juce::Justification::centredLeft);
 }
 if(page==3){
  const float cardY=148.0f,cardH=(float)getHeight()-166.0f;const float gap=14.0f,cardW=((float)getWidth()-56.0f-2.0f*gap)/3.0f;
  for(int i=0;i<3;i++){const float x=28.0f+i*(cardW+gap);panel(g,{x,cardY,cardW,cardH});const juce::Colour accents[]={juce::Colour(0xff55a9de),juce::Colour(0xffd39b55),juce::Colour(0xffd85a49)};g.setColour(accents[i]);g.fillRoundedRectangle(x+12,cardY+14,cardW-24,4,2);}
  const float c0=28.0f,c1=c0+cardW+gap,c2=c1+cardW+gap;
  g.setColour(juce::Colour(0xff8ec6e8));g.setFont(17);g.drawText("REVERB",c0+20,cardY+28,cardW-40,26,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xffe5ba7b));g.drawText("DELAY",c1+20,cardY+28,cardW-40,26,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xffe88c7e));g.drawText("MASTER / COMPRESSOR",c2+20,cardY+28,cardW-40,26,juce::Justification::centredLeft);
  reverbEnable.setBounds((int)c0+20,(int)cardY+70,130,30);delayEnable.setBounds((int)c1+20,(int)cardY+70,115,30);delayPingPong.setBounds((int)c1+145,(int)cardY+70,140,30);compressorEnable.setBounds((int)c2+20,(int)cardY+70,130,30);
  static const char* revLabels[]={"ROOM SIZE","DAMPING","RETURN"};static const char* delayLabels[]={"TIME · BEATS","FEEDBACK","MIX"};static const char* masterLabels[]={"LOW","HIGH","COMP DRIVE"};
  const int knobW=(int)(cardW-28)/3;
  for(int i=0;i<3;i++){g.setColour(juce::Colour(0xffb89b78));g.setFont(10);g.drawText(revLabels[i],(int)c0+12+i*knobW,(int)cardY+120,knobW,18,juce::Justification::centred);g.drawText(delayLabels[i],(int)c1+12+i*knobW,(int)cardY+120,knobW,18,juce::Justification::centred);g.drawText(masterLabels[i],(int)c2+12+i*knobW,(int)cardY+120,knobW,18,juce::Justification::centred);}
  static const char* compLabels[]={"THRESHOLD dB","RATIO","ATTACK ms","RELEASE ms","BOOST dB"};for(int i=0;i<5;i++){g.setColour(juce::Colour(0xffb89b78));g.setFont(9.5f);const int compW=(int)(cardW-20)/5;g.drawFittedText(compLabels[i],(int)c2+10+i*compW,(int)cardY+294,compW,24,juce::Justification::centred,2);}
  g.setColour(juce::Colour(0xffb89b78));g.setFont(10);g.drawText("OUTPUT DRIVE",c2+cardW*.5f-70,cardY+cardH-160,140,18,juce::Justification::centred);
 }
 if(page==4){
  panel(g,{18,112,(float)getWidth()-36,(float)getHeight()-132});
  g.setColour(juce::Colour(0xffe3b66f));g.setFont(23);g.drawText("VIRTUAL DRUMMER",42,132,300,34,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff9b6a43));g.setFont(13);g.drawText("An evolving groove: stable backbeat, human timing, phrase-level changes and fills.",42,173,getWidth()-84,24,juce::Justification::centredLeft);
  static const char*vd[]={"ENERGY","HUMAN FEEL","SYNCOPATION"};for(int i=0;i<3;i++)g.drawText(vd[i],80+i*215,397,180,20,juce::Justification::centred);
  g.drawText("STYLE",95,249,170,18,juce::Justification::centred);g.drawText("HAT DIVISION",365,249,180,18,juce::Justification::centred);g.drawText("PHRASE LENGTH",635,249,180,18,juce::Justification::centred);
  g.setColour(juce::Colour(0xffb89b78));g.setFont(11);g.drawText("FILL INTERVAL",60,433,180,18,juce::Justification::centredLeft);g.drawText("BREAK INTERVAL",280,433,180,18,juce::Justification::centredLeft);g.drawText("STOCHASTIC ARTIFACTS",520,433,240,18,juce::Justification::centredLeft);g.drawText("AMOUNT",825,433,100,18,juce::Justification::centred);
  g.setColour(juce::Colour(0xff755039));g.setFont(12);g.drawText("Fills and breaks are generated on bar boundaries; the groove develops over phrases and song sections.",42,getHeight()-48,getWidth()-84,24,juce::Justification::centredLeft);
 }
 if(page==5){
  panel(g,{18,112,(float)getWidth()-36,(float)getHeight()-132});
  g.setColour(juce::Colour(0xffe3b66f));g.setFont(23);g.drawText("SONG CHAIN",42,132,280,34,juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff9b6a43));g.setFont(13);g.drawText("Build a song from sections, patterns and explicit bar counts.",42,173,getWidth()-84,24,juce::Justification::centredLeft);
 }
}

void JerzyDrumMachineAudioProcessorEditor::resized(){
 auto w=getWidth();auto h=getHeight();
 // Components are all registered with addAndMakeVisible in the constructor.
 // Hide first so a newly opened editor can never show controls from every page.
 for(int i=0;i<getNumChildComponents();++i)getChildComponent(i)->setVisible(false);
 const int navX=w-718;seq.setVisible(true);sound.setVisible(true);mix.setVisible(true);fxPage.setVisible(true);drummer.setVisible(true);songPage.setVisible(true);
 seq.setBounds(navX,18,96,38);sound.setBounds(navX+100,18,96,38);mix.setBounds(navX+200,18,96,38);fxPage.setBounds(navX+300,18,82,38);drummer.setBounds(navX+386,18,112,38);songPage.setBounds(navX+504,18,88,38);run.setVisible(page==0||page==4);run.setBounds(navX+602,18,94,38);
 bool isSeq=page==0,isSound=page==1,isMix=page==2,isFx=page==3,isDrummer=page==4,isSong=page==5;
 for(int i=0;i<12;i++){
  instruments[i].setVisible(isSeq||isSound);
  if(isSeq)instruments[i].setBounds(30,145+i*38,136,30);
  else if(isSound){int iw=(w-70)/12;instruments[i].setBounds(35+i*iw,126,iw-5,34);}
 }
 for(int b=0;b<4;b++){banks[b].setVisible(isSeq);banks[b].setBounds(205+b*105,126,96,30);}
 int stepW=juce::jmax(38,(w-250)/16);int stepSize=juce::jmin(60,stepW-5);for(int s=0;s<16;s++){steps[s].setVisible(isSeq);steps[s].setBounds(202+s*stepW,172,stepSize,stepSize);}
 stepVelocity.setVisible(isSeq);stepProbability.setVisible(isSeq);stepRatchet.setVisible(isSeq);stepMicro.setVisible(isSeq);stepAccent.setVisible(isSeq);stepFlam.setVisible(isSeq);patternLength.setVisible(isSeq);patternSelect.setVisible(isSeq);changeMode.setVisible(isSeq);stepNote.setVisible(isSeq&&selected==11);
 const int paramY=juce::jmax(395,h-283),grooveY=h-146;
 stepVelocity.setBounds(300,paramY,250,22);stepProbability.setBounds(300,paramY+26,250,22);stepRatchet.setBounds(300,paramY+52,250,22);stepMicro.setBounds(300,paramY+78,250,22);stepAccent.setBounds(565,paramY+5,90,28);stepFlam.setBounds(565,paramY+48,90,28);trackLength.setVisible(isSeq);trackLength.setBounds(805,paramY,210,22);patternLength.setBounds(805,paramY+26,210,22);patternSelect.setBounds(805,paramY+52,235,26);changeMode.setBounds(805,paramY+78,235,26);stepNote.setBounds(1048,paramY+78,170,22);
 generate.setVisible(isSeq);mutate.setVisible(isSeq);fill.setVisible(isSeq);generate.setBounds(w-356,grooveY+8,108,27);mutate.setBounds(w-240,grooveY+8,100,27);fill.setBounds(w-132,grooveY+8,88,27);
 int gw=(w-64)/8;for(int i=0;i<8;i++){genParam[i].setVisible(isSeq);genParam[i].setBounds(28+i*gw,grooveY+34,gw,70);}
 drummerEnable.setVisible(isDrummer);drummerEnable.setBounds(44,205,190,38);drummerStyle.setVisible(isDrummer);hatDivision.setVisible(isDrummer);phraseLength.setVisible(isDrummer);drummerStyle.setBounds(100,272,160,34);hatDivision.setBounds(370,272,170,34);phraseLength.setBounds(640,272,160,34);
 for(int i=0;i<3;i++){drummerParam[i].setVisible(isDrummer);drummerParam[i].setBounds(84+i*215,285,170,105);}
 drummerFillEvery.setVisible(isDrummer);drummerBreakEvery.setVisible(isDrummer);artifactEnable.setVisible(isDrummer);artifactAmount.setVisible(isDrummer);artifactSeed.setVisible(isDrummer);randomizeSeed.setVisible(isDrummer);drummerFillEvery.setBounds(60,455,190,34);drummerBreakEvery.setBounds(280,455,190,34);artifactEnable.setBounds(520,455,175,34);randomizeSeed.setBounds(704,455,112,34);artifactAmount.setBounds(820,448,105,105);artifactSeed.setBounds(520,555,275,24);
 songPatternSelect.setVisible(isSong);songSectionSelect.setVisible(isSong);songBars.setVisible(isSong);songAdd.setVisible(isSong);songRemove.setVisible(isSong);songPlay.setVisible(isSong);songPatternSelect.setBounds(48,210,180,34);songSectionSelect.setBounds(242,210,170,34);songBars.setBounds(428,210,120,34);songAdd.setBounds(560,210,132,34);songRemove.setBounds(702,210,124,34);songPlay.setBounds(836,210,150,34);
 const int songW=(w-160)/4-12;for(int i=0;i<DrumEngine::maxSongEntries;i++){auto&slot=songSlots[(size_t)i];slot.setVisible(isSong);slot.setBounds(80+(i%4)*(songW+12),276+(i/4)*88,songW,76);}

 loadSample.setVisible(isSound&&selected>=8&&selected<=10);loadSample.setBounds(w/2-72,326,144,36);saveKit.setVisible(isSound);loadKit.setVisible(isSound);saveKit.setBounds(34,642,112,34);loadKit.setBounds(154,642,112,34);showSynthKeyboard.setVisible(isSound&&selected==11);showSynthKeyboard.setBounds(284,642,220,34);
 const int whiteW=(w-88)/14,blackW=(int)(whiteW*.58f);const int whiteIndex[]={0,0,1,1,2,3,3,4,4,5,5,6};const int blackAfter[]={0,0,0,0,0,0,3,0,4,0,5,0};
 for(int i=0;i<24;i++){auto&key=synthKeys[(size_t)i];key.setVisible(isSound&&selected==11&&showSynthKeyboard.getToggleState());const int octave=i/12,semitone=i%12;if(semitone==1||semitone==3||semitone==6||semitone==8||semitone==10)key.setBounds(44+(octave*7+blackAfter[semitone]+1)*whiteW-blackW/2,494,blackW,64);else key.setBounds(44+(octave*7+whiteIndex[semitone])*whiteW,494,whiteW-2,98);}
 for(int octave=0;octave<2;octave++)for(int semitone:{1,3,6,8,10})synthKeys[(size_t)(octave*12+semitone)].toFront(false);
 for(int i=0;i<4;i++){soundParam[i].setVisible(isSound&&selected!=11);soundParam[i].setBounds(305+i*165,338,140,120);}
 const int synthW=(w-96)/8;for(int i=0;i<8;i++){synthParam[(size_t)i].setVisible(isSound&&selected==11);synthParam[(size_t)i].setBounds(48+i*synthW,303,synthW-4,134);}
 for(auto*s:{&synthWave1,&synthWave2})s->setVisible(isSound&&selected==11);synthWave1.setBounds(48,450,128,32);synthWave2.setBounds(188,450,128,32);synthOsc2Enable.setVisible(isSound&&selected==11);synthFilterEnable.setVisible(isSound&&selected==11);synthOsc2Enable.setBounds(332,450,100,32);synthFilterEnable.setBounds(440,450,110,32);

 int cw=(w-120)/12;for(int i=0;i<12;i++){int x=92+i*cw;channelGain[i].setVisible(isMix);channelPan[i].setVisible(isMix);channelFilter[i].setVisible(isMix);channelDrive[i].setVisible(isMix);revSend[i].setVisible(isMix);delSend[i].setVisible(isMix);channelMute[i].setVisible(isMix);channelSolo[i].setVisible(isMix);channelGain[i].setBounds(x,156,cw-4,58);channelPan[i].setBounds(x,214,cw-4,58);channelFilter[i].setBounds(x,272,cw-4,58);channelDrive[i].setBounds(x,330,cw-4,58);revSend[i].setBounds(x,388,cw-4,58);delSend[i].setBounds(x,446,cw-4,58);int bw=juce::jmax(20,(cw-10)/2);channelMute[i].setBounds(x+2,518,bw,27);channelSolo[i].setBounds(x+bw+5,518,bw,27);}
 const float cardY=148.0f,cardH=(float)h-166.0f,gap=14.0f,cardW=((float)w-56.0f-2.0f*gap)/3.0f,c0=28.0f,c1=c0+cardW+gap,c2=c1+cardW+gap;
 reverbEnable.setVisible(isFx);delayEnable.setVisible(isFx);delayPingPong.setVisible(isFx);compressorEnable.setVisible(isFx);
 for(int i=0;i<14;i++){fxParam[(size_t)i].setVisible(isFx);const int knobW=(int)(cardW-28)/3;if(i<3)fxParam[(size_t)i].setBounds((int)c0+12+i*knobW,(int)cardY+138,knobW,132);else if(i<6)fxParam[(size_t)i].setBounds((int)c1+12+(i-3)*knobW,(int)cardY+138,knobW,132);else if(i==6||i==7||i==12){const int j=i==6?0:(i==7?1:2);fxParam[(size_t)i].setBounds((int)c2+12+j*knobW,(int)cardY+138,knobW,132);}else{const int compW=(int)(cardW-20)/5;fxParam[(size_t)i].setBounds((int)c2+10+(i-8)*compW,(int)cardY+318,compW,132);}}
 masterDrive.setVisible(isFx);masterDrive.setBounds((int)(c2+cardW*.5f-65), (int)(cardY+cardH-142),130,122);
 saveKit.setVisible(isSound);loadKit.setVisible(isSound);
}

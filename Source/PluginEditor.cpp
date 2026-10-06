#include "PluginEditor.h"
static const char* names[]={"KICK","SNARE","TOM","METAL HAT","FM PERC","PHASE PERC","WAVE METAL","NOISE RESO","SAMPLE 1","SAMPLE 2","SAMPLE 3","SYNTH"};
JerzyDrumMachineAudioProcessorEditor::JerzyDrumMachineAudioProcessorEditor(JerzyDrumMachineAudioProcessor&x):AudioProcessorEditor(&x),p(x){
 setSize(1280,720);setResizable(true,true);setResizeLimits(960,540,1920,1080);
 for(auto*b:{&seq,&sound,&mix,&generate,&mutate,&fill})addAndMakeVisible(*b);
 seq.onClick=[this]{page=0;resized();repaint();};sound.onClick=[this]{page=1;resized();repaint();};mix.onClick=[this]{page=2;resized();repaint();};
 for(int i=0;i<12;i++){instruments[i].setButtonText(names[i]);addAndMakeVisible(instruments[i]);instruments[i].onClick=[this,i]{selected=i;repaint();};}
 for(int s=0;s<16;s++){steps[s].setButtonText(juce::String(s+1));addAndMakeVisible(steps[s]);steps[s].onClick=[this,s]{auto&st=p.engine.pattern(p.engine.getPattern()).step[selected][s];st.on=!st.on;steps[s].setToggleState(st.on,juce::dontSendNotification);};}
 generate.onClick=[this]{p.engine.generate(p.apvts.getRawParameterValue("density")->load(),p.apvts.getRawParameterValue("variation")->load());repaint();};\n mutate.onClick=[this]{p.engine.mutate(p.apvts.getRawParameterValue("variation")->load());repaint();};\n fill.onClick=[this]{p.engine.fill();repaint();};\n startTimerHz(20);
}
void JerzyDrumMachineAudioProcessorEditor::paint(juce::Graphics&g){
 g.fillAll(juce::Colour(0xff090807)); auto r=getLocalBounds().toFloat();g.setColour(juce::Colour(0xff4b2b1d));g.drawRoundedRectangle(r.reduced(6),8,2);
 g.setColour(juce::Colour(0xffc47a38));g.setFont(26);g.drawText("JERZY  DRUM MACHINE",22,12,340,36,juce::Justification::centredLeft);
 g.setFont(14);g.setColour(juce::Colour(0xffd6a45e));g.drawText(page==0?"SEQ / PATTERN":page==1?"SOUND / INSTRUMENT":"MIX / FX",22,82,300,28,juce::Justification::left);
 if(page==1){g.setFont(32);g.drawText(names[selected],420,170,500,50,juce::Justification::centred);g.setFont(16);g.drawText(selected<4?"ANALOG MODEL":selected<8?"DIGITAL SYNTHESIS":selected<11?"SAMPLE ENGINE":"SYNTH VOICE",420,225,500,30,juce::Justification::centred);}
 if(page==2){g.setFont(24);g.drawText("12 CHANNEL MIXER   •   REVERB   •   DELAY   •   MASTER EQ / COMP / DRIVE",220,180,900,60,juce::Justification::centred);}
}
void JerzyDrumMachineAudioProcessorEditor::resized(){
 auto w=getWidth();seq.setBounds(390,18,100,40);sound.setBounds(496,18,100,40);mix.setBounds(602,18,100,40);
 bool isSeq=page==0;for(int i=0;i<12;i++){instruments[i].setVisible(isSeq);instruments[i].setBounds(24,120+i*38,150,30);}
 for(int s=0;s<16;s++){steps[s].setVisible(isSeq);steps[s].setBounds(190+s*((w-230)/16),140,(w-250)/16,300);}
 generate.setVisible(isSeq);mutate.setVisible(isSeq);fill.setVisible(isSeq);generate.setBounds(w-360,610,110,38);mutate.setBounds(w-240,610,100,38);fill.setBounds(w-130,610,90,38);
}

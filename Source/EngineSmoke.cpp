#include <JuceHeader.h>
#include "DrumEngine.h"
#include "DrumMidiBridge.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

int main()
{
 std::ofstream trace("smoke-diagnostics.txt",std::ios::out|std::ios::trunc);
 auto log=[&](const std::string& message){trace<<message<<'\n';trace.flush();std::cerr<<message<<'\n'<<std::flush;};
 try
 {
  juce::ScopedJuceInitialiser_GUI guiInitialiser;
  auto engine=std::make_unique<DrumEngine>();engine->prepare(48000.0);engine->setHost(120.0,false);engine->trigger(0,1.0f);
  juce::AudioBuffer<float> audio(2,8192);engine->process(audio);double energy=0.0;
  for(int channel=0;channel<2;channel++)for(int sample=0;sample<audio.getNumSamples();sample++){const float value=audio.getSample(channel,sample);if(!std::isfinite(value)){log("FAIL: non-finite output");return 1;}energy+=std::abs(value);}
  if(energy<=1.0){log("FAIL: engine produced silence");return 2;}
  if(engine->getChannelMeter(0)<=0.0f){log("FAIL: mixer channel meter did not report activity");return 17;}

  auto renderDrum=[](int kind,int parameter,float value){VoiceDSP voice;voice.prepare(48000.0);voice.tune=.2f;voice.decay=.35f;voice.tone=.25f;voice.character=.2f;float* target=parameter==0?&voice.tune:(parameter==1?&voice.decay:(parameter==2?&voice.tone:&voice.character));*target=value;voice.trigger(.8f,kind);double sum=0.0;for(int i=0;i<8192;i++)sum+=std::abs(voice.process());return sum;};
  for(int kind=0;kind<8;kind++)for(int parameter=0;parameter<4;parameter++){const double low=renderDrum(kind,parameter,.2f),high=renderDrum(kind,parameter,.8f);if(std::abs(low-high)<.01){log("FAIL: analog/digital drum control did not change audio");return 3;}}
  for(int kind:{4,5,6})for(int extra=0;extra<2;extra++){VoiceDSP a,b;a.prepare(48000.0);b.prepare(48000.0);a.trigger(.8f,kind);b.trigger(.8f,kind);if(extra==0){a.extra1=.05f;b.extra1=.95f;if(kind==5){a.updateOpenHatAttack();b.updateOpenHatAttack();}}else{a.extra2=.05f;b.extra2=.95f;}double ea=0,eb=0;for(int i=0;i<24000;i++){ea+=std::abs(a.process());eb+=std::abs(b.process());}if(std::abs(ea-eb)<.05){log("FAIL: expanded FM metal/open hat/floor tom controls did not change audio");return 31;}}
  auto renderHatTail=[](int kind,float decay){VoiceDSP voice;voice.prepare(48000.0);voice.tune=.5f;voice.decay=decay;voice.tone=.55f;voice.character=.65f;voice.trigger(.85f,kind);double sum=0;for(int i=0;i<48000;i++){const float x=voice.process();if(i>=4800)sum+=std::abs(x);}return sum;};
  const double closedHatTail=renderHatTail(3,.5f),openHatTail=renderHatTail(5,.5f),openHatZeroTail=renderHatTail(5,0.0f);if(openHatTail<closedHatTail*2.0||openHatTail<10.0||openHatZeroTail>=openHatTail*.3){log("FAIL: open hat decay/closed-hat contrast is not controllable");return 20;}
  VoiceDSP openHat;openHat.prepare(48000.0);openHat.trigger(.85f,5);if(std::abs(openHat.process())>1.0e-8f){log("FAIL: open hat envelope did not start from zero");return 27;}

  VoiceDSP synth;synth.prepare(48000.0);synth.configureSynth(.1f,1000.0f,1.0f,100.0f,20000.0f,0.0f,0,0,false,0.0f,0.0f,0,false);synth.synthNoteOn(60,1.0f);
  int firstCrossing=-1,lastCrossing=-1,crossings=0;float previous=0.0f;
  for(int i=0;i<48000;i++){const float sample=synth.processSynth();if(previous<=0.0f&&sample>0.0f){if(firstCrossing<0)firstCrossing=i;lastCrossing=i;++crossings;}previous=sample;}
  const double measuredHz=(crossings>1)?(crossings-1)*48000.0/(lastCrossing-firstCrossing):0.0;
  if(std::abs(measuredHz-261.625565)>1.5){log("FAIL: MIDI note 60 is not tuned to C4");return 4;}
  synth.configureSynth(.1f,1000.0f,1.0f,100.0f,20000.0f,.2f,1,2,true,.7f,50.0f,2,true);synth.synthNoteOn(127,1.0f);
  for(int i=0;i<4096;i++)if(!std::isfinite(synth.processSynth())){log("FAIL: high synth notes destabilized oscillator/filter");return 8;}

  PatternStep automatedStep;automatedStep.soundMask=1|2;automatedStep.sound[0]=.95f;automatedStep.sound[1]=.12f;VoiceDSP automated;automated.prepare(48000.0);automated.tune=.1f;automated.decay=.9f;automated.trigger(.9f,0,&automatedStep);if(std::abs(automated.hitTune-.95f)>.001f||std::abs(automated.hitDecay-.12f)>.001f){log("FAIL: sequencer sound overrides were not applied to the hit");return 32;}
  auto hatGrid=std::make_unique<DrumEngine>();hatGrid->prepare(48000.0);for(int division=1;division<=3;division++){hatGrid->setVirtualDrummerSettings(1,division,.65f,.2f,.25f,4,4,8,1);hatGrid->setVirtualDrummer(true);int hatHits=0;for(int i=0;i<16;i++)hatHits+=hatGrid->pattern(0).step[3][i].on?1:0;const int expected=division==1?4:(division==2?8:16);if(hatHits!=expected){log("FAIL: virtual drummer hi-hat division did not match the selected grid");return 33;}hatGrid->setVirtualDrummer(false);}
  hatGrid->pattern(0).step[0][0].on=true;hatGrid->clearSequence();for(const auto& track:hatGrid->pattern(0).step)for(const auto& item:track)if(item.on){log("FAIL: Clear Seq left active notes");return 34;}


  SampleSlot sampleSlot;sampleSlot.audio.setSize(1,4096);sampleSlot.audio.clear();for(int i=0;i<4096;i++)sampleSlot.audio.setSample(0,i,1.0f);sampleSlot.loaded=true;sampleSlot.trigger(1.625f,1,0.0f,0.0f);sampleSlot.process(48000.0);if(std::abs(sampleSlot.playbackPitch-1.625f)>.001f||!sampleSlot.playbackReverse||!sampleSlot.decayEnabled||!sampleSlot.toneEnabled||sampleSlot.playbackToneAlpha>.1f){log("FAIL: sample pitch, decay, tone, and reverse step controls did not apply");return 36;}
  PatternStep synthStep;synthStep.soundMask=1|2|4|8;synthStep.sound[0]=1.0f;synthStep.sound[1]=.75f;synthStep.sound[2]=.25f;synthStep.sound[3]=.5f;VoiceDSP synthAutomation;synthAutomation.prepare(48000.0);synthAutomation.synthDecayMs=180.0f;synthAutomation.sustain=.72f;synthAutomation.synthNoteOn(60,1.0f,&synthStep);if(synthAutomation.midiNote!=60||synthAutomation.hitSustain!=.25f||std::abs(synthAutomation.attackStep-1.0f/(1000.0f*.001f*48000.0f))>.000001f){log("FAIL: synth step ADSR controls were not applied");return 35;}
  auto rhythm=std::make_unique<DrumEngine>();rhythm->prepare(48000.0);rhythm->setHost(120.0,false);rhythm->pattern(0).length=64;rhythm->setTrackLength(0,0,3);rhythm->setTrackLength(0,1,5);rhythm->setPreviewPlaying(true);juce::AudioBuffer<float> rhythmAudio(2,36001);rhythm->process(rhythmAudio);
  if(rhythm->getTrackStep(0)!=0||rhythm->getTrackStep(1)!=1){log("FAIL: independent track loops did not advance at separate lengths");return 5;}
  auto triplets=std::make_unique<DrumEngine>();triplets->prepare(48000.0);triplets->setHost(120.0,false);triplets->pattern(0).length=64;triplets->setTripletSubdivision(true);triplets->setPreviewPlaying(true);juce::AudioBuffer<float> tripletAudio(2,24000);triplets->process(tripletAudio);if(triplets->getCurrentStep()!=5){log("FAIL: triplet grid did not advance six divisions per beat");return 18;}
  auto independentTriplets=std::make_unique<DrumEngine>();independentTriplets->prepare(48000.0);independentTriplets->setHost(120.0,false);independentTriplets->pattern(0).length=64;independentTriplets->setTrackTriplet(1,true);independentTriplets->setTripletSubdivision(true);independentTriplets->setPreviewPlaying(true);juce::AudioBuffer<float> independentTripletAudio(2,24000);independentTriplets->process(independentTripletAudio);if(independentTriplets->getTrackStep(0)!=3||independentTriplets->getTrackStep(1)!=5){log("FAIL: per-track triplet timing did not diverge independently");return 28;}
  auto preview=std::make_unique<DrumEngine>();preview->prepare(48000.0);preview->setHost(120.0,false);preview->setReverbEnabled(false);preview->setDelayEnabled(false);preview->previewSynthNote(60,1.0f);juce::AudioBuffer<float> previewAudio(2,129600);preview->process(previewAudio);double previewTail=0;for(int c=0;c<2;c++)for(int i=110400;i<129600;i++)previewTail+=std::abs(previewAudio.getSample(c,i));if(previewTail>.1){log("FAIL: synth click preview did not end after its short gate and release tail");return 19;}
  auto quickKey=std::make_unique<DrumEngine>();quickKey->prepare(48000.0);quickKey->setHost(120.0,false);quickKey->requestSynthNoteOn(60,1.0f);quickKey->requestSynthNoteOff(60);juce::AudioBuffer<float> quickKeyAudio(2,512);quickKey->process(quickKeyAudio);double quickKeyEnergy=0;for(int c=0;c<2;c++)for(int i=0;i<512;i++)quickKeyEnergy+=std::abs(quickKeyAudio.getSample(c,i));if(quickKeyEnergy<.1){log("FAIL: rapid GUI synth note-on/note-off produced no audio");return 25;}
  auto filteredEnergy=[](float position){DrumEngine engine;engine.prepare(48000.0);engine.setMasterFilter(position);engine.setHost(120.0,false);engine.trigger(0,1.0f);juce::AudioBuffer<float> audio(2,8192);engine.process(audio);double energy=0;for(int c=0;c<2;c++)for(int i=0;i<audio.getNumSamples();i++)energy+=std::abs(audio.getSample(c,i));return energy;};const double filterNeutral=filteredEnergy(.5f),filterHighPass=filteredEnergy(1.0f);if(!(filterNeutral>filterHighPass*1.5)){log("FAIL: master DJ high-pass did not change the signal");return 26;}

  auto drummer=std::make_unique<DrumEngine>();drummer->prepare(48000.0);drummer->setVirtualDrummerSettings(1,2,.65f,.2f,.25f,4,4,8,3);drummer->setVirtualDrummer(true);
  auto&electronic=drummer->pattern(0);for(int s:{0,4,8,12})if(!electronic.step[0][s].on){log("FAIL: electronic drummer lost four-on-the-floor kick anchors");return 9;}
  for(int s:{4,12})if(!electronic.step[1][s].on){log("FAIL: virtual drummer lost backbeat anchors");return 12;}
  auto before=electronic.step;drummer->setVirtualDrummerSettings(1,2,.65f,.9f,.8f,4,4,8,3);if(drummer->pattern(0).step[0][0].on!=before[0][0].on||drummer->pattern(0).step[1][4].on!=before[1][4].on){log("FAIL: drummer controls destabilized core groove");return 13;}

  auto kickGrid=std::make_unique<DrumEngine>();kickGrid->prepare(48000.0);kickGrid->setVirtualDrummerSettings(1,2,.65f,.2f,.25f,4,4,8,1);kickGrid->setVirtualDrummer(true);int sequenceKicks=0;for(int i=0;i<16;i++)sequenceKicks+=kickGrid->pattern(0).step[0][i].on?1:0;if(sequenceKicks!=1||!kickGrid->pattern(0).step[0][0].on){log("FAIL: default kick division is not one kick per 16-step sequence");return 29;}kickGrid->setVirtualDrummerSettings(1,2,.65f,.2f,.25f,4,4,8,2);int halfSequenceKicks=0;for(int i=0;i<16;i++)halfSequenceKicks+=kickGrid->pattern(0).step[0][i].on?1:0;if(halfSequenceKicks!=2||!kickGrid->pattern(0).step[0][8].on){log("FAIL: kick division did not place two kicks in the sequence");return 30;}

  engine->pattern(0).length=64;engine->pattern(0).step[0][7].soundMask=5;engine->pattern(0).step[0][7].sound[0]=.91f;engine->pattern(0).step[0][7].sound[2]=.22f;engine->pattern(0).step[11][63].note=72;engine->pattern(0).step[11][63].on=true;engine->setTrackLength(0,0,7);engine->setTripletSubdivision(true);engine->setTrackTriplet(2,true);engine->setVirtualDrummerSettings(1,2,.65f,.2f,.25f,4,4,8,3);engine->setSynthTranspose(-2);engine->masterMid=.62f;engine->setSongEntry(0,0,0,8);engine->setSongEntry(1,1,2,3);engine->gain[0]=.42f;engine->panorama[0]=-.25f;engine->setVoiceExtraParam(5,0,.72f);engine->setVoiceExtraParam(5,1,.83f);
  auto state=engine->saveState();auto restored=std::make_unique<DrumEngine>();restored->prepare(48000.0);restored->loadState(state);
  if(std::abs(restored->gain[0]-.42f)>.001f||std::abs(restored->panorama[0]+.25f)>.001f||restored->pattern(0).step[11][63].note!=72||restored->pattern(0).step[0][7].soundMask!=5||std::abs(restored->pattern(0).step[0][7].sound[0]-.91f)>.001f||restored->pattern(0).step[0][7].soundMask!=5||std::abs(restored->pattern(0).step[0][7].sound[0]-.91f)>.001f||restored->pattern(0).length!=64||restored->pattern(0).trackLength[0]!=7||!restored->isTripletSubdivision()||!restored->getTrackTriplet(2)||restored->getSynthTranspose()!=-2||std::abs(restored->masterMid-.62f)>.001f||std::abs(restored->getVoiceExtraParam(5,0)-.72f)>.001f||std::abs(restored->getVoiceExtraParam(5,1)-.83f)>.001f){log("FAIL: engine state did not restore");return 6;}
  if(restored->getSongLength()!=2||restored->getSongEntry(0).bars!=8||restored->getSongEntry(1).pattern!=1||restored->getSongEntry(1).section!=2){log("FAIL: song chain did not restore section lengths");return 7;}

  auto processor=std::make_unique<JerzyDrumMachineAudioProcessor>();processor->prepareToPlay(48000.0,512);juce::AudioBuffer<float> pluginAudio(2,512);juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);processor->processBlock(pluginAudio,midi);double pluginEnergy=0;for(int i=0;i<pluginAudio.getNumSamples();i++)pluginEnergy+=std::abs(pluginAudio.getSample(0,i))+std::abs(pluginAudio.getSample(1,i));if(pluginEnergy<.01){log("FAIL: plug-in did not play MIDI channel 1 C4");return 14;}
  std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());auto visibleChildren=[&]{int count=0;for(int i=0;i<editor->getNumChildComponents();++i)if(editor->getChildComponent(i)->isVisible())++count;return count;};if(visibleChildren()<6||visibleChildren()>140){log("FAIL: editor did not isolate the initial page controls");return 15;}
  // Regression for actual GUI handlers: COPY 1-16, select BANK 17-32, PASTE 16.
  {
    auto& pat=processor->engine.pattern(0);
    pat.step[0][0].on=true;pat.step[0][0].velocity=.71f;
    pat.step[1][0].on=true;
    auto clickText=[&](const juce::String& label)->bool {
      for(int i=0;i<editor->getNumChildComponents();++i)
        if(auto* b=dynamic_cast<juce::Button*>(editor->getChildComponent(i));
            b&&b->getButtonText()==label){
          b->triggerClick();
          // JUCE delivers Button::triggerClick via the MessageManager queue.
          // The smoke test is a console app, so pump messages explicitly.
          juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
          return true;
        }
      return false;
    };
    const bool copied=clickText("COPY 16");
    const bool selectedDestination=clickText("17-32");
    const bool pasted=clickText("PASTE 16");
    if(!copied||!selectedDestination||!pasted||
       !pat.step[0][16].on||std::abs(pat.step[0][16].velocity-.71f)>.001f){
       log("FAIL: GUI Copy 1-16 -> Paste 17-32 (copy="+std::to_string(copied)+
           ", bank="+std::to_string(selectedDestination)+", paste="+std::to_string(pasted)+
           ", note="+std::to_string(pat.step[0][16].on)+
           ", velocity="+std::to_string(pat.step[0][16].velocity)+")");return 56;
    }
    if(!clickText("CLEAR SEQ")||pat.step[0][0].on||pat.step[0][16].on||!pat.step[1][0].on){
       log("FAIL: GUI CLEAR TRACK affected other voice");return 57;
    }
    if(!clickText("CLEAR: TRACK")||!clickText("CLEAR SEQ")||pat.step[1][0].on){
       log("FAIL: GUI CLEAR ALL did not clear all voices");return 58;
    }
  }
  bool joinedBanks=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(editor->getChildComponent(i));button&&button->getComponentID()=="joinBanks64"){joinedBanks=true;break;}if(auto*editorView=dynamic_cast<JerzyDrumMachineAudioProcessorEditor*>(editor.get()))editorView->joinBanksTo64Steps();else joinedBanks=false;if(!joinedBanks||processor->engine.pattern(0).length!=64){log("FAIL: GUI bank join did not make a 64-step pattern");return 21;}for(int v=0;v<DrumEngine::voices;v++)if(processor->engine.pattern(0).trackLength[(size_t)v]!=64){log("FAIL: joined 64-step pattern did not extend default instrument tracks");return 22;}
  for(const auto*pageName:{"SEQ","SOUND","MIX","FX","DRUMMER","SONG"}){bool clicked=false;for(int i=0;i<editor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(editor->getChildComponent(i));button&&button->getButtonText()==pageName){button->triggerClick();clicked=true;break;}if(!clicked||visibleChildren()>140){log("FAIL: page navigation exposed overlapping layouts");return 16;}}
  // GUI regression: both new effect toggles and a five-mode delay must be
  // visible in the FX rack (not just bound as hidden host parameters).
  {
    bool clickedFX=false,hasChorus=false,hasTape=false,hasModes=false;
    for(int i=0;i<editor->getNumChildComponents();++i)
        if(auto* b=dynamic_cast<juce::Button*>(editor->getChildComponent(i));
           b&&b->getButtonText()=="FX"){b->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(25);clickedFX=true;break;}
    for(int i=0;i<editor->getNumChildComponents();++i){
        auto* component=editor->getChildComponent(i);
        if(auto* b=dynamic_cast<juce::Button*>(component)){
            hasChorus|=b->isVisible()&&b->getButtonText()=="CHORUS ON";
            hasTape|=b->isVisible()&&b->getButtonText()=="TAPE ON";
        }
        if(auto* combo=dynamic_cast<juce::ComboBox*>(component))
            hasModes|=combo->isVisible()&&combo->getNumItems()==5;
    }
    if(!clickedFX||!hasChorus||!hasTape||!hasModes){
        log("FAIL: FX GUI missing Chorus, Tape or Delay modes");return 59;
    }
  }
  auto auditionProcessor=std::make_unique<JerzyDrumMachineAudioProcessor>();auditionProcessor->prepareToPlay(48000.0,512);std::unique_ptr<juce::AudioProcessorEditor> auditionEditor(auditionProcessor->createEditor());bool synthClicked=false;for(int i=0;i<auditionEditor->getNumChildComponents();++i)if(auto*button=dynamic_cast<juce::Button*>(auditionEditor->getChildComponent(i));button&&button->getButtonText()=="SYNTH"){button->triggerClick();synthClicked=true;break;}if(!synthClicked){log("FAIL: could not click synth preview in the editor");return 23;}juce::AudioBuffer<float> auditionAudio(2,512);juce::MidiBuffer noMidi;double auditionEnergy=0;for(int block=0;block<48;block++){auditionProcessor->processBlock(auditionAudio,noMidi);for(int c=0;c<2;c++)for(int i=0;i<auditionAudio.getNumSamples();i++)auditionEnergy+=std::abs(auditionAudio.getSample(c,i));}if(auditionEnergy<1.0){log("FAIL: synth GUI audition produced no plug-in output");return 24;}
  // Bank copy MUST paste only a bank, not replace a 64-step track.
  {
    auto seq=std::make_unique<DrumEngine>();seq->prepare(48000.0);
    auto& p=seq->pattern(0);
    p.step[0][0].on=true;p.step[0][2].on=true;p.step[0][2].ratchet=3;
    p.step[0][18].on=false;p.step[0][32].on=true;
    const auto copy=seq->copyBank(0,0,0);
    if(!seq->pasteBank(0,0,1,copy) || !p.step[0][16].on ||
       !p.step[0][18].on||p.step[0][18].ratchet!=3||
       !p.step[0][32].on||p.trackLength[0]<32){log("FAIL: COPY 1-16 to 17-32 lost data");return 50;}
    p.step[1][0].on=true;
    seq->clearVoice(0);
    if(p.step[0][0].on||p.step[0][18].on||!p.step[1][0].on){
        log("FAIL: CLEAR TRACK deleted another instrument or left notes");return 51;
    }
    seq->clearSequence();
    if(p.step[1][0].on){log("FAIL: CLEAR ALL leaves active drum");return 52;}
  }
  // Stereo FX must produce finite sound and audible modulation without
  // audio-thread allocation (all buffers are allocated by prepare).
  {
    DrumExtendedFX fx;fx.prepare(48000.0);
    double energy=0,stereoDifference=0,tapeEnergy=0;
    for(int k=0;k<24000;k++){
        const float x=(float)(.18*std::sin(juce::MathConstants<double>::twoPi*330.0*k/48000.0));
        const auto c=fx.chorus(x,x,.8f,.95f);
        const auto t=fx.tape(x,x,.8f,.6f,.42f);
        for(auto y:{c[0],c[1],t[0],t[1]})if(!std::isfinite(y)||std::abs(y)>8.0f){
            log("FAIL: Chorus/Tape FX produced non-finite or unbounded output");return 53;
        }
        energy+=std::abs(c[0])+std::abs(c[1]);
        stereoDifference+=std::abs(c[0]-c[1]);
        tapeEnergy+=std::abs(t[0])+std::abs(t[1]);
    }
    if(energy<10.0||stereoDifference<.1||tapeEnergy<10.0){
        log("FAIL: Chorus width or Tape transport returned silence");return 54;
    }
  }
  {
    auto fxSeq=std::make_unique<DrumEngine>();fxSeq->prepare(48000.0);
    fxSeq->delaySend[0]=1.f;fxSeq->setDelay(.25f,.8f,.7f);fxSeq->setDelayEnabled(true);
    for(int mode=0;mode<5;++mode){
        fxSeq->setDelayMode(mode);fxSeq->trigger(0,.9f);
        juce::AudioBuffer<float> audio(2,4096);
        for(int block=0;block<12;++block){
            fxSeq->process(audio);
            for(int c=0;c<2;++c)for(int i=0;i<4096;++i)
                if(!std::isfinite(audio.getSample(c,i))){log("FAIL: Delay mode not finite");return 55;}
        }
    }
  }
  // MIDI interoperability must preserve existing independent grid choices,
  // velocity and pitched synth note data without mutating a saved kit.
  {
    auto midiEngine=std::make_unique<DrumEngine>();
    midiEngine->prepare(48000.0);
    auto& pat=midiEngine->pattern(0);
    pat.trackLength[0]=16;
    pat.step[0][2].on=true;pat.step[0][2].velocity=.85f;
    pat.trackTriplet[5]=true;pat.trackLength[5]=24;
    pat.step[5][1].on=true;pat.step[5][1].velocity=.77f;
    pat.step[5][2].on=true;pat.step[5][2].velocity=.67f;
    pat.step[11][0].on=true;pat.step[11][0].note=67;
    auto midiOut=juce::File::getSpecialLocation(juce::File::tempDirectory)
       .getNonexistentChildFile("jerzy_full_drum_",".mid",false);
    if(!JerzyDrumMidi::exportPattern(*midiEngine,0,midiOut)){log("FAIL: full sequencer MIDI export");return 41;}
    auto imported=std::make_unique<DrumEngine>();imported->prepare(48000.0);
    const bool roundtrip=JerzyDrumMidi::importPattern(*imported,0,midiOut);
    midiOut.deleteFile();
    const auto& back=imported->pattern(0);
    if(!roundtrip||!back.step[0][2].on||!back.trackTriplet[5]||!back.step[5][1].on||
       !back.step[5][2].on||back.step[11][0].note!=67){
        log("FAIL: full drum MIDI roundtrip loses independent triplets or synth pitch");return 42;
    }
    log("PASS: MIDI per-voice triplet and synth pitch roundtrip");
  }
  log("PASS: analog/digital controls audible, open/closed hat tails distinct, short synth preview and GUI synth audition audible, octave tuning and C4 stable, mixer meters active, 64-step bank join and triplet state work, plug-in MIDI audible, page layouts isolated, drummer anchors stable, selectable hat grid, per-step sound automation and Clear Seq, polyrhythm advances independently, state round-trip");return 0;
 }
 catch(const std::exception& error){log(std::string("FAIL exception: ")+error.what());return 10;}
 catch(...){log("FAIL unknown exception");return 11;}
}

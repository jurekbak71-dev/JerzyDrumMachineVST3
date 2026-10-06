#pragma once
#include <atomic>
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <memory>

struct PatternStep { bool on=false; float velocity=.85f, probability=1.0f; int ratchet=1,note=60; float micro=0.0f; bool accent=false,flam=false; };
struct Pattern { std::array<std::array<PatternStep,64>,12> step{}; int length=16; };
struct SongEntry { int pattern=0,section=0; };

struct SampleSlot { juce::AudioBuffer<float> audio; double sourceRate=44100.0,pos=0.0; bool loaded=false,active=false,reverse=false; juce::String sourcePath; float pitch=1.0f,level=1.0f; void trigger(){if(loaded){active=true;pos=reverse?(double)audio.getNumSamples()-1.0:0.0;}} float process(double outRate){if(!loaded||!active||audio.getNumSamples()<2)return 0;int i=juce::jlimit(0,audio.getNumSamples()-2,(int)pos);float frac=(float)(pos-i);float x=audio.getSample(0,i)*(1-frac)+audio.getSample(0,i+1)*frac;double inc=(sourceRate/outRate)*pitch*(reverse?-1.0:1.0);pos+=inc;if(pos<0||pos>=audio.getNumSamples()-1)active=false;return x*level;} };

struct VoiceDSP {
 double sr=44100.0, phase=0, phase2=0; float env=0, noiseEnv=0, clickEnv=0, pitchEnv=0, velocity=0; int kind=0,midiNote=60; float tune=0.5f,decay=0.5f,tone=0.5f,character=0.5f; juce::Random rng;
 void prepare(double s){sr=s;}
 void trigger(float v,int k){kind=k;velocity=v;env=v;noiseEnv=v;clickEnv=v;pitchEnv=v;phase=phase2=0;}
 float process(){
  if(env<1.0e-5f && noiseEnv<1.0e-5f)return 0.0f;
  const float n=rng.nextFloat()*2.0f-1.0f; double f=80.0;
  float tuneMul=std::pow(2.0f,(tune-.5f)*2.0f);
  switch(kind){
   case 0:f=32.0+68.0*tune+pitchEnv*(90.0+character*230.0);break;
   case 1:f=145.0+110.0*tune;break;
   case 2:f=62.0+180.0*tune+pitchEnv*(45.0+character*190.0);break;
   case 3:f=2700.0+6300.0*tune;break;
   case 4:f=155.0;break; case 5:f=240.0;break; case 6:f=330.0;break; case 7:f=520.0;break;
   case 8:f=105.0;break; case 9:f=175.0;break; case 10:f=260.0;break;
   default:f=440.0*std::pow(2.0,(midiNote-69)/12.0);break;
  }
  f*=tuneMul; phase+=juce::MathConstants<double>::twoPi*f/sr; phase2+=juce::MathConstants<double>::twoPi*f*(kind>=4&&kind<=7?(1.05+character*1.2):1.4+character*1.2)/sr;
  if(phase>juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
  if(phase2>juce::MathConstants<double>::twoPi)phase2-=juce::MathConstants<double>::twoPi;
  float a=(float)std::sin(phase),b=(float)std::sin(phase2),x=0;
  if(kind==0)x=a*(.92f+.12f*character)+n*clickEnv*tone*.75f;
  else if(kind==1)x=(a*(.16f+.58f*tone)+b*.12f)+n*noiseEnv*(.22f+character*1.25f);
  else if(kind==2)x=a*.76f+b*(.10f+.62f*tone);
  else if(kind==3)x=n*noiseEnv*(.25f+.45f*tone)+(a*b)*(.10f+character*.75f);
  else if(kind==4)x=std::sin((float)phase+b*(1.0f+character*7.0f));
  else if(kind==5)x=std::tanh((a+b*(.2f+tone))* (1.2f+character*3.0f));
  else if(kind==6)x=(a*.45f+b*.35f+n*.20f);
  else if(kind==7)x=n*.55f+std::sin((float)phase+n*.9f)*.45f;
  else if(kind>=8&&kind<=10)x=(a*.55f+n*.45f); // sample slots have distinct fallback synthesis until a WAV is loaded
  else x=a*.7f+b*.3f;
  float d=juce::jmap(decay,0.0f,1.0f,0.965f,0.99975f); env*= kind==3?juce::jmin(d,.992f):d; noiseEnv*=juce::jmap(decay,0.0f,1.0f,.94f,.995f);clickEnv*=.82f;pitchEnv*=kind==0?.975f:.94f;
  // Apply the amplitude envelope to the pitched component as well as the noise.
  // Without this, Decay only affected the noise tail and oscillator voices ran
  // at full level until the envelope suddenly crossed the silence threshold.
  return std::tanh(x*env*1.65f);
 }
};

class DrumEngine {
public:
 enum class ChangeMode{Immediate=0,NextBeat,NextBar,EndPattern};
 static constexpr int voices=12,patterns=32;
 DrumEngine(){for(auto&patternIndex:audioSongPattern)patternIndex.store(0,std::memory_order_relaxed);}
 void prepare(double s){sr=s;for(auto&v:dsp)v.prepare(s);reverb.setSampleRate(s);juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=reverbEnabled?reverbMix:0.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);delayL.assign((size_t)(s*2.0),0.f);delayR.assign((size_t)(s*2.0),0.f);initialiseFactoryPatterns();}
 void trigger(int i,float v){if(i>=0&&i<voices){if(i>=8&&i<=10&&samples[(size_t)(i-8)].loaded){sampleVelocity[(size_t)(i-8)]=v;samples[(size_t)(i-8)].trigger();}else dsp[(size_t)i].trigger(v,i);}}
 void triggerSynthNote(int note,float velocity){dsp[11].midiNote=juce::jlimit(0,127,note);trigger(11,velocity);}
 int getSynthMidiNote()const{return dsp[11].midiNote;}
 void setPattern(int p){current=juce::jlimit(0,patterns-1,p);step=-1;samplesToStep=0;hasPpq=false;}
 void setPreviewPlaying(bool shouldPlay){previewPlaying.store(shouldPlay,std::memory_order_relaxed);}
 void setVirtualDrummer(bool enabled){if(enabled==virtualDrummerEnabled)return;virtualDrummerEnabled=enabled;generatedBars=0;if(enabled){savedPattern=pattern(current);pattern(current).length=juce::jmax(16,pattern(current).length);buildVirtualBar(0);}else pattern(current)=savedPattern;}
 void setVirtualDrummerSettings(int styleIn,int divisionIn,float energyIn,float humanizeIn,float syncopationIn,int phraseBarsIn){drummerStyle=juce::jlimit(0,4,styleIn);hatDivision=divisionIn==1?1:(divisionIn==2?2:4);drummerEnergy=juce::jlimit(0.f,1.f,energyIn);drummerHumanize=juce::jlimit(0.f,1.f,humanizeIn);drummerSyncopation=juce::jlimit(0.f,1.f,syncopationIn);phraseBars=phraseBarsIn>=8?8:(phraseBarsIn<=2?2:4);if(virtualDrummerEnabled)buildVirtualBar(generatedBars);}
 bool isVirtualDrummerEnabled()const{return virtualDrummerEnabled;}
 static constexpr int maxSongEntries=16;
 int getSongLength()const{return songLength;}
 int getSongPosition()const{return songPosition.load(std::memory_order_relaxed);}
 SongEntry getSongEntry(int slot)const{return slot>=0&&slot<maxSongEntries?songChain[(size_t)slot]:SongEntry{};}
 bool isSongPlaying()const{return songPlaying;}
 void setSongEntry(int slot,int patternIndex,int section){if(slot<0||slot>=maxSongEntries)return;songChain[(size_t)slot]={juce::jlimit(0,patterns-1,patternIndex),juce::jlimit(0,4,section)};songLength=juce::jmax(songLength,slot+1);audioSongPattern[(size_t)slot].store(songChain[(size_t)slot].pattern,std::memory_order_relaxed);activeSongLength.store(songLength,std::memory_order_relaxed);}
 void removeSongEntry(int slot){if(slot<0||slot>=songLength)return;for(int i=slot;i<songLength-1;i++){songChain[(size_t)i]=songChain[(size_t)(i+1)];audioSongPattern[(size_t)i].store(songChain[(size_t)i].pattern,std::memory_order_relaxed);}--songLength;activeSongLength.store(songLength,std::memory_order_relaxed);if(songLength==0){songPlaying=false;songPosition.store(0,std::memory_order_relaxed);}else songPosition.store(juce::jlimit(0,songLength-1,getSongPosition()),std::memory_order_relaxed);}
 void setSongPlaying(bool shouldPlay){songPlaying=shouldPlay&&songLength>0;if(songPlaying){songPosition.store(0,std::memory_order_relaxed);setPattern(audioSongPattern[0].load(std::memory_order_relaxed));}}
 void requestPattern(int p){p=juce::jlimit(0,patterns-1,p);if(songPlaying)setSongPlaying(false);if(changeMode==ChangeMode::Immediate)setPattern(p);else pendingPattern=p;}
 void setChangeMode(ChangeMode m){changeMode=m;}
 ChangeMode getChangeMode()const{return changeMode;}
 int getPattern()const{return current;} int getCurrentStep()const{return step;}
 Pattern& pattern(int p){return pats[(size_t)juce::jlimit(0,patterns-1,p)];}
 void setHost(double b,bool play){bpm=b>20?b:120;hostPlaying=play;} void setHostPpq(double ppq){hostPpq=ppq;hasPpq=true;}
 void setSwing(float s){swing=juce::jlimit(0.0f,.75f,s);}
 void setReverb(float size,float damping,float mix){reverbSize=juce::jlimit(0.f,1.f,size);reverbDamping=juce::jlimit(0.f,1.f,damping);reverbMix=juce::jlimit(0.f,1.f,mix);juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=reverbEnabled?reverbMix:0.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);}
 void setReverbEnabled(bool enabled){reverbEnabled=enabled;setReverb(reverbSize,reverbDamping,reverbMix);}
 void setDelay(float beats,float feedback,float mix){delayBeats=juce::jlimit(.125f,2.0f,beats);delayFeedback=juce::jlimit(0.f,.88f,feedback);delayMix=juce::jlimit(0.f,1.f,mix);}
 void setDelayEnabled(bool enabled){delayEnabled=enabled;}
 void setDelayPingPong(bool enabled){delayPingPong=enabled;}
 void setCompressor(bool enabled,float thresholdDb,float ratio,float attackMs,float releaseMs){compressorEnabled=enabled;compressorThresholdDb=juce::jlimit(-36.f,0.f,thresholdDb);compressorRatio=juce::jlimit(1.f,20.f,ratio);compressorAttackMs=juce::jlimit(.1f,100.f,attackMs);compressorReleaseMs=juce::jlimit(10.f,500.f,releaseMs);}
 void setMaster(float bass,float treble,float comp){masterBass=juce::jlimit(.4f,1.8f,bass);masterTreble=juce::jlimit(.4f,1.8f,treble);masterComp=juce::jlimit(.6f,3.0f,comp);}
 void setVoiceParam(int i,int param,float value){if(i<0||i>=voices)return;value=juce::jlimit(0.0f,1.0f,value);auto&v=dsp[(size_t)i];if(param==0)v.tune=value;else if(param==1)v.decay=value;else if(param==2)v.tone=value;else if(param==3)v.character=value;if(i>=8&&i<=10){auto&s=samples[(size_t)(i-8)];if(param==0)s.pitch=juce::jmap(value,0.0f,1.0f,.5f,2.0f);else if(param==3)s.reverse=value>.75f;}}
 float getVoiceParam(int i,int param)const{if(i<0||i>=voices)return .5f;auto const&v=dsp[(size_t)i];return param==0?v.tune:param==1?v.decay:param==2?v.tone:v.character;}
 juce::ValueTree saveState()const{juce::ValueTree root("ENGINE");root.setProperty("pattern",current,nullptr);root.setProperty("changeMode",(int)changeMode,nullptr);root.setProperty("masterBass",masterBass,nullptr);root.setProperty("masterTreble",masterTreble,nullptr);root.setProperty("masterComp",masterComp,nullptr);root.setProperty("reverbSize",reverbSize,nullptr);root.setProperty("reverbDamping",reverbDamping,nullptr);root.setProperty("reverbMix",reverbMix,nullptr);root.setProperty("reverbEnabled",reverbEnabled,nullptr);root.setProperty("delayBeats",delayBeats,nullptr);root.setProperty("delayFeedback",delayFeedback,nullptr);root.setProperty("delayMix",delayMix,nullptr);root.setProperty("delayEnabled",delayEnabled,nullptr);root.setProperty("delayPingPong",delayPingPong,nullptr);root.setProperty("compressorEnabled",compressorEnabled,nullptr);root.setProperty("compressorThresholdDb",compressorThresholdDb,nullptr);root.setProperty("compressorRatio",compressorRatio,nullptr);root.setProperty("compressorAttackMs",compressorAttackMs,nullptr);root.setProperty("compressorReleaseMs",compressorReleaseMs,nullptr);for(int v=0;v<voices;v++){juce::ValueTree voice("VOICE");voice.setProperty("index",v,nullptr);if(v>=8&&v<=10)voice.setProperty("samplePath",samples[(size_t)(v-8)].sourcePath,nullptr);voice.setProperty("gain",gain[v],nullptr);voice.setProperty("pan",panorama[v],nullptr);voice.setProperty("rev",reverbSend[v],nullptr);voice.setProperty("del",delaySend[v],nullptr);voice.setProperty("filter",channelFilter[v],nullptr);voice.setProperty("drive",channelDrive[v],nullptr);voice.setProperty("mute",mute[v],nullptr);voice.setProperty("solo",solo[v],nullptr);voice.setProperty("tune",dsp[v].tune,nullptr);voice.setProperty("decay",dsp[v].decay,nullptr);voice.setProperty("tone",dsp[v].tone,nullptr);voice.setProperty("char",dsp[v].character,nullptr);root.addChild(voice,-1,nullptr);}for(int pidx=0;pidx<patterns;pidx++){juce::ValueTree pat("PATTERN");pat.setProperty("index",pidx,nullptr);pat.setProperty("length",pats[pidx].length,nullptr);for(int v=0;v<voices;v++)for(int s=0;s<64;s++){auto const&st=pats[pidx].step[v][s];if(st.on||st.velocity!=.85f||st.probability!=1.f||st.ratchet!=1||st.micro!=0.f||st.accent||st.flam||st.note!=60){juce::ValueTree n("STEP");n.setProperty("v",v,nullptr);n.setProperty("s",s,nullptr);n.setProperty("on",st.on,nullptr);n.setProperty("vel",st.velocity,nullptr);n.setProperty("prob",st.probability,nullptr);n.setProperty("rat",st.ratchet,nullptr);n.setProperty("micro",st.micro,nullptr);n.setProperty("accent",st.accent,nullptr);n.setProperty("flam",st.flam,nullptr);n.setProperty("note",st.note,nullptr);pat.addChild(n,-1,nullptr);}}root.addChild(pat,-1,nullptr);}juce::ValueTree song("SONG");song.setProperty("length",songLength,nullptr);for(int i=0;i<songLength;i++){juce::ValueTree entry("ENTRY");entry.setProperty("pattern",songChain[(size_t)i].pattern,nullptr);entry.setProperty("section",songChain[(size_t)i].section,nullptr);song.addChild(entry,-1,nullptr);}root.addChild(song,-1,nullptr);return root;}
 void loadState(const juce::ValueTree& root)
 {
  if(!root.isValid())return;
  current=(int)root.getProperty("pattern",0);
  changeMode=(ChangeMode)(int)root.getProperty("changeMode",(int)ChangeMode::EndPattern);
  masterBass=(float)root.getProperty("masterBass",1.0);
  masterTreble=(float)root.getProperty("masterTreble",1.0);
  masterComp=(float)root.getProperty("masterComp",1.35);
  reverbSize=(float)root.getProperty("reverbSize",.45);
  reverbDamping=(float)root.getProperty("reverbDamping",.55);
  reverbMix=(float)root.getProperty("reverbMix",.35);
  reverbEnabled=(bool)root.getProperty("reverbEnabled",true);
  delayBeats=(float)root.getProperty("delayBeats",.75);
  delayFeedback=(float)root.getProperty("delayFeedback",.36);
  delayMix=(float)root.getProperty("delayMix",.45);
  delayEnabled=(bool)root.getProperty("delayEnabled",true);
  delayPingPong=(bool)root.getProperty("delayPingPong",true);
  compressorEnabled=(bool)root.getProperty("compressorEnabled",true);
  compressorThresholdDb=(float)root.getProperty("compressorThresholdDb",-18.0);
  compressorRatio=(float)root.getProperty("compressorRatio",3.0);
  compressorAttackMs=(float)root.getProperty("compressorAttackMs",10.0);
  compressorReleaseMs=(float)root.getProperty("compressorReleaseMs",120.0);
  setReverb(reverbSize,reverbDamping,reverbMix);

  songLength=0;
  activeSongLength.store(0,std::memory_order_relaxed);
  songPosition.store(0,std::memory_order_relaxed);
  songPlaying=false;

  for(auto child:root)
  {
   if(child.hasType("VOICE"))
   {
    const int v=(int)child.getProperty("index",-1);
    if(v<0||v>=voices)continue;
    gain[v]=(float)child.getProperty("gain",1.0);
    panorama[v]=(float)child.getProperty("pan",0.0);
    reverbSend[v]=(float)child.getProperty("rev",0.1);
    delaySend[v]=(float)child.getProperty("del",0.0);
    channelFilter[v]=(float)child.getProperty("filter",0.0);
    channelDrive[v]=(float)child.getProperty("drive",0.0);
    mute[v]=(bool)child.getProperty("mute",false);
    solo[v]=(bool)child.getProperty("solo",false);
    dsp[v].tune=(float)child.getProperty("tune",.5);
    dsp[v].decay=(float)child.getProperty("decay",.5);
    dsp[v].tone=(float)child.getProperty("tone",.5);
    dsp[v].character=(float)child.getProperty("char",.5);
    if(v>=8&&v<=10)
    {
     auto path=child.getProperty("samplePath").toString();
     if(path.isNotEmpty())
     {
      juce::File file(path);
      if(file.existsAsFile())loadSample(v-8,file);
     }
    }
   }
   else if(child.hasType("PATTERN"))
   {
    const int pidx=(int)child.getProperty("index",-1);
    if(pidx<0||pidx>=patterns)continue;
    pats[pidx]=Pattern{};
    pats[pidx].length=juce::jlimit(1,64,(int)child.getProperty("length",16));
    for(auto n:child)
    {
     if(!n.hasType("STEP"))continue;
     const int v=(int)n.getProperty("v",-1),s=(int)n.getProperty("s",-1);
     if(v<0||v>=voices||s<0||s>=64)continue;
     auto& st=pats[pidx].step[v][s];
     st.on=(bool)n.getProperty("on",false);
     st.velocity=(float)n.getProperty("vel",.85);
     st.probability=(float)n.getProperty("prob",1.0);
     st.ratchet=(int)n.getProperty("rat",1);
     st.micro=(float)n.getProperty("micro",0.0);
     st.accent=(bool)n.getProperty("accent",false);
     st.flam=(bool)n.getProperty("flam",false);
     st.note=(int)n.getProperty("note",60);
    }
   }
   else if(child.hasType("SONG"))
   {
    for(auto entry:child)
    {
     if(!entry.hasType("ENTRY")||songLength>=maxSongEntries)continue;
     const int index=songLength++;
     songChain[(size_t)index]={(int)entry.getProperty("pattern",0),(int)entry.getProperty("section",0)};
     audioSongPattern[(size_t)index].store(songChain[(size_t)index].pattern,std::memory_order_relaxed);
     activeSongLength.store(songLength,std::memory_order_relaxed);
    }
   }
  }
 }
 bool loadSample(int slot,const juce::File& file){if(slot<0||slot>=3)return false;juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));if(!r)return false;auto&ss=samples[(size_t)slot];ss.audio.setSize(1,(int)r->lengthInSamples);r->read(&ss.audio,0,(int)r->lengthInSamples,0,true,false);ss.sourceRate=r->sampleRate;ss.loaded=true;ss.active=false;ss.sourcePath=file.getFullPathName();sampleNames[(size_t)slot]=file.getFileName();return true;}
 juce::String getSampleName(int slot)const{return slot>=0&&slot<3?sampleNames[(size_t)slot]:juce::String();}
 void setGenerator(float complexityIn,float syncopationIn,float humanizeIn,float chaosIn,float kickStableIn,float snareStableIn,float hatActivityIn,float percActivityIn){complexity=complexityIn;syncopation=syncopationIn;humanize=humanizeIn;chaos=chaosIn;kickStability=kickStableIn;snareStability=snareStableIn;hatActivity=hatActivityIn;percActivity=percActivityIn;}
 void generate(float density,float variation){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++){float grid=(s%4==0)?1.0f:((s%2==0)?.55f:.22f);float role=.2f;if(v==0)role=juce::jmap(kickStability,.45f,.95f)*grid;else if(v==1)role=((s%8)==4?juce::jmap(snareStability,.55f,.98f):.05f);else if(v==3)role=juce::jmap(hatActivity,.25f,.9f)*((s%2)==0?1.f:.65f);else role=juce::jmap(percActivity,.08f,.55f);float offbeat=((s%4)!=0?syncopation*.35f:0.f);float complexBoost=complexity*((s%4)!=0?.28f:.08f);float randomTerm=(random.nextFloat()-.5f)*(variation+chaos*.65f);float chance=juce::jlimit(0.f,1.f,(role+offbeat+complexBoost)*density+randomTerm);auto&st=p.step[v][s];st.on=random.nextFloat()<chance;st.velocity=juce::jlimit(.2f,1.f,.72f+(random.nextFloat()-.5f)*(humanize*.55f+variation*.25f));st.probability=juce::jlimit(.35f,1.f,1.f-chaos*.25f+random.nextFloat()*chaos*.25f);st.micro=juce::jlimit(0.f,1.f,random.nextFloat()*humanize*.45f);st.accent=((s%4)==0)&&random.nextFloat()<complexity*.35f;}}
 void mutate(float amount){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++)if(random.nextFloat()<amount*.18f)p.step[v][s].on=!p.step[v][s].on;}
 void fill(){auto&p=pattern(current);for(int s=juce::jmax(0,p.length-4);s<p.length;s++){p.step[3][s].on=true;p.step[3][s].velocity=.65f+.1f*(s&1);p.step[4][s].on=(s&1)!=0;}}
 void process(juce::AudioBuffer<float>&out,std::array<juce::AudioBuffer<float>*,voices>* stems=nullptr){
  out.clear(); if(stems)for(auto*sb:*stems)if(sb)sb->clear(); const int n=out.getNumSamples(); const double base=sr*60.0/bpm/4.0;
  if(hostPlaying&&hasPpq){int target=((int)std::floor(hostPpq*4.0))%juce::jmax(1,pattern(current).length);if(target!=step){step=target-1;samplesToStep=0;}hasPpq=false;}
  for(int i=0;i<n;i++){for(int v=0;v<voices;v++){if(pendingMicro[v]>0&&--pendingMicro[v]==0){if(v==11)triggerSynthNote(pendingNote[v],pendingVelocity[v]);else trigger(v,pendingVelocity[v]);}if(pendingFlam[v]>0&&--pendingFlam[v]==0)trigger(v,.72f);if(pendingRatchet[v]>0&&ratchetCounter[v]<=0){trigger(v,.68f);pendingRatchet[v]--;ratchetCounter[v]=(int)juce::jmax(1.0,base/(pendingRatchet[v]+2));}if(ratchetCounter[v]>0)--ratchetCounter[v];}
   const bool sequencePlaying=hostPlaying||previewPlaying.load(std::memory_order_relaxed);
   if(sequencePlaying&&samplesToStep<=0){advance();double d=base*((step&1)?1.0+swing:1.0-swing);samplesToStep+=juce::jmax(1.0,d);}
   samplesToStep-=1.0; float l=0,r=0;
   bool anySolo=false;for(bool s:solo)if(s){anySolo=true;break;}float revSendL=0,revSendR=0,delSendL=0,delSendR=0;for(int v=0;v<voices;v++){float raw=(v>=8&&v<=10&&samples[(size_t)(v-8)].loaded)?samples[(size_t)(v-8)].process(sr)*sampleVelocity[(size_t)(v-8)]:dsp[v].process();channelLP[v]+=juce::jmap(channelFilter[v],.02f,.55f)*(raw-channelLP[v]);float filtered=raw*(1.0f-channelFilter[v])+channelLP[v]*channelFilter[v];float driven=std::tanh(filtered*(1.0f+channelDrive[v]*5.0f));float norm=std::tanh(1.0f+channelDrive[v]*5.0f);if(norm>1.0e-5f)driven/=norm;bool audible=!mute[v]&&(!anySolo||solo[v]);float x=(audible?driven:0.0f)*gain[v];float pan=panorama[v];float vl=x*std::sqrt(.5f*(1-pan)),vr=x*std::sqrt(.5f*(1+pan));l+=vl;r+=vr;
if(stems){auto*sb=(*stems)[(size_t)v];if(sb){if(sb->getNumChannels()>0)sb->setSample(0,i,vl);if(sb->getNumChannels()>1)sb->setSample(1,i,vr);}}
revSendL+=vl*reverbSend[v];revSendR+=vr*reverbSend[v];delSendL+=vl*delaySend[v];delSendR+=vr*delaySend[v];}
   float wetL=revSendL,wetR=revSendR;reverb.processStereo(&wetL,&wetR,1);
   if(!delayL.empty()){
    const size_t read=(delayPos+delayL.size()-(size_t)juce::jlimit(1.0,sr*1.9,sr*60.0/bpm*delayBeats))%delayL.size();
    const float dl=delayL[read],dr=delayR[read];
    delayL[delayPos]=delSendL+(delayPingPong?dr:dl)*delayFeedback;
    delayR[delayPos]=delSendR+(delayPingPong?dl:dr)*delayFeedback;
    delayPos=(delayPos+1)%delayL.size();
    if(delayEnabled){l+=dl*delayMix;r+=dr*delayMix;}
   }
   if(reverbEnabled){l+=wetL;r+=wetR;}
   l=std::tanh(l*.24f*drive);r=std::tanh(r*.24f*drive);
   const float mono=.5f*(l+r),low=masterLow.process(mono),high=mono-low;
   float eq=low*masterBass+high*masterTreble;
   if(compressorEnabled){
    const float detector=std::abs(eq);
    const float timeMs=detector>compressorEnvelope?compressorAttackMs:compressorReleaseMs;
    const float coefficient=std::exp(-1.0f/(juce::jmax(.1f,timeMs)*.001f*(float)sr));
    compressorEnvelope=detector+(compressorEnvelope-detector)*coefficient;
    const float levelDb=20.0f*std::log10(juce::jmax(1.0e-7f,compressorEnvelope));
    if(levelDb>compressorThresholdDb){const float compressedDb=compressorThresholdDb+(levelDb-compressorThresholdDb)/compressorRatio;eq*=std::pow(10.0f,(compressedDb-levelDb)/20.0f);}
   }
   const float comp=std::tanh(eq*masterComp)/(std::tanh(masterComp)+1.0e-6f);
   l=.72f*l+.28f*comp;r=.72f*r+.28f*comp;
   if(out.getNumChannels()>0)out.setSample(0,i,l);if(out.getNumChannels()>1)out.setSample(1,i,r);
  }
 }
 float drive=1.15f,masterBass=1.0f,masterTreble=1.0f,masterComp=1.35f,reverbSize=.45f,reverbDamping=.55f,reverbMix=.35f,delayBeats=.75f,delayFeedback=.36f,delayMix=.45f,compressorThresholdDb=-18.f,compressorRatio=3.f,compressorAttackMs=10.f,compressorReleaseMs=120.f; bool reverbEnabled=true,delayEnabled=true,delayPingPong=true,compressorEnabled=true; std::array<float,voices> gain{1,1,1,1,1,1,1,1,1,1,1,1},panorama{},reverbSend{.08f,.12f,.08f,.16f,.12f,.12f,.14f,.18f,.10f,.10f,.10f,.15f},delaySend{0,0,0,.05f,.08f,.08f,.10f,.12f,.08f,.08f,.08f,.12f},channelFilter{},channelDrive{}; std::array<bool,voices> mute{},solo{};
private:
 std::array<SongEntry,maxSongEntries>songChain{};std::array<std::atomic<int>,maxSongEntries>audioSongPattern{};std::atomic<int>activeSongLength{0},songPosition{0};int songLength=0;std::atomic<bool>songPlaying{false};
 Pattern savedPattern{}; std::atomic<bool>virtualDrummerEnabled{false}; int generatedBars=0,drummerStyle=0,hatDivision=2,phraseBars=4; float drummerEnergy=.55f,drummerHumanize=.2f,drummerSyncopation=.25f;
 void advance(){auto&p=pattern(current);step=(step+1)%juce::jmax(1,p.length);for(int v=0;v<voices;v++){auto&st=p.step[v][step];if(st.on&&random.nextFloat()<=st.probability){int microDelay=juce::jmax(0,(int)(st.micro*sr*.03f));if(microDelay>0){pendingMicro[v]=microDelay;pendingVelocity[v]=juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f));pendingNote[v]=st.note;}else if(v==11)triggerSynthNote(st.note,juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f)));else trigger(v,juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f)));if(st.flam)pendingFlam[v]=juce::jmax(1,(int)(sr*.018));if(st.ratchet>1)pendingRatchet[v]=st.ratchet-1;}}
  if(virtualDrummerEnabled&&step%16==15){++generatedBars;buildVirtualBar(generatedBars);}
  const int activeLength=activeSongLength.load(std::memory_order_relaxed);if(songPlaying&&activeLength>0&&step==p.length-1){const int next=getSongPosition()+1;pendingPattern=-1;if(next>=activeLength){songPlaying=false;if(!hostPlaying)previewPlaying.store(false,std::memory_order_relaxed);}else{songPosition.store(next,std::memory_order_relaxed);setPattern(audioSongPattern[(size_t)next].load(std::memory_order_relaxed));}}
  else if(pendingPattern>=0){bool change=false;if(changeMode==ChangeMode::EndPattern&&step==p.length-1)change=true;else if(changeMode==ChangeMode::NextBeat&&(step%4)==0)change=true;else if(changeMode==ChangeMode::NextBar&&(step%16)==0)change=true;if(change){int np=pendingPattern;pendingPattern=-1;setPattern(np);}}}
 void buildVirtualBar(int bar){auto&p=pattern(current);const int start=(bar*16)%juce::jmax(16,p.length);const int phrase=phraseBars>0?(bar/phraseBars)%4:0;const float energy=juce::jlimit(.15f,1.f,drummerEnergy+phrase*.12f);const float styleKick=drummerStyle==2?.78f:(drummerStyle==4?.9f:1.f);const float styleSnare=drummerStyle==1?.72f:(drummerStyle==2?.9f:1.f);const float styleHat=drummerStyle==4?.48f:(drummerStyle==2?.88f:1.f);
  for(int i=0;i<16;i++){int s=(start+i)%p.length;for(int v=0;v<voices;v++){auto&st=p.step[v][s];st.on=false;st.flam=false;st.ratchet=1;st.probability=1.f;st.micro=0.f;st.accent=false;float chance=0.f;
    if(v==0){if(i%4==0)chance=.96f*styleKick;else if((i==6||i==14)&&drummerStyle==3)chance=.35f*drummerSyncopation;else if((i==10)&&drummerStyle==2)chance=.26f*drummerSyncopation;}
    else if(v==1){if(i==4||i==12)chance=.92f*styleSnare;else if((i==3||i==11||i==14)&&drummerStyle!=4)chance=.08f+energy*.14f;}
    else if(v==3){if(i%hatDivision==0)chance=juce::jlimit(.1f,.98f,.48f+energy*.38f)*styleHat;}
    else if(v>=4&&v<=7){if((i%8==6||i%8==2)&&drummerStyle==2)chance=.18f+energy*.32f;else if(i%8==7&&drummerStyle==1)chance=.1f+energy*.2f;}
    if(phrase==3&&i>=12){if(v==3)chance=juce::jmax(chance,.82f);if(v==1&&i%2==0)chance=juce::jmax(chance,.58f);if(v==4&&i%2==1)chance=juce::jmax(chance,.42f);}
    st.on=random.nextFloat()<chance;const float accent=(i%4==0)?1.f:.78f;st.velocity=juce::jlimit(.25f,1.f,accent*(.72f+(random.nextFloat()-.5f)*drummerHumanize*.36f));st.micro=juce::jlimit(0.f,1.f,random.nextFloat()*drummerHumanize*.22f);st.accent=(i%4==0)&&v==0;
  }} }
 void initialiseFactoryPatterns(){for(int q=0;q<patterns;q++){auto&p=pats[q];p.length=16;for(int s=0;s<16;s++){p.step[0][s].on=s%4==0;p.step[1][s].on=s==4||s==12;p.step[3][s].on=s%2==0;p.step[4][s].on=(q%2)&&s%4==2;p.step[7][s].on=(q%3==2)&&(s==7||s==15);}}}
 double sr=44100,bpm=120,samplesToStep=0,hostPpq=0;float swing=0,complexity=.5f,syncopation=.25f,humanize=.15f,chaos=.1f,kickStability=.85f,snareStability=.9f,hatActivity=.65f,percActivity=.35f,compressorEnvelope=0;bool hostPlaying=false,hasPpq=false;std::atomic<bool>previewPlaying{false};std::array<int,voices>pendingFlam{},pendingRatchet{},ratchetCounter{},pendingMicro{};std::array<float,voices>pendingVelocity{};std::array<int,voices>pendingNote{};int pendingPattern=-1;ChangeMode changeMode=ChangeMode::EndPattern;juce::Reverb reverb;std::vector<float>delayL,delayR;size_t delayPos=0;struct OnePole{float z=0;float process(float x){z+=.08f*(x-z);return z;}}masterLow;int current=0,step=-1;juce::Random random;std::array<VoiceDSP,voices>dsp{};std::array<SampleSlot,3>samples{};std::array<float,3>sampleVelocity{1,1,1};std::array<juce::String,3>sampleNames{};std::array<Pattern,patterns>pats{};std::array<float,voices>channelLP{};
};

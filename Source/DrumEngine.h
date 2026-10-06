#pragma once
#include <atomic>
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <memory>

struct PatternStep { bool on=false; float velocity=.85f, probability=1.0f; int ratchet=1; float micro=0.0f; bool accent=false,flam=false; };
struct Pattern { std::array<std::array<PatternStep,64>,12> step{}; int length=16; };

struct SampleSlot { juce::AudioBuffer<float> audio; double sourceRate=44100.0,pos=0.0; bool loaded=false,active=false,reverse=false; juce::String sourcePath; float pitch=1.0f,level=1.0f; void trigger(){if(loaded){active=true;pos=reverse?(double)audio.getNumSamples()-1.0:0.0;}} float process(double outRate){if(!loaded||!active||audio.getNumSamples()<2)return 0;int i=juce::jlimit(0,audio.getNumSamples()-2,(int)pos);float frac=(float)(pos-i);float x=audio.getSample(0,i)*(1-frac)+audio.getSample(0,i+1)*frac;double inc=(sourceRate/outRate)*pitch*(reverse?-1.0:1.0);pos+=inc;if(pos<0||pos>=audio.getNumSamples()-1)active=false;return x*level;} };

struct VoiceDSP {
 double sr=44100.0, phase=0, phase2=0; float env=0, noiseEnv=0, velocity=0; int kind=0; float tune=0.5f,decay=0.5f,tone=0.5f,character=0.5f; juce::Random rng;
 void prepare(double s){sr=s;}
 void trigger(float v,int k){kind=k;velocity=v;env=v;noiseEnv=v;phase=phase2=0;}
 float process(){
  if(env<1.0e-5f && noiseEnv<1.0e-5f)return 0.0f;
  const float n=rng.nextFloat()*2.0f-1.0f; double f=80.0;
  float tuneMul=std::pow(2.0f,(tune-.5f)*2.0f);
  switch(kind){
   case 0:f=48.0+120.0*env;break; case 1:f=185.0;break; case 2:f=95.0;break; case 3:f=420.0;break;
   case 4:f=155.0;break; case 5:f=240.0;break; case 6:f=330.0;break; case 7:f=520.0;break;
   case 8:f=105.0;break; case 9:f=175.0;break; case 10:f=260.0;break; default:f=72.0;break;
  }
  f*=tuneMul; phase+=juce::MathConstants<double>::twoPi*f/sr; phase2+=juce::MathConstants<double>::twoPi*f*(kind>=4&&kind<=7?(1.05+character*1.2):1.4+character*1.2)/sr;
  if(phase>juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
  if(phase2>juce::MathConstants<double>::twoPi)phase2-=juce::MathConstants<double>::twoPi;
  float a=(float)std::sin(phase),b=(float)std::sin(phase2),x=0;
  if(kind==0)x=a*0.98f;
  else if(kind==1)x=a*(.15f+tone*.35f)+n*noiseEnv*(.95f-tone*.55f);
  else if(kind==2)x=(a>0?1.f:-1.f)*.65f+a*.35f;
  else if(kind==3)x=n*(.35f+tone*.5f)+(a*b)*(.25f+character*.5f);
  else if(kind==4)x=std::sin((float)phase+b*(1.0f+character*7.0f));
  else if(kind==5)x=std::tanh((a+b*(.2f+tone))* (1.2f+character*3.0f));
  else if(kind==6)x=(a*.45f+b*.35f+n*.20f);
  else if(kind==7)x=n*.55f+std::sin((float)phase+n*.9f)*.45f;
  else if(kind>=8&&kind<=10)x=(a*.55f+n*.45f); // sample slots have distinct fallback synthesis until a WAV is loaded
  else x=a*.7f+b*.3f;
  float d=juce::jmap(decay,0.0f,1.0f,0.965f,0.99975f); env*= kind==3?juce::jmin(d,.992f):d; noiseEnv*=juce::jmap(decay,0.0f,1.0f,.94f,.995f);
  return std::tanh(x*velocity*1.65f);
 }
};

class DrumEngine {
public:
 enum class ChangeMode{Immediate=0,NextBeat,NextBar,EndPattern};
 static constexpr int voices=12,patterns=32;
 void prepare(double s){sr=s;for(auto&v:dsp)v.prepare(s);reverb.setSampleRate(s);juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=1.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);delayL.assign((size_t)(s*2.0),0.f);delayR.assign((size_t)(s*2.0),0.f);initialiseFactoryPatterns();}
 void trigger(int i,float v){if(i>=0&&i<voices){if(i>=8&&i<=10&&samples[(size_t)(i-8)].loaded){sampleVelocity[(size_t)(i-8)]=v;samples[(size_t)(i-8)].trigger();}else dsp[(size_t)i].trigger(v,i);}}
 void setPattern(int p){current=juce::jlimit(0,patterns-1,p);step=-1;samplesToStep=0;hasPpq=false;}
 void setPreviewPlaying(bool shouldPlay){previewPlaying.store(shouldPlay,std::memory_order_relaxed);}
 void requestPattern(int p){p=juce::jlimit(0,patterns-1,p);if(changeMode==ChangeMode::Immediate)setPattern(p);else pendingPattern=p;}
 void setChangeMode(ChangeMode m){changeMode=m;}
 ChangeMode getChangeMode()const{return changeMode;}
 int getPattern()const{return current;} int getCurrentStep()const{return step;}
 Pattern& pattern(int p){return pats[(size_t)juce::jlimit(0,patterns-1,p)];}
 void setHost(double b,bool play){bpm=b>20?b:120;hostPlaying=play;} void setHostPpq(double ppq){hostPpq=ppq;hasPpq=true;}
 void setSwing(float s){swing=juce::jlimit(0.0f,.75f,s);}
 void setReverb(float size,float damping){reverbSize=juce::jlimit(0.f,1.f,size);reverbDamping=juce::jlimit(0.f,1.f,damping);juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=1.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);}
 void setDelay(float beats,float feedback,float mix){delayBeats=juce::jlimit(.125f,2.0f,beats);delayFeedback=juce::jlimit(0.f,.88f,feedback);delayMix=juce::jlimit(0.f,1.f,mix);}
 void setMaster(float bass,float treble,float comp){masterBass=juce::jlimit(.4f,1.8f,bass);masterTreble=juce::jlimit(.4f,1.8f,treble);masterComp=juce::jlimit(.6f,3.0f,comp);}
 void setVoiceParam(int i,int param,float value){if(i<0||i>=voices)return;value=juce::jlimit(0.0f,1.0f,value);auto&v=dsp[(size_t)i];if(param==0)v.tune=value;else if(param==1)v.decay=value;else if(param==2)v.tone=value;else if(param==3)v.character=value;if(i>=8&&i<=10){auto&s=samples[(size_t)(i-8)];if(param==0)s.pitch=juce::jmap(value,0.0f,1.0f,.5f,2.0f);else if(param==3)s.reverse=value>.75f;}}
 float getVoiceParam(int i,int param)const{if(i<0||i>=voices)return .5f;auto const&v=dsp[(size_t)i];return param==0?v.tune:param==1?v.decay:param==2?v.tone:v.character;}
 juce::ValueTree saveState()const{juce::ValueTree root("ENGINE");root.setProperty("pattern",current,nullptr);root.setProperty("changeMode",(int)changeMode,nullptr);root.setProperty("masterBass",masterBass,nullptr);root.setProperty("masterTreble",masterTreble,nullptr);root.setProperty("masterComp",masterComp,nullptr);root.setProperty("reverbSize",reverbSize,nullptr);root.setProperty("reverbDamping",reverbDamping,nullptr);root.setProperty("delayBeats",delayBeats,nullptr);root.setProperty("delayFeedback",delayFeedback,nullptr);root.setProperty("delayMix",delayMix,nullptr);for(int v=0;v<voices;v++){juce::ValueTree voice("VOICE");voice.setProperty("index",v,nullptr);if(v>=8&&v<=10)voice.setProperty("samplePath",samples[(size_t)(v-8)].sourcePath,nullptr);voice.setProperty("gain",gain[v],nullptr);voice.setProperty("pan",panorama[v],nullptr);voice.setProperty("rev",reverbSend[v],nullptr);voice.setProperty("del",delaySend[v],nullptr);voice.setProperty("filter",channelFilter[v],nullptr);voice.setProperty("drive",channelDrive[v],nullptr);voice.setProperty("mute",mute[v],nullptr);voice.setProperty("solo",solo[v],nullptr);voice.setProperty("tune",dsp[v].tune,nullptr);voice.setProperty("decay",dsp[v].decay,nullptr);voice.setProperty("tone",dsp[v].tone,nullptr);voice.setProperty("char",dsp[v].character,nullptr);root.addChild(voice,-1,nullptr);}for(int pidx=0;pidx<patterns;pidx++){juce::ValueTree pat("PATTERN");pat.setProperty("index",pidx,nullptr);pat.setProperty("length",pats[pidx].length,nullptr);for(int v=0;v<voices;v++)for(int s=0;s<64;s++){auto const&st=pats[pidx].step[v][s];if(st.on||st.velocity!=.85f||st.probability!=1.f||st.ratchet!=1||st.micro!=0.f||st.accent||st.flam){juce::ValueTree n("STEP");n.setProperty("v",v,nullptr);n.setProperty("s",s,nullptr);n.setProperty("on",st.on,nullptr);n.setProperty("vel",st.velocity,nullptr);n.setProperty("prob",st.probability,nullptr);n.setProperty("rat",st.ratchet,nullptr);n.setProperty("micro",st.micro,nullptr);n.setProperty("accent",st.accent,nullptr);n.setProperty("flam",st.flam,nullptr);pat.addChild(n,-1,nullptr);}}root.addChild(pat,-1,nullptr);}return root;}
 void loadState(const juce::ValueTree&root){if(!root.isValid())return;current=(int)root.getProperty("pattern",0);changeMode=(ChangeMode)(int)root.getProperty("changeMode",(int)ChangeMode::EndPattern);masterBass=(float)root.getProperty("masterBass",1.0);masterTreble=(float)root.getProperty("masterTreble",1.0);masterComp=(float)root.getProperty("masterComp",1.35);reverbSize=(float)root.getProperty("reverbSize",.45);reverbDamping=(float)root.getProperty("reverbDamping",.55);delayBeats=(float)root.getProperty("delayBeats",.75);delayFeedback=(float)root.getProperty("delayFeedback",.36);delayMix=(float)root.getProperty("delayMix",.45);setReverb(reverbSize,reverbDamping);for(auto child:root){if(child.hasType("VOICE")){int v=(int)child.getProperty("index",-1);if(v>=0&&v<voices){gain[v]=(float)child.getProperty("gain",1.0);panorama[v]=(float)child.getProperty("pan",0.0);reverbSend[v]=(float)child.getProperty("rev",0.1);delaySend[v]=(float)child.getProperty("del",0.0);channelFilter[v]=(float)child.getProperty("filter",0.0);channelDrive[v]=(float)child.getProperty("drive",0.0);mute[v]=(bool)child.getProperty("mute",false);solo[v]=(bool)child.getProperty("solo",false);dsp[v].tune=(float)child.getProperty("tune",.5);dsp[v].decay=(float)child.getProperty("decay",.5);dsp[v].tone=(float)child.getProperty("tone",.5);dsp[v].character=(float)child.getProperty("char",.5);if(v>=8&&v<=10){auto path=child.getProperty("samplePath").toString();if(path.isNotEmpty()){juce::File file(path);if(file.existsAsFile())loadSample(v-8,file);}}}}else if(child.hasType("PATTERN")){int pidx=(int)child.getProperty("index",-1);if(pidx>=0&&pidx<patterns){pats[pidx]=Pattern{};pats[pidx].length=juce::jlimit(1,64,(int)child.getProperty("length",16));for(auto n:child){if(!n.hasType("STEP"))continue;int v=(int)n.getProperty("v",-1),s=(int)n.getProperty("s",-1);if(v<0||v>=voices||s<0||s>=64)continue;auto&st=pats[pidx].step[v][s];st.on=(bool)n.getProperty("on",false);st.velocity=(float)n.getProperty("vel",.85);st.probability=(float)n.getProperty("prob",1.0);st.ratchet=(int)n.getProperty("rat",1);st.micro=(float)n.getProperty("micro",0.0);st.accent=(bool)n.getProperty("accent",false);st.flam=(bool)n.getProperty("flam",false);}}}}}
 bool loadSample(int slot,const juce::File& file){if(slot<0||slot>=3)return false;juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));if(!r)return false;auto&ss=samples[(size_t)slot];ss.audio.setSize(1,(int)r->lengthInSamples);r->read(&ss.audio,0,(int)r->lengthInSamples,0,true,false);ss.sourceRate=r->sampleRate;ss.loaded=true;ss.active=false;ss.sourcePath=file.getFullPathName();sampleNames[(size_t)slot]=file.getFileName();return true;}
 juce::String getSampleName(int slot)const{return slot>=0&&slot<3?sampleNames[(size_t)slot]:juce::String();}
 void setGenerator(float complexityIn,float syncopationIn,float humanizeIn,float chaosIn,float kickStableIn,float snareStableIn,float hatActivityIn,float percActivityIn){complexity=complexityIn;syncopation=syncopationIn;humanize=humanizeIn;chaos=chaosIn;kickStability=kickStableIn;snareStability=snareStableIn;hatActivity=hatActivityIn;percActivity=percActivityIn;}
 void generate(float density,float variation){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++){float grid=(s%4==0)?1.0f:((s%2==0)?.55f:.22f);float role=.2f;if(v==0)role=juce::jmap(kickStability,.45f,.95f)*grid;else if(v==1)role=((s%8)==4?juce::jmap(snareStability,.55f,.98f):.05f);else if(v==3)role=juce::jmap(hatActivity,.25f,.9f)*((s%2)==0?1.f:.65f);else role=juce::jmap(percActivity,.08f,.55f);float offbeat=((s%4)!=0?syncopation*.35f:0.f);float complexBoost=complexity*((s%4)!=0?.28f:.08f);float randomTerm=(random.nextFloat()-.5f)*(variation+chaos*.65f);float chance=juce::jlimit(0.f,1.f,(role+offbeat+complexBoost)*density+randomTerm);auto&st=p.step[v][s];st.on=random.nextFloat()<chance;st.velocity=juce::jlimit(.2f,1.f,.72f+(random.nextFloat()-.5f)*(humanize*.55f+variation*.25f));st.probability=juce::jlimit(.35f,1.f,1.f-chaos*.25f+random.nextFloat()*chaos*.25f);st.micro=juce::jlimit(0.f,1.f,random.nextFloat()*humanize*.45f);st.accent=((s%4)==0)&&random.nextFloat()<complexity*.35f;}}
 void mutate(float amount){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++)if(random.nextFloat()<amount*.18f)p.step[v][s].on=!p.step[v][s].on;}
 void fill(){auto&p=pattern(current);for(int s=juce::jmax(0,p.length-4);s<p.length;s++){p.step[3][s].on=true;p.step[3][s].velocity=.65f+.1f*(s&1);p.step[4][s].on=(s&1)!=0;}}
 void process(juce::AudioBuffer<float>&out,std::array<juce::AudioBuffer<float>*,voices>* stems=nullptr){
  out.clear(); if(stems)for(auto*sb:*stems)if(sb)sb->clear(); const int n=out.getNumSamples(); const double base=sr*60.0/bpm/4.0;
  if(hostPlaying&&hasPpq){int target=((int)std::floor(hostPpq*4.0))%juce::jmax(1,pattern(current).length);if(target!=step){step=target-1;samplesToStep=0;}hasPpq=false;}
  for(int i=0;i<n;i++){for(int v=0;v<voices;v++){if(pendingMicro[v]>0&&--pendingMicro[v]==0)trigger(v,pendingVelocity[v]);if(pendingFlam[v]>0&&--pendingFlam[v]==0)trigger(v,.72f);if(pendingRatchet[v]>0&&ratchetCounter[v]<=0){trigger(v,.68f);pendingRatchet[v]--;ratchetCounter[v]=(int)juce::jmax(1.0,base/(pendingRatchet[v]+2));}if(ratchetCounter[v]>0)--ratchetCounter[v];}
   const bool sequencePlaying=hostPlaying||previewPlaying.load(std::memory_order_relaxed);
   if(sequencePlaying&&samplesToStep<=0){advance();double d=base*((step&1)?1.0+swing:1.0-swing);samplesToStep+=juce::jmax(1.0,d);}
   samplesToStep-=1.0; float l=0,r=0;
   bool anySolo=false;for(bool s:solo)if(s){anySolo=true;break;}float revSendL=0,revSendR=0,delSendL=0,delSendR=0;for(int v=0;v<voices;v++){float raw=(v>=8&&v<=10&&samples[(size_t)(v-8)].loaded)?samples[(size_t)(v-8)].process(sr)*sampleVelocity[(size_t)(v-8)]:dsp[v].process();channelLP[v]+=juce::jmap(channelFilter[v],.02f,.55f)*(raw-channelLP[v]);float filtered=raw*(1.0f-channelFilter[v])+channelLP[v]*channelFilter[v];float driven=std::tanh(filtered*(1.0f+channelDrive[v]*5.0f));float norm=std::tanh(1.0f+channelDrive[v]*5.0f);if(norm>1.0e-5f)driven/=norm;bool audible=!mute[v]&&(!anySolo||solo[v]);float x=(audible?driven:0.0f)*gain[v];float pan=panorama[v];float vl=x*std::sqrt(.5f*(1-pan)),vr=x*std::sqrt(.5f*(1+pan));l+=vl;r+=vr;
if(stems){auto*sb=(*stems)[(size_t)v];if(sb){if(sb->getNumChannels()>0)sb->setSample(0,i,vl);if(sb->getNumChannels()>1)sb->setSample(1,i,vr);}}
revSendL+=vl*reverbSend[v];revSendR+=vr*reverbSend[v];delSendL+=vl*delaySend[v];delSendR+=vr*delaySend[v];}
   float wetL=revSendL,wetR=revSendR;reverb.processStereo(&wetL,&wetR,1);if(!delayL.empty()){size_t read=(delayPos+delayL.size()-(size_t)juce::jlimit(1.0,sr*1.9,sr*60.0/bpm*delayBeats))%delayL.size();float dl=delayL[read],dr=delayR[read];delayL[delayPos]=delSendL+dr*delayFeedback;delayR[delayPos]=delSendR+dl*delayFeedback;delayPos=(delayPos+1)%delayL.size();l+=dl*delayMix;r+=dr*delayMix;}l+=wetL;r+=wetR;l=std::tanh(l*.24f*drive);r=std::tanh(r*.24f*drive);float mono=.5f*(l+r);float low=masterLow.process(mono);float high=mono-low;float eq=low*masterBass+high*masterTreble;float comp=std::tanh(eq*masterComp)/(std::tanh(masterComp)+1.0e-6f);l=.72f*l+.28f*comp;r=.72f*r+.28f*comp;
   if(out.getNumChannels()>0)out.setSample(0,i,l);if(out.getNumChannels()>1)out.setSample(1,i,r);
  }
 }
 float drive=1.15f,masterBass=1.0f,masterTreble=1.0f,masterComp=1.35f,reverbSize=.45f,reverbDamping=.55f,delayBeats=.75f,delayFeedback=.36f,delayMix=.45f; std::array<float,voices> gain{1,1,1,1,1,1,1,1,1,1,1,1},panorama{},reverbSend{.08f,.12f,.08f,.16f,.12f,.12f,.14f,.18f,.10f,.10f,.10f,.15f},delaySend{0,0,0,.05f,.08f,.08f,.10f,.12f,.08f,.08f,.08f,.12f},channelFilter{},channelDrive{}; std::array<bool,voices> mute{},solo{};
private:
 void advance(){auto&p=pattern(current);step=(step+1)%juce::jmax(1,p.length);for(int v=0;v<voices;v++){auto&st=p.step[v][step];if(st.on&&random.nextFloat()<=st.probability){int microDelay=juce::jmax(0,(int)(st.micro*sr*.03f));if(microDelay>0){pendingMicro[v]=microDelay;pendingVelocity[v]=juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f));}else trigger(v,juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f)));if(st.flam)pendingFlam[v]=juce::jmax(1,(int)(sr*.018));if(st.ratchet>1)pendingRatchet[v]=st.ratchet-1;}}
  if(pendingPattern>=0){bool change=false;if(changeMode==ChangeMode::EndPattern&&step==p.length-1)change=true;else if(changeMode==ChangeMode::NextBeat&&(step%4)==0)change=true;else if(changeMode==ChangeMode::NextBar&&(step%16)==0)change=true;if(change){int np=pendingPattern;pendingPattern=-1;setPattern(np);}}}
 void initialiseFactoryPatterns(){for(int q=0;q<patterns;q++){auto&p=pats[q];p.length=16;for(int s=0;s<16;s++){p.step[0][s].on=s%4==0;p.step[1][s].on=s==4||s==12;p.step[3][s].on=s%2==0;p.step[4][s].on=(q%2)&&s%4==2;p.step[7][s].on=(q%3==2)&&(s==7||s==15);}}}
 double sr=44100,bpm=120,samplesToStep=0,hostPpq=0;float swing=0,complexity=.5f,syncopation=.25f,humanize=.15f,chaos=.1f,kickStability=.85f,snareStability=.9f,hatActivity=.65f,percActivity=.35f;bool hostPlaying=false,hasPpq=false;std::atomic<bool>previewPlaying{false};std::array<int,voices>pendingFlam{},pendingRatchet{},ratchetCounter{},pendingMicro{};std::array<float,voices>pendingVelocity{};int pendingPattern=-1;ChangeMode changeMode=ChangeMode::EndPattern;juce::Reverb reverb;std::vector<float>delayL,delayR;size_t delayPos=0;struct OnePole{float z=0;float process(float x){z+=.08f*(x-z);return z;}}masterLow;int current=0,step=-1;juce::Random random;std::array<VoiceDSP,voices>dsp{};std::array<SampleSlot,3>samples{};std::array<float,3>sampleVelocity{1,1,1};std::array<juce::String,3>sampleNames{};std::array<Pattern,patterns>pats{};std::array<float,voices>channelLP{};
};

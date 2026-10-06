#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <memory>

struct PatternStep { bool on=false; float velocity=.85f, probability=1.0f; int ratchet=1; float micro=0.0f; bool accent=false; };
struct Pattern { std::array<std::array<PatternStep,64>,12> step{}; int length=16; };

struct SampleSlot { juce::AudioBuffer<float> audio; double sourceRate=44100.0,pos=0.0; bool loaded=false,active=false,reverse=false; float pitch=1.0f,level=1.0f; void trigger(){if(loaded){active=true;pos=reverse?(double)audio.getNumSamples()-1.0:0.0;}} float process(double outRate){if(!loaded||!active||audio.getNumSamples()<2)return 0;int i=juce::jlimit(0,audio.getNumSamples()-2,(int)pos);float frac=(float)(pos-i);float x=audio.getSample(0,i)*(1-frac)+audio.getSample(0,i+1)*frac;double inc=(sourceRate/outRate)*pitch*(reverse?-1.0:1.0);pos+=inc;if(pos<0||pos>=audio.getNumSamples()-1)active=false;return x*level;} };

struct VoiceDSP {
 double sr=44100.0, phase=0, phase2=0; float env=0, noiseEnv=0, velocity=0; int kind=0; juce::Random rng;
 void prepare(double s){sr=s;}
 void trigger(float v,int k){kind=k;velocity=v;env=v;noiseEnv=v;phase=phase2=0;}
 float process(){
  if(env<1.0e-5f && noiseEnv<1.0e-5f)return 0.0f;
  const float n=rng.nextFloat()*2.0f-1.0f; double f=80.0;
  switch(kind){
   case 0:f=48.0+120.0*env;break; case 1:f=185.0;break; case 2:f=95.0;break; case 3:f=420.0;break;
   case 4:f=155.0;break; case 5:f=240.0;break; case 6:f=330.0;break; case 7:f=520.0;break;
   case 8:f=105.0;break; case 9:f=175.0;break; case 10:f=260.0;break; default:f=72.0;break;
  }
  phase+=juce::MathConstants<double>::twoPi*f/sr; phase2+=juce::MathConstants<double>::twoPi*f*(kind>=4&&kind<=7?1.414:2.01)/sr;
  if(phase>juce::MathConstants<double>::twoPi)phase-=juce::MathConstants<double>::twoPi;
  if(phase2>juce::MathConstants<double>::twoPi)phase2-=juce::MathConstants<double>::twoPi;
  float a=(float)std::sin(phase),b=(float)std::sin(phase2),x=0;
  if(kind==0)x=a*0.98f;
  else if(kind==1)x=a*.32f+n*noiseEnv*.78f;
  else if(kind==2)x=(a>0?1.f:-1.f)*.65f+a*.35f;
  else if(kind==3)x=n*.65f+(a*b)*.45f;
  else if(kind==4)x=std::sin((float)phase+b*3.5f);
  else if(kind==5)x=std::tanh((a+b*.8f)*2.2f);
  else if(kind==6)x=(a*.45f+b*.35f+n*.20f);
  else if(kind==7)x=n*.55f+std::sin((float)phase+n*.9f)*.45f;
  else if(kind>=8&&kind<=10)x=(a*.55f+n*.45f); // sample slots have distinct fallback synthesis until a WAV is loaded
  else x=a*.7f+b*.3f;
  env*= kind==0?.9988f:(kind==3?.985f:(kind==11?.9992f:.9945f)); noiseEnv*=.982f;
  return std::tanh(x*velocity*1.65f);
 }
};

class DrumEngine {
public:
 static constexpr int voices=12,patterns=32;
 void prepare(double s){sr=s;for(auto&v:dsp)v.prepare(s);reverb.setSampleRate(s);juce::dsp::Reverb::Parameters rp;rp.roomSize=.45f;rp.damping=.55f;rp.wetLevel=.22f;rp.dryLevel=.78f;reverb.setParameters(rp);delayL.assign((size_t)(s*2.0),0.f);delayR.assign((size_t)(s*2.0),0.f);initialiseFactoryPatterns();}
 void trigger(int i,float v){if(i>=0&&i<voices){if(i>=8&&i<=10&&samples[(size_t)(i-8)].loaded){sampleVelocity[(size_t)(i-8)]=v;samples[(size_t)(i-8)].trigger();}else dsp[(size_t)i].trigger(v,i);}}
 void setPattern(int p){current=juce::jlimit(0,patterns-1,p);step=-1;samplesToStep=0;}
 int getPattern()const{return current;} int getCurrentStep()const{return step;}
 Pattern& pattern(int p){return pats[(size_t)juce::jlimit(0,patterns-1,p)];}
 void setHost(double b,bool play){bpm=b>20?b:120;hostPlaying=play;}
 void setSwing(float s){swing=juce::jlimit(0.0f,.75f,s);}
 bool loadSample(int slot,const juce::File& file){if(slot<0||slot>=3)return false;juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));if(!r)return false;auto&ss=samples[(size_t)slot];ss.audio.setSize(1,(int)r->lengthInSamples);r->read(&ss.audio,0,(int)r->lengthInSamples,0,true,false);ss.sourceRate=r->sampleRate;ss.loaded=true;ss.active=false;sampleNames[(size_t)slot]=file.getFileName();return true;}
 juce::String getSampleName(int slot)const{return slot>=0&&slot<3?sampleNames[(size_t)slot]:juce::String();}
 void generate(float density,float variation){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++){float role=v==0?(s%4==0?.85f:.15f):v==1?((s==4||s==12)?.9f:.08f):v==3?.55f:.22f;float chance=juce::jlimit(0.f,1.f,role*density+(random.nextFloat()-.5f)*variation);p.step[v][s].on=random.nextFloat()<chance;p.step[v][s].velocity=.55f+random.nextFloat()*.45f;p.step[v][s].probability=.72f+random.nextFloat()*.28f;}}
 void mutate(float amount){auto&p=pattern(current);for(int v=0;v<voices;v++)for(int s=0;s<p.length;s++)if(random.nextFloat()<amount*.18f)p.step[v][s].on=!p.step[v][s].on;}
 void fill(){auto&p=pattern(current);for(int s=juce::jmax(0,p.length-4);s<p.length;s++){p.step[3][s].on=true;p.step[3][s].velocity=.65f+.1f*(s&1);p.step[4][s].on=(s&1)!=0;}}
 void process(juce::AudioBuffer<float>&out){
  out.clear(); const int n=out.getNumSamples(); const double base=sr*60.0/bpm/4.0;
  for(int i=0;i<n;i++){
   if(hostPlaying&&samplesToStep<=0){advance();double d=base*((step&1)?1.0+swing:1.0-swing);samplesToStep+=juce::jmax(1.0,d);}
   samplesToStep-=1.0; float l=0,r=0;
   float revSendL=0,revSendR=0,delSendL=0,delSendR=0;for(int v=0;v<voices;v++){float raw=(v>=8&&v<=10&&samples[(size_t)(v-8)].loaded)?samples[(size_t)(v-8)].process(sr)*sampleVelocity[(size_t)(v-8)]:dsp[v].process();float x=raw*gain[v];float pan=panorama[v];float vl=x*std::sqrt(.5f*(1-pan)),vr=x*std::sqrt(.5f*(1+pan));l+=vl;r+=vr;revSendL+=vl*reverbSend[v];revSendR+=vr*reverbSend[v];delSendL+=vl*delaySend[v];delSendR+=vr*delaySend[v];}
   float wetL=revSendL,wetR=revSendR;reverb.processStereo(&wetL,&wetR,1);if(!delayL.empty()){size_t read=(delayPos+delayL.size()-(size_t)juce::jlimit(1.0,sr*1.9,sr*60.0/bpm*.75))%delayL.size();float dl=delayL[read],dr=delayR[read];delayL[delayPos]=delSendL+dr*.36f;delayR[delayPos]=delSendR+dl*.36f;delayPos=(delayPos+1)%delayL.size();l+=dl*.45f;r+=dr*.45f;}l+=wetL;r+=wetR;l=std::tanh(l*.24f*drive);r=std::tanh(r*.24f*drive);float mono=.5f*(l+r);float low=masterLow.process(mono);float high=mono-low;float eq=low*masterBass+high*masterTreble;float comp=std::tanh(eq*masterComp)/(std::tanh(masterComp)+1.0e-6f);l=.72f*l+.28f*comp;r=.72f*r+.28f*comp;
   if(out.getNumChannels()>0)out.setSample(0,i,l);if(out.getNumChannels()>1)out.setSample(1,i,r);
  }
 }
 float drive=1.15f,masterBass=1.0f,masterTreble=1.0f,masterComp=1.35f; std::array<float,voices> gain{1,1,1,1,1,1,1,1,1,1,1,1},panorama{},reverbSend{.08f,.12f,.08f,.16f,.12f,.12f,.14f,.18f,.10f,.10f,.10f,.15f},delaySend{0,0,0,.05f,.08f,.08f,.10f,.12f,.08f,.08f,.08f,.12f};
private:
 void advance(){auto&p=pattern(current);step=(step+1)%juce::jmax(1,p.length);for(int v=0;v<voices;v++){auto&st=p.step[v][step];if(st.on&&random.nextFloat()<=st.probability){int reps=juce::jlimit(1,4,st.ratchet);trigger(v,juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f)));(void)reps;}}}
 void initialiseFactoryPatterns(){for(int q=0;q<patterns;q++){auto&p=pats[q];p.length=16;for(int s=0;s<16;s++){p.step[0][s].on=s%4==0;p.step[1][s].on=s==4||s==12;p.step[3][s].on=s%2==0;p.step[4][s].on=(q%2)&&s%4==2;p.step[7][s].on=(q%3==2)&&(s==7||s==15);}}}
 double sr=44100,bpm=120,samplesToStep=0;float swing=0;bool hostPlaying=false;juce::Reverb reverb;std::vector<float>delayL,delayR;size_t delayPos=0;struct OnePole{float z=0;float process(float x){z+=.08f*(x-z);return z;}}masterLow;int current=0,step=-1;juce::Random random;std::array<VoiceDSP,voices>dsp{};std::array<SampleSlot,3>samples{};std::array<float,3>sampleVelocity{1,1,1};std::array<juce::String,3>sampleNames{};std::array<Pattern,patterns>pats{};
};
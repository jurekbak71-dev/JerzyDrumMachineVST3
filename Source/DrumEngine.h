#pragma once
#include <atomic>
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

struct PatternStep { bool on=false; float velocity=.85f, probability=1.0f; int ratchet=1,note=60; float micro=0.0f; bool accent=false,flam=false; };
struct Pattern { std::array<std::array<PatternStep,64>,12> step{}; std::array<int,12> trackLength{16,16,16,16,16,16,16,16,16,16,16,16}; int length=16; };
struct SongEntry { int pattern=0,section=0,bars=4; };

struct SampleSlot { juce::AudioBuffer<float> audio; double sourceRate=44100.0,pos=0.0; bool loaded=false,active=false,reverse=false; juce::String sourcePath; float pitch=1.0f,level=1.0f; void trigger(){if(loaded){active=true;pos=reverse?(double)audio.getNumSamples()-1.0:0.0;}} float process(double outRate){if(!loaded||!active||audio.getNumSamples()<2)return 0;int i=juce::jlimit(0,audio.getNumSamples()-2,(int)pos);float frac=(float)(pos-i);float x=audio.getSample(0,i)*(1-frac)+audio.getSample(0,i+1)*frac;double inc=(sourceRate/outRate)*pitch*(reverse?-1.0:1.0);pos+=inc;if(pos<0||pos>=audio.getNumSamples()-1)active=false;return x*level;} };

struct VoiceDSP {
 enum class Stage : uint8_t { off, attack, decay, sustain, release };
 double sr=44100.0, phase=0, phase2=0, phase3=0;
 float env=0, noiseEnv=0, velocity=0, tune=.5f, decay=.5f, tone=.5f, character=.5f;
 float drumDecayCoeff=.999f, snareNoiseCoeff=.1f, snareWireCoeff=.999f;
 int kind=0,midiNote=60;
 // Dedicated mono synth section. Drum tuning never touches MIDI note pitch.
 float attackMs=8, synthDecayMs=180, sustain=.72f, releaseMs=140, cutoff=12000, resonance=.12f;
 float osc2Mix=.35f, detuneCents=7, filterG=0, filterK=1.7f, filterA1=1,filterA2=0,filterA3=0,ic1eq=0,ic2eq=0;
 double synthBaseHz=261.625565,osc2Ratio=1.004;
 int wave1=0,wave2=1,octave2=0; bool osc2Enabled=true,filterEnabled=true;
 Stage stage=Stage::off; float synthEnv=0, attackStep=0, decayCoeff=0, releaseCoeff=0;
 uint32_t noiseState=0x6d2b79f5u; float noiseLow=0, noiseBand=0, metalPhase=0,resonator1=0,resonator2=0;

 static const std::array<float,2049>& sineTable(){
  static const auto table=[] { std::array<float,2049> t{}; for(size_t i=0;i<2048;i++)t[i]=std::sin(juce::MathConstants<float>::twoPi*(float)i/2048.0f); t[2048]=t[0]; return t; }();
  return table;
 }
 static float sine(double p){const auto& t=sineTable();p-=std::floor(p);const double ix=p*2048.0;const auto i=(size_t)ix;const float f=(float)(ix-i);return t[i]+(t[i+1]-t[i])*f;}
 static float polyBlep(double p,double dt){if(dt<=0||dt>=.5)return 0; if(p<dt){const float x=(float)(p/dt);return x+x-x*x-1.0f;} if(p>1.0-dt){const float x=(float)((p-1.0)/dt);return x*x+x+x+1.0f;}return 0;}
 static float oscillator(int shape,double p,double dt){
  if(shape==1)return 2.0f*(float)p-1.0f-polyBlep(p,dt);
  if(shape==2){const float x=p<.5?1.0f:-1.0f;return x+polyBlep(p,dt)-polyBlep(p<.5?p+.5:p-.5,dt);}
  if(shape==3)return 1.0f-4.0f*(float)std::abs(p-.5);
  return sine(p);
 }
 float noise(){noiseState^=noiseState<<13;noiseState^=noiseState>>17;noiseState^=noiseState<<5;return (float)(noiseState>>8)*(1.0f/8388607.5f)-1.0f;}
 void prepare(double s){sr=s;rebuildSynthCoefficients();rebuildDrumCoefficients();}
 void rebuildDrumCoefficients(){
  const float minSeconds[]={.045f,.035f,.06f,.025f,.025f,.025f,.04f,.025f};
  const float maxSeconds[]={1.65f,.82f,1.9f,3.8f,1.2f,1.3f,2.6f,1.5f};
  const int k=juce::jlimit(0,7,kind);
  const float seconds=juce::jmap(decay,0.0f,1.0f,minSeconds[k],maxSeconds[k]);
  drumDecayCoeff=std::exp(-1.0f/(seconds*(float)sr));
  snareNoiseCoeff=juce::jmap(tone,.012f,.24f);
  const float wireSeconds=juce::jmap(character,.08f,1.0f,.045f,.24f);
  snareWireCoeff=std::exp(-1.0f/(wireSeconds*(float)sr));
 }
 void rebuildSynthCoefficients(){
  const float safeSr=(float)juce::jmax(8000.0,sr);
  attackStep=1.0f/juce::jmax(1.0f,attackMs*.001f*safeSr);
  decayCoeff=std::exp(-1.0f/(juce::jmax(.001f,synthDecayMs*.001f)*safeSr));
  releaseCoeff=std::exp(-1.0f/(juce::jmax(.001f,releaseMs*.001f)*safeSr));
  const float fc=juce::jlimit(20.0f,safeSr*.45f,cutoff);
  filterG=std::tan(juce::MathConstants<float>::pi*fc/safeSr);
  filterK=2.0f-1.94f*juce::jlimit(0.0f,.98f,resonance);
  const float inverseDenominator=1.0f/(1.0f+filterG*(filterG+filterK));
  filterA1=inverseDenominator;filterA2=filterG*inverseDenominator;filterA3=filterG*filterA2;
 }
 void configureSynth(float a,float d,float s,float r,float fc,float q,int w1,int w2,bool second,float mix,float cents,int oct,bool filter){
  if(attackMs==a&&synthDecayMs==d&&sustain==s&&releaseMs==r&&cutoff==fc&&resonance==q&&wave1==w1&&wave2==w2&&osc2Enabled==second&&osc2Mix==mix&&detuneCents==cents&&octave2==oct&&filterEnabled==filter)return;
  attackMs=a;synthDecayMs=d;sustain=s;releaseMs=r;cutoff=fc;resonance=q;wave1=w1;wave2=w2;osc2Enabled=second;osc2Mix=mix;detuneCents=cents;octave2=oct;filterEnabled=filter;osc2Ratio=std::exp2((octave2*12.0+detuneCents/100.0)/12.0);rebuildSynthCoefficients();
 }
 void synthNoteOn(int note,float v){midiNote=juce::jlimit(0,127,note);synthBaseHz=440.0*std::exp2((midiNote-69)/12.0);velocity=juce::jlimit(0.0f,1.0f,v);stage=Stage::attack;phase=phase2=0;if(synthEnv<.001f)synthEnv=0;}
 void synthNoteOff(){if(stage!=Stage::off&&stage!=Stage::release)stage=Stage::release;}
 void trigger(float v,int k){kind=k;velocity=juce::jlimit(0.0f,1.0f,v);if(k==11){synthNoteOn(midiNote,v);return;}rebuildDrumCoefficients();env=1.0f;noiseEnv=1.0f;metalPhase=0;phase=phase2=phase3=0;noiseLow=noiseBand=resonator1=resonator2=0;}
 float advanceSynthEnvelope(){
  switch(stage){
   case Stage::attack:synthEnv=juce::jmin(1.0f,synthEnv+attackStep);if(synthEnv>=1.0f)stage=Stage::decay;break;
   case Stage::decay:synthEnv=sustain+(synthEnv-sustain)*decayCoeff;if(std::abs(synthEnv-sustain)<1.0e-4f){synthEnv=sustain;stage=Stage::sustain;}break;
   case Stage::sustain:synthEnv=sustain;break;
   case Stage::release:synthEnv*=releaseCoeff;if(synthEnv<1.0e-5f){synthEnv=0;stage=Stage::off;}break;
   case Stage::off:return 0;
  }
  return synthEnv*velocity;
 }
 float processSynth(){
  const float amp=advanceSynthEnvelope();if(amp<=0)return 0;
  const double dt1=juce::jlimit(0.0,.45,synthBaseHz/sr),dt2=juce::jlimit(0.0,.45,dt1*osc2Ratio);
  const float a=oscillator(wave1,phase,dt1),b=oscillator(wave2,phase2,dt2);
  phase+=dt1;phase-=std::floor(phase);phase2+=dt2;phase2-=std::floor(phase2);
  float x=osc2Enabled?a*(1.0f-osc2Mix)+b*osc2Mix:a;
  if(filterEnabled){const float v3=x-ic2eq;const float v1=filterA1*ic1eq+filterA2*v3;const float v2=ic2eq+filterA2*ic1eq+filterA3*v3;ic1eq=2.0f*v1-ic1eq;ic2eq=2.0f*v2-ic2eq;x=v2;}
  return std::tanh(x*amp*1.5f);
 }
 float process(){
  if(kind==11)return processSynth();
  if(env<1.0e-5f)return 0.0f;
  const float n=noise();const float t=tune, c=character, b=tone;double f1=140.0,f2=230.0;float x=0;
  const float pitchSweep=env*env;
  switch(kind){
   case 0:{ // analog bass drum: descending membrane pitch, sine body and controlled drive; no noise click
    f1=juce::jmap(t,30.0f,96.0f)+pitchSweep*juce::jmap(c,8.0f,115.0f);
    phase+=f1/sr;phase-=std::floor(phase);
    const float body=sine(phase),second=sine(phase*2.01);
    x=body*(.92f+.08f*b)+second*(.04f+.24f*c);
    const float punch=std::exp(-((float)(1.0-env))* (5.0f+25.0f*(1.0f-c)));
    x*=.78f+.22f*punch;x=std::tanh(x*(1.0f+3.8f*c));break;
   }
   case 1:{ // snare: tuned shell + independently shaped, band-limited wire/noise spectrum
    f1=juce::jmap(t,150.0f,270.0f);f2=f1*1.47;
    phase+=f1/sr;phase-=std::floor(phase);phase2+=f2/sr;phase2-=std::floor(phase2);
    noiseLow+=snareNoiseCoeff*(n-noiseLow);const float snareNoise=n-noiseLow;
    const float snappy=juce::jmap(c,.12f,1.5f);
    x=sine(phase)*juce::jmap(b,.56f,.25f)+sine(phase2)*.18f+snareNoise*snappy*noiseEnv;
    noiseEnv*=snareWireCoeff;
    x*=.65f+.35f*std::exp(-((float)(1.0-env))*18.0f);break;
   }
   case 2:{ // dual-head tom: tunable shell and controlled head detune
    f1=juce::jmap(t,58.0f,225.0f)*(1.0+pitchSweep*.16);f2=f1*(1.006+c*.065);
    phase+=f1/sr;phase-=std::floor(phase);phase2+=f2/sr;phase2-=std::floor(phase2);
    x=sine(phase)*(1.0f-.32f*b)+sine(phase2)*(.12f+.46f*b);x=std::tanh(x*(1.0f+1.5f*c));break;
   }
   case 3:{ // metallic hats: six inharmonic partials plus a tunable, high-passed noise layer
    f1=juce::jmap(t,3500.0f,10500.0f);phase+=f1/sr;phase-=std::floor(phase);phase2+=f1*1.342/sr;phase2-=std::floor(phase2);phase3+=f1*1.731/sr;phase3-=std::floor(phase3);
    noiseLow+=juce::jmap(b,.04f,.55f)*(n-noiseLow);const float hiss=n-noiseLow;
    x=(sine(phase)+sine(phase2)*.72f+sine(phase3)*.48f)*(.12f+.38f*c)+hiss*(.16f+.72f*b);break;
   }
   case 4:{ // digital FM drum: carrier, modulator ratio and index are independent macros
    f1=juce::jmap(t,55.0f,440.0f)*(1.0+pitchSweep*.5);f2=f1*juce::jmap(c,1.0f,5.0f);
    phase+=f1/sr;phase-=std::floor(phase);phase2+=f2/sr;phase2-=std::floor(phase2);
    x=sine(phase+sine(phase2)*juce::jmap(b,.015f,.32f));break;
   }
   case 5:{ // phase percussion: moving phase distortion amount and ratio
    f1=juce::jmap(t,75.0f,820.0f);f2=f1*juce::jmap(b,1.01f,3.5f);
    phase+=f1/sr;phase-=std::floor(phase);phase2+=f2/sr;phase2-=std::floor(phase2);
    x=sine(phase+sine(phase2)*(.05f+c*.75f));break;
   }
   case 6:{ // wave metal: inharmonic oscillator bank, brightness and ring
    f1=juce::jmap(t,110.0f,1500.0f);phase+=f1/sr;phase-=std::floor(phase);phase2+=f1*1.41421356/sr;phase2-=std::floor(phase2);phase3+=f1*2.2360679/sr;phase3-=std::floor(phase3);
    x=(sine(phase)*.55f+sine(phase2)*(.1f+.35f*b)+sine(phase3)*(.05f+.35f*c));break;
   }
   case 7:{ // digital resonator: stable damped mode excited by a shaped noise transient
    f1=juce::jmap(t,180.0f,5200.0f);const float w=juce::MathConstants<float>::twoPi*(float)f1/(float)sr;
    const float radius=juce::jmap(c,.84f,.985f),feedback=2.0f*radius*std::cos(w);
    const float excitation=(n-noiseLow)*(1.0f-radius)*.8f;noiseLow+=juce::jmap(b,.04f,.5f)*(n-noiseLow);
    const float y=excitation+feedback*resonator1-radius*radius*resonator2;resonator2=resonator1;resonator1=juce::jlimit(-8.0f,8.0f,y);
    x=resonator1*(.35f+.9f*c)+noiseLow*(.08f+.5f*b);break;
   }
   default:{f1=juce::jmap(t,65.0f,880.0f);phase+=f1/sr;phase-=std::floor(phase);x=sine(phase)*.7f+n*.3f;break;}
  }
  env*=drumDecayCoeff;
  return std::tanh(x*env*velocity*1.7f);
 }
};

class DrumEngine {
public:
 enum class ChangeMode{Immediate=0,NextBeat,NextBar,EndPattern};
 static constexpr int voices=12,patterns=32;
 DrumEngine(){trackStep.fill(-1);artifactRandom.setSeed((int64_t)artifactSeed);for(auto&patternIndex:audioSongPattern)patternIndex.store(0,std::memory_order_relaxed);for(auto&bars:audioSongBars)bars.store(4,std::memory_order_relaxed);}
 void prepare(double s){sr=s;for(auto&v:dsp)v.prepare(s);rebuildCompressorCoefficients();reverb.setSampleRate(s);juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=reverbEnabled?reverbMix:0.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);delayL.assign((size_t)(s*2.0),0.f);delayR.assign((size_t)(s*2.0),0.f);initialiseFactoryPatterns();}
 void trigger(int i,float v){if(i>=0&&i<voices){if(i>=8&&i<=10&&samples[(size_t)(i-8)].loaded){sampleVelocity[(size_t)(i-8)]=v;samples[(size_t)(i-8)].trigger();}else dsp[(size_t)i].trigger(v,i);}}
 void triggerSynthNote(int note,float velocity){dsp[11].synthNoteOn(note,velocity);synthGateSamples=-1;}
 void releaseSynthNote(int note){if(note==dsp[11].midiNote){dsp[11].synthNoteOff();synthGateSamples=0;}}
 void configureSynth(float a,float d,float s,float r,float cutoff,float resonance,int wave1,int wave2,bool second,float mix,float cents,int octave,bool filter){synthAttackMs=a;synthDecayMs=d;synthSustain=s;synthReleaseMs=r;synthCutoff=cutoff;synthResonance=resonance;synthWave1=wave1;synthWave2=wave2;synthOsc2=second;synthOsc2Mix=mix;synthDetuneCents=cents;synthOctave2=octave;synthFilter=filter;dsp[11].configureSynth(a,d,s,r,cutoff,resonance,wave1,wave2,second,mix,cents,octave,filter);}
 int getSynthMidiNote()const{return dsp[11].midiNote;}
 void setPattern(int p){current=juce::jlimit(0,patterns-1,p);step=-1;trackStep.fill(-1);samplesToStep=0;hasPpq=false;}
 void setPreviewPlaying(bool shouldPlay){previewPlaying.store(shouldPlay,std::memory_order_relaxed);}
 void setVirtualDrummer(bool enabled){if(enabled==virtualDrummerEnabled)return;virtualDrummerEnabled=enabled;generatedBars=0;if(enabled){savedPattern=pattern(current);auto&p=pattern(current);p.length=juce::jmax(16,p.length);for(int v=0;v<voices;v++)p.trackLength[(size_t)v]=p.length;buildVirtualBar(0);}else pattern(current)=savedPattern;}
 void setVirtualDrummerSettings(int styleIn,int divisionIn,float energyIn,float humanizeIn,float syncopationIn,int phraseBarsIn,int fillEveryIn=4,int breakEveryIn=8){drummerStyle=juce::jlimit(0,4,styleIn);hatDivision=divisionIn==1?1:(divisionIn==2?2:4);drummerEnergy=juce::jlimit(0.f,1.f,energyIn);drummerHumanize=juce::jlimit(0.f,1.f,humanizeIn);drummerSyncopation=juce::jlimit(0.f,1.f,syncopationIn);phraseBars=juce::jlimit(1,16,phraseBarsIn);fillEveryBars=juce::jlimit(1,32,fillEveryIn);breakEveryBars=juce::jlimit(1,32,breakEveryIn);if(virtualDrummerEnabled)buildVirtualBar(generatedBars);}
 bool isVirtualDrummerEnabled()const{return virtualDrummerEnabled;}
 static constexpr int maxSongEntries=16;
 int getSongLength()const{return songLength;}
 int getSongPosition()const{return songPosition.load(std::memory_order_relaxed);}
 SongEntry getSongEntry(int slot)const{return slot>=0&&slot<maxSongEntries?songChain[(size_t)slot]:SongEntry{};}
 bool isSongPlaying()const{return songPlaying;}
 void setSongEntry(int slot,int patternIndex,int section,int bars=4){if(slot<0||slot>=maxSongEntries)return;songChain[(size_t)slot]={juce::jlimit(0,patterns-1,patternIndex),juce::jlimit(0,6,section),juce::jlimit(1,64,bars)};songLength=juce::jmax(songLength,slot+1);audioSongPattern[(size_t)slot].store(songChain[(size_t)slot].pattern,std::memory_order_relaxed);audioSongBars[(size_t)slot].store(songChain[(size_t)slot].bars,std::memory_order_relaxed);activeSongLength.store(songLength,std::memory_order_relaxed);}
 void removeSongEntry(int slot){if(slot<0||slot>=songLength)return;for(int i=slot;i<songLength-1;i++){songChain[(size_t)i]=songChain[(size_t)(i+1)];audioSongPattern[(size_t)i].store(songChain[(size_t)i].pattern,std::memory_order_relaxed);audioSongBars[(size_t)i].store(songChain[(size_t)i].bars,std::memory_order_relaxed);}--songLength;activeSongLength.store(songLength,std::memory_order_relaxed);if(songLength==0){songPlaying=false;songPosition.store(0,std::memory_order_relaxed);}else songPosition.store(juce::jlimit(0,songLength-1,getSongPosition()),std::memory_order_relaxed);}
 void setSongPlaying(bool shouldPlay){songPlaying=shouldPlay&&songLength>0;if(songPlaying){songPosition.store(0,std::memory_order_relaxed);songTicks=0;setPattern(audioSongPattern[0].load(std::memory_order_relaxed));}}
 void requestPattern(int p){p=juce::jlimit(0,patterns-1,p);if(songPlaying)setSongPlaying(false);if(changeMode==ChangeMode::Immediate)setPattern(p);else pendingPattern=p;}
 void setChangeMode(ChangeMode m){changeMode=m;}
 ChangeMode getChangeMode()const{return changeMode;}
 int getPattern()const{return current;} int getCurrentStep()const{return step;}
 int getTrackStep(int v)const{return v>=0&&v<voices?trackStep[(size_t)v]:0;}
 void setTrackLength(int p,int v,int length){if(v>=0&&v<voices)pattern(p).trackLength[(size_t)v]=juce::jlimit(1,64,length);}
 Pattern& pattern(int p){return pats[(size_t)juce::jlimit(0,patterns-1,p)];}
 void setHost(double b,bool play){bpm=b>20?b:120;hostPlaying=play;} void setHostPpq(double ppq){hostPpq=ppq;hasPpq=true;}
 void setSwing(float s){swing=juce::jlimit(0.0f,.75f,s);}
 void setReverb(float size,float damping,float mix){const float newSize=juce::jlimit(0.f,1.f,size),newDamping=juce::jlimit(0.f,1.f,damping),newMix=juce::jlimit(0.f,1.f,mix);if(reverbConfigured&&newSize==reverbSize&&newDamping==reverbDamping&&newMix==reverbMix&&reverbAppliedEnabled==reverbEnabled)return;reverbSize=newSize;reverbDamping=newDamping;reverbMix=newMix;juce::Reverb::Parameters rp;rp.roomSize=reverbSize;rp.damping=reverbDamping;rp.wetLevel=reverbEnabled?reverbMix:0.0f;rp.dryLevel=0.0f;rp.width=1.0f;reverb.setParameters(rp);reverbConfigured=true;reverbAppliedEnabled=reverbEnabled;}
 void setReverbEnabled(bool enabled){reverbEnabled=enabled;setReverb(reverbSize,reverbDamping,reverbMix);}
 void setDelay(float beats,float feedback,float mix){delayBeats=juce::jlimit(.125f,2.0f,beats);delayFeedback=juce::jlimit(0.f,.88f,feedback);delayMix=juce::jlimit(0.f,1.f,mix);}
 void setDelayEnabled(bool enabled){delayEnabled=enabled;}
 void setDelayPingPong(bool enabled){delayPingPong=enabled;}
 void setCompressor(bool enabled,float thresholdDb,float ratio,float attackMs,float releaseMs){compressorEnabled=enabled;const float th=juce::jlimit(-36.f,0.f,thresholdDb),ra=juce::jlimit(1.f,20.f,ratio),at=juce::jlimit(.1f,100.f,attackMs),rel=juce::jlimit(10.f,500.f,releaseMs);if(th!=compressorThresholdDb||ra!=compressorRatio||at!=compressorAttackMs||rel!=compressorReleaseMs){compressorThresholdDb=th;compressorRatio=ra;compressorAttackMs=at;compressorReleaseMs=rel;rebuildCompressorCoefficients();}}
 void setCompressorBoost(float db){compressorBoostDb=juce::jlimit(0.0f,24.0f,db);compressorBoostLinear=std::pow(10.0f,compressorBoostDb*.05f);}
 void setArtifactSettings(bool enabled,float amount,uint32_t seed){seedRandomize=enabled;artifactAmount=juce::jlimit(0.0f,1.0f,amount);if(seed!=artifactSeed){artifactSeed=seed;artifactRandom.setSeed((int64_t)seed);}}
 bool isSeedRandomizeEnabled()const{return seedRandomize;}float getArtifactAmount()const{return artifactAmount;}uint32_t getArtifactSeed()const{return artifactSeed;}
 void setMaster(float bass,float treble,float comp){masterBass=juce::jlimit(.4f,1.8f,bass);masterTreble=juce::jlimit(.4f,1.8f,treble);masterComp=juce::jlimit(.6f,3.0f,comp);}
 void setVoiceParam(int i,int param,float value){if(i<0||i>=voices)return;value=juce::jlimit(0.0f,1.0f,value);auto&v=dsp[(size_t)i];float*target=param==0?&v.tune:(param==1?&v.decay:(param==2?&v.tone:&v.character));if(std::abs(*target-value)>1.0e-5f){*target=value;if(param==1||param==2||param==3)v.rebuildDrumCoefficients();}if(i>=8&&i<=10){auto&s=samples[(size_t)(i-8)];if(param==0)s.pitch=juce::jmap(value,0.0f,1.0f,.5f,2.0f);else if(param==3)s.reverse=value>.75f;}}
 float getVoiceParam(int i,int param)const{if(i<0||i>=voices)return .5f;auto const&v=dsp[(size_t)i];return param==0?v.tune:param==1?v.decay:param==2?v.tone:v.character;}
 juce::ValueTree saveState()const{juce::ValueTree root("ENGINE");root.setProperty("pattern",current,nullptr);root.setProperty("changeMode",(int)changeMode,nullptr);root.setProperty("masterBass",masterBass,nullptr);root.setProperty("masterTreble",masterTreble,nullptr);root.setProperty("masterComp",masterComp,nullptr);root.setProperty("reverbSize",reverbSize,nullptr);root.setProperty("reverbDamping",reverbDamping,nullptr);root.setProperty("reverbMix",reverbMix,nullptr);root.setProperty("reverbEnabled",reverbEnabled,nullptr);root.setProperty("delayBeats",delayBeats,nullptr);root.setProperty("delayFeedback",delayFeedback,nullptr);root.setProperty("delayMix",delayMix,nullptr);root.setProperty("delayEnabled",delayEnabled,nullptr);root.setProperty("delayPingPong",delayPingPong,nullptr);root.setProperty("compressorEnabled",compressorEnabled,nullptr);root.setProperty("compressorThresholdDb",compressorThresholdDb,nullptr);root.setProperty("compressorRatio",compressorRatio,nullptr);root.setProperty("compressorAttackMs",compressorAttackMs,nullptr);root.setProperty("compressorReleaseMs",compressorReleaseMs,nullptr);root.setProperty("compressorBoostDb",compressorBoostDb,nullptr);root.setProperty("synthAttackMs",synthAttackMs,nullptr);root.setProperty("synthDecayMs",synthDecayMs,nullptr);root.setProperty("synthSustain",synthSustain,nullptr);root.setProperty("synthReleaseMs",synthReleaseMs,nullptr);root.setProperty("synthCutoff",synthCutoff,nullptr);root.setProperty("synthResonance",synthResonance,nullptr);root.setProperty("synthWave1",synthWave1,nullptr);root.setProperty("synthWave2",synthWave2,nullptr);root.setProperty("synthOsc2",synthOsc2,nullptr);root.setProperty("synthOsc2Mix",synthOsc2Mix,nullptr);root.setProperty("synthDetuneCents",synthDetuneCents,nullptr);root.setProperty("synthOctave2",synthOctave2,nullptr);root.setProperty("synthFilter",synthFilter,nullptr);root.setProperty("seedRandomize",seedRandomize,nullptr);root.setProperty("artifactAmount",artifactAmount,nullptr);root.setProperty("artifactSeed",(int64_t)artifactSeed,nullptr);for(int v=0;v<voices;v++){juce::ValueTree voice("VOICE");voice.setProperty("index",v,nullptr);if(v>=8&&v<=10)voice.setProperty("samplePath",samples[(size_t)(v-8)].sourcePath,nullptr);voice.setProperty("gain",gain[v],nullptr);voice.setProperty("pan",panorama[v],nullptr);voice.setProperty("rev",reverbSend[v],nullptr);voice.setProperty("del",delaySend[v],nullptr);voice.setProperty("filter",channelFilter[v],nullptr);voice.setProperty("drive",channelDrive[v],nullptr);voice.setProperty("mute",mute[v],nullptr);voice.setProperty("solo",solo[v],nullptr);voice.setProperty("tune",dsp[v].tune,nullptr);voice.setProperty("decay",dsp[v].decay,nullptr);voice.setProperty("tone",dsp[v].tone,nullptr);voice.setProperty("char",dsp[v].character,nullptr);root.addChild(voice,-1,nullptr);}for(int pidx=0;pidx<patterns;pidx++){juce::ValueTree pat("PATTERN");pat.setProperty("index",pidx,nullptr);pat.setProperty("length",pats[pidx].length,nullptr);for(int v=0;v<voices;v++)pat.setProperty("trackLength"+juce::String(v),pats[pidx].trackLength[(size_t)v],nullptr);for(int v=0;v<voices;v++)for(int s=0;s<64;s++){auto const&st=pats[pidx].step[v][s];if(st.on||st.velocity!=.85f||st.probability!=1.f||st.ratchet!=1||st.micro!=0.f||st.accent||st.flam||st.note!=60){juce::ValueTree n("STEP");n.setProperty("v",v,nullptr);n.setProperty("s",s,nullptr);n.setProperty("on",st.on,nullptr);n.setProperty("vel",st.velocity,nullptr);n.setProperty("prob",st.probability,nullptr);n.setProperty("rat",st.ratchet,nullptr);n.setProperty("micro",st.micro,nullptr);n.setProperty("accent",st.accent,nullptr);n.setProperty("flam",st.flam,nullptr);n.setProperty("note",st.note,nullptr);pat.addChild(n,-1,nullptr);}}root.addChild(pat,-1,nullptr);}juce::ValueTree song("SONG");song.setProperty("length",songLength,nullptr);for(int i=0;i<songLength;i++){juce::ValueTree entry("ENTRY");entry.setProperty("pattern",songChain[(size_t)i].pattern,nullptr);entry.setProperty("section",songChain[(size_t)i].section,nullptr);entry.setProperty("bars",songChain[(size_t)i].bars,nullptr);song.addChild(entry,-1,nullptr);}root.addChild(song,-1,nullptr);return root;}
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
  compressorReleaseMs=(float)root.getProperty("compressorReleaseMs",120.0);compressorBoostDb=(float)root.getProperty("compressorBoostDb",0.0);
  configureSynth((float)root.getProperty("synthAttackMs",8.0),(float)root.getProperty("synthDecayMs",180.0),(float)root.getProperty("synthSustain",.72),(float)root.getProperty("synthReleaseMs",140.0),(float)root.getProperty("synthCutoff",12000.0),(float)root.getProperty("synthResonance",.12),(int)root.getProperty("synthWave1",0),(int)root.getProperty("synthWave2",1),(bool)root.getProperty("synthOsc2",true),(float)root.getProperty("synthOsc2Mix",.35),(float)root.getProperty("synthDetuneCents",7.0),(int)root.getProperty("synthOctave2",0),(bool)root.getProperty("synthFilter",true));seedRandomize=(bool)root.getProperty("seedRandomize",false);artifactAmount=(float)root.getProperty("artifactAmount",.25);artifactSeed=(uint32_t)(int64_t)root.getProperty("artifactSeed",70271);artifactRandom.setSeed((int64_t)artifactSeed);
  setReverb(reverbSize,reverbDamping,reverbMix);rebuildCompressorCoefficients();compressorBoostLinear=std::pow(10.0f,compressorBoostDb*.05f);

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
    dsp[v].rebuildDrumCoefficients();
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
    pats[pidx].length=juce::jlimit(1,64,(int)child.getProperty("length",16));for(int v=0;v<voices;v++)pats[pidx].trackLength[(size_t)v]=juce::jlimit(1,64,(int)child.getProperty("trackLength"+juce::String(v),16));
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
     songChain[(size_t)index]={(int)entry.getProperty("pattern",0),(int)entry.getProperty("section",0),(int)entry.getProperty("bars",4)};
     audioSongPattern[(size_t)index].store(songChain[(size_t)index].pattern,std::memory_order_relaxed);
     audioSongBars[(size_t)index].store(songChain[(size_t)index].bars,std::memory_order_relaxed);
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
  bool anySolo=false;std::array<float,voices> panL{},panR{},driveGain{},driveNorm{};
  for(int v=0;v<voices;v++){anySolo|=solo[(size_t)v];const float pan=juce::jlimit(-1.0f,1.0f,panorama[(size_t)v]);panL[(size_t)v]=std::sqrt(.5f*(1.0f-pan));panR[(size_t)v]=std::sqrt(.5f*(1.0f+pan));const float amount=1.0f+juce::jlimit(0.0f,1.0f,channelDrive[(size_t)v])*5.0f;driveGain[(size_t)v]=amount;driveNorm[(size_t)v]=juce::jmax(1.0e-5f,std::tanh(amount));}
  if(hostPlaying&&hasPpq){int target=((int)std::floor(hostPpq*4.0))%juce::jmax(1,pattern(current).length);if(target!=step){step=target-1;samplesToStep=0;for(int v=0;v<voices;v++)trackStep[(size_t)v]=(target%juce::jmax(1,pattern(current).trackLength[(size_t)v]))-1;}hasPpq=false;}
  for(int i=0;i<n;i++){if(synthGateSamples>0&&--synthGateSamples==0)dsp[11].synthNoteOff();for(int v=0;v<voices;v++){if(pendingMicro[v]>0&&--pendingMicro[v]==0){if(v==11){triggerSynthNote(pendingNote[v],pendingVelocity[v]);synthGateSamples=(int)(base*.82);}else trigger(v,pendingVelocity[v]);}if(pendingFlam[v]>0&&--pendingFlam[v]==0)trigger(v,.72f);if(pendingRatchet[v]>0&&ratchetCounter[v]<=0){trigger(v,.68f);pendingRatchet[v]--;ratchetCounter[v]=(int)juce::jmax(1.0,base/(pendingRatchet[v]+2));}if(ratchetCounter[v]>0)--ratchetCounter[v];}
   const bool sequencePlaying=hostPlaying||previewPlaying.load(std::memory_order_relaxed);
   if(sequencePlaying&&samplesToStep<=0){advance();double d=base*((step&1)?1.0+swing:1.0-swing);samplesToStep+=juce::jmax(1.0,d);}
   samplesToStep-=1.0; float l=0,r=0;
   float revSendL=0,revSendR=0,delSendL=0,delSendR=0;for(int v=0;v<voices;v++){float raw=(v>=8&&v<=10&&samples[(size_t)(v-8)].loaded)?samples[(size_t)(v-8)].process(sr)*sampleVelocity[(size_t)(v-8)]:dsp[v].process();channelLP[v]+=juce::jmap(channelFilter[v],.02f,.55f)*(raw-channelLP[v]);float filtered=raw*(1.0f-channelFilter[v])+channelLP[v]*channelFilter[v];float driven=std::tanh(filtered*driveGain[(size_t)v])/driveNorm[(size_t)v];bool audible=!mute[v]&&(!anySolo||solo[v]);float x=(audible?driven:0.0f)*gain[v];float vl=x*panL[(size_t)v],vr=x*panR[(size_t)v];l+=vl;r+=vr;
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
   applyRandomArtifact(l,r);
   if(compressorEnabled){
    l*=compressorBoostLinear;r*=compressorBoostLinear;
    const float detector=juce::jmax(std::abs(l),std::abs(r));
    const float envCoeff=detector>compressorEnvelope?compressorAttackCoeff:compressorReleaseCoeff;
    compressorEnvelope=detector+(compressorEnvelope-detector)*envCoeff;
    float target=1.0f;
    if(compressorEnvelope>compressorThresholdLinear){const float reduced=compressorThresholdLinear+(compressorEnvelope-compressorThresholdLinear)/compressorRatio;target=reduced/juce::jmax(compressorEnvelope,1.0e-6f);}
    const float gainCoeff=target<compressorGain?compressorAttackCoeff:compressorReleaseCoeff;
    compressorGain=target+(compressorGain-target)*gainCoeff;l*=compressorGain;r*=compressorGain;
   }
   const float saturation=drive*.24f*masterComp;l=std::tanh(l*saturation);r=std::tanh(r*saturation);
   const float mono=.5f*(l+r),low=masterLow.process(mono),high=mono-low;
   float eq=low*masterBass+high*masterTreble;
   const float eqDelta=eq-mono;l+=eqDelta;r+=eqDelta;
   if(out.getNumChannels()>0)out.setSample(0,i,l);if(out.getNumChannels()>1)out.setSample(1,i,r);
  }
 }
 float drive=1.15f,masterBass=1.0f,masterTreble=1.0f,masterComp=1.35f,reverbSize=.45f,reverbDamping=.55f,reverbMix=.35f,delayBeats=.75f,delayFeedback=.36f,delayMix=.45f,compressorThresholdDb=-18.f,compressorRatio=3.f,compressorAttackMs=10.f,compressorReleaseMs=120.f,compressorBoostDb=0.f; bool reverbEnabled=true,delayEnabled=true,delayPingPong=true,compressorEnabled=true; std::array<float,voices> gain{1,1,1,1,1,1,1,1,1,1,1,1},panorama{},reverbSend{.08f,.12f,.08f,.16f,.12f,.12f,.14f,.18f,.10f,.10f,.10f,.15f},delaySend{0,0,0,.05f,.08f,.08f,.10f,.12f,.08f,.08f,.08f,.12f},channelFilter{},channelDrive{}; std::array<bool,voices> mute{},solo{};
 friend class JerzyDrumMachineAudioProcessor;
private:
 std::array<SongEntry,maxSongEntries>songChain{};std::array<std::atomic<int>,maxSongEntries>audioSongPattern{},audioSongBars{};std::atomic<int>activeSongLength{0},songPosition{0};int songLength=0,songTicks=0;std::atomic<bool>songPlaying{false};
 Pattern savedPattern{}; std::atomic<bool>virtualDrummerEnabled{false}; int generatedBars=0,drummerStyle=0,hatDivision=2,phraseBars=4,fillEveryBars=4,breakEveryBars=8; float drummerEnergy=.55f,drummerHumanize=.2f,drummerSyncopation=.25f;bool reverbConfigured=false,reverbAppliedEnabled=true;
 bool seedRandomize=false;uint32_t artifactSeed=70271;float artifactAmount=.25f,artifactWet=.65f,artifactScale=1024,artifactHoldL=0,artifactHoldR=0;int artifactMode=0,artifactRemaining=0,artifactPeriod=1,artifactCounter=0;juce::Random artifactRandom;
 float compressorThresholdLinear=.12589f,compressorAttackCoeff=.8f,compressorReleaseCoeff=.99f,compressorBoostLinear=1,compressorGain=1;int synthGateSamples=0;
 float synthAttackMs=8,synthDecayMs=180,synthSustain=.72f,synthReleaseMs=140,synthCutoff=12000,synthResonance=.12f,synthOsc2Mix=.35f,synthDetuneCents=7;int synthWave1=0,synthWave2=1,synthOctave2=0;bool synthOsc2=true,synthFilter=true;
 void rebuildCompressorCoefficients(){const float safeSr=(float)juce::jmax(8000.0,sr);compressorThresholdLinear=std::pow(10.0f,compressorThresholdDb*.05f);compressorAttackCoeff=std::exp(-1.0f/(juce::jmax(.1f,compressorAttackMs)*.001f*safeSr));compressorReleaseCoeff=std::exp(-1.0f/(juce::jmax(10.f,compressorReleaseMs)*.001f*safeSr));}
 void advance(){
  auto&p=pattern(current);step=(step+1)%juce::jmax(1,p.length);
  for(int v=0;v<voices;v++){
   const int len=juce::jlimit(1,64,p.trackLength[(size_t)v]);trackStep[(size_t)v]=(trackStep[(size_t)v]+1)%len;
   auto&st=p.step[v][(size_t)trackStep[(size_t)v]];
   if(st.on&&random.nextFloat()<=st.probability){
    const float velocity=juce::jlimit(0.f,1.f,st.velocity*(st.accent?1.18f:1.f));
    const int microDelay=juce::jmax(0,(int)(st.micro*sr*.03f));
    if(microDelay>0){pendingMicro[v]=microDelay;pendingVelocity[v]=velocity;pendingNote[v]=st.note;}
    else if(v==11){triggerSynthNote(st.note,velocity);synthGateSamples=(int)(sr*60.0/bpm*.205);}
    else trigger(v,velocity);
    if(st.flam)pendingFlam[v]=juce::jmax(1,(int)(sr*.018));if(st.ratchet>1)pendingRatchet[v]=st.ratchet-1;
   }
  }
   if(++barTicks>=16){barTicks=0;
   if(virtualDrummerEnabled){++generatedBars;buildVirtualBar(generatedBars);}
   if(seedRandomize&&artifactRandom.nextFloat()<artifactAmount*.65f){artifactMode=artifactRandom.nextInt(3);artifactRemaining=(int)(sr*juce::jmap(artifactRandom.nextFloat(),.012f,.13f));artifactPeriod=juce::jmax(1,(int)(sr*juce::jmap(artifactRandom.nextFloat(),.001f,.012f)));artifactWet=juce::jmap(artifactRandom.nextFloat(),.35f,.9f);artifactScale=std::exp2(juce::jmap(artifactWet,14.0f,5.0f));}
  }
  if(songPlaying){
   ++songTicks;const int slot=getSongPosition();const int duration=juce::jmax(16,audioSongBars[(size_t)slot].load(std::memory_order_relaxed)*16);
   if(songTicks>=duration){const int next=slot+1;pendingPattern=-1;songTicks=0;
    if(next>=activeSongLength.load(std::memory_order_relaxed)){songPlaying=false;if(!hostPlaying)previewPlaying.store(false,std::memory_order_relaxed);}
    else{songPosition.store(next,std::memory_order_relaxed);setPattern(audioSongPattern[(size_t)next].load(std::memory_order_relaxed));}
   }
  }else if(pendingPattern>=0){bool change=false;if(changeMode==ChangeMode::Immediate)change=true;else if(changeMode==ChangeMode::EndPattern&&step==p.length-1)change=true;else if(changeMode==ChangeMode::NextBeat&&(step%4)==0)change=true;else if(changeMode==ChangeMode::NextBar&&(step%16)==0)change=true;if(change){const int np=pendingPattern;pendingPattern=-1;setPattern(np);}}
 }
 void buildVirtualBar(int bar){
  auto&p=pattern(current);const int start=(bar*16)%juce::jmax(16,p.length);const int phrase=bar%juce::jmax(1,phraseBars);
  const bool breakBar=((bar+1)%breakEveryBars)==0;
  const bool fillBar=!breakBar&&((bar+1)%fillEveryBars)==0;
  // Phrase energy breathes gently while the style's kick and backbeat remain fixed.
  const float phraseLift=(phrase<phraseBars/2?phrase:phraseBars-1-phrase);
  const float energy=juce::jlimit(.1f,1.f,drummerEnergy+phraseLift*.025f);
  for(int i=0;i<16;i++)for(int v=0;v<voices;v++){
   const int s=(start+i)%p.length;auto&st=p.step[v][s];st.on=false;st.flam=false;st.ratchet=1;st.probability=1.f;st.micro=0.f;st.accent=false;
   bool hit=false;
   if(v==0){
    switch(drummerStyle){
     case 0: hit=(i==0||i==8||(drummerSyncopation>.55f&&energy>.45f&&(i==10||i==14)));break; // rock
     case 1: hit=(i%4==0);break; // four-on-the-floor
     case 2: hit=(i==0||i==6||i==10||(drummerSyncopation>.7f&&i==14));break; // breakbeat
     case 3: hit=(i==0||i==6||i==10||(drummerSyncopation>.45f&&i==14));break; // funk
     default:hit=(i==0||(drummerEnergy>.7f&&i==10));break; // minimal
    }
   }else if(v==1){
    hit=(i==4||i==12);
    if(drummerStyle==2&&i==15&&drummerEnergy>.6f)hit=true;
    if(drummerStyle==3&&i==14&&drummerSyncopation>.65f)hit=true;
    if(drummerStyle==4&&i==4)hit=false;
   }else if(v==3){
    hit=(i%hatDivision==0);
    if(drummerStyle==4&&i%4!=0)hit=false;
   }else if(v==4){
    hit=((drummerStyle==2&&drummerSyncopation>.45f&&(i==7||i==15))||
         (drummerStyle==3&&drummerSyncopation>.65f&&i==11));
   }
   // An explicit break drops the backbeat and leaves sparse time markers.
   if(breakBar){hit=(v==3&&(i==0||i==8))||(v==7&&i==15);}
   // Fills are short and repeatable: two snare/tom answers, one restrained ratchet.
   if(fillBar&&i>=12){
    if(v==1)hit=(i==12||i==15);
    else if(v==2)hit=(i==13||i==14);
    else if(v==3)hit=(i%2==0);
    else if(v==4)hit=(i==15);
   }
   st.on=hit;
   const float accent=(i%4==0)?1.f:.78f;
   const float velocityShape=1.0f+(energy-.5f)*.16f;
   st.velocity=juce::jlimit(.25f,1.f,accent*velocityShape*(v==3?.72f:1.f));
   st.micro=(i%2==1)?drummerHumanize*.035f:0.f;
   st.accent=(i%4==0)&&v==0;
   if(fillBar&&i==15&&(v==1||v==2))st.ratchet=2;
  }
 }
 void applyRandomArtifact(float&l,float&r){if(!seedRandomize||artifactRemaining<=0)return;--artifactRemaining;const float wet=artifactWet;if(artifactMode==0){const float cut=artifactRemaining>0?0.0f:1.0f;l=l*(1.0f-wet+wet*cut);r=r*(1.0f-wet+wet*cut);}else if(artifactMode==1){if(--artifactCounter<=0){artifactCounter=artifactPeriod;artifactHoldL=l;artifactHoldR=r;}l=l*(1.0f-wet)+artifactHoldL*wet;r=r*(1.0f-wet)+artifactHoldR*wet;}else{l=l*(1.0f-wet)+std::round(l*artifactScale)/artifactScale*wet;r=r*(1.0f-wet)+std::round(r*artifactScale)/artifactScale*wet;}}
 void initialiseFactoryPatterns(){for(int q=0;q<patterns;q++){auto&p=pats[q];p.length=16;for(int s=0;s<16;s++){p.step[0][s].on=s%4==0;p.step[1][s].on=s==4||s==12;p.step[3][s].on=s%2==0;p.step[4][s].on=(q%2)&&s%4==2;p.step[7][s].on=(q%3==2)&&(s==7||s==15);}}}
 double sr=44100,bpm=120,samplesToStep=0,hostPpq=0;float swing=0,complexity=.5f,syncopation=.25f,humanize=.15f,chaos=.1f,kickStability=.85f,snareStability=.9f,hatActivity=.65f,percActivity=.35f,compressorEnvelope=0;bool hostPlaying=false,hasPpq=false;std::atomic<bool>previewPlaying{false};std::array<int,voices>trackStep{},pendingFlam{},pendingRatchet{},ratchetCounter{},pendingMicro{};std::array<float,voices>pendingVelocity{};std::array<int,voices>pendingNote{};int pendingPattern=-1,barTicks=0;ChangeMode changeMode=ChangeMode::EndPattern;juce::Reverb reverb;std::vector<float>delayL,delayR;size_t delayPos=0;struct OnePole{float z=0;float process(float x){z+=.08f*(x-z);return z;}}masterLow;int current=0,step=-1;juce::Random random;std::array<VoiceDSP,voices>dsp{};std::array<SampleSlot,3>samples{};std::array<float,3>sampleVelocity{1,1,1};std::array<juce::String,3>sampleNames{};std::array<Pattern,patterns>pats{};std::array<float,voices>channelLP{};
};

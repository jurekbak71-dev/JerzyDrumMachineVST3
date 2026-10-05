#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

struct DrumVoice {
    double sr=44100.0, phase=0.0, env=0.0, noiseEnv=0.0, freq=80.0; int type=0;
    juce::Random rng;
    void prepare(double s){sr=s;}
    void trigger(float velocity,int t){type=t; phase=0; env=velocity; noiseEnv=velocity; freq=(t==0?55.0:(t==1?180.0:110.0+t*35.0));}
    float process(){
        if(env<0.00001f && noiseEnv<0.00001f) return 0;
        const double sweep=freq*(1.0+env*(type==0?2.8:0.35));
        phase+=juce::MathConstants<double>::twoPi*sweep/sr; if(phase>juce::MathConstants<double>::twoPi) phase-=juce::MathConstants<double>::twoPi;
        float tonal=(float)std::sin(phase);
        if(type>=4 && type<=7) tonal=(float)(0.65*std::sin(phase)+0.35*std::sin(phase*(type-1.25)));
        if(type==11) tonal=(float)(0.7*std::sin(phase)+0.3*std::sin(phase*2.01));
        float noise=rng.nextFloat()*2.0f-1.0f;
        float mix=(type==1||type==3||type==6)? tonal*0.45f+noise*0.55f : tonal*0.88f+noise*0.12f;
        env*= type==0?0.99935f:(type==3?0.994f:0.9975f); noiseEnv*=0.993f;
        return std::tanh(mix*env*1.5f);
    }
};

struct PatternStep { bool on=false; float velocity=.85f, probability=1.0f; int ratchet=1; };
struct Pattern { std::array<std::array<PatternStep,64>,12> step{}; int length=16; };

class DrumEngine {
public:
    static constexpr int voices=12, patterns=32;
    void prepare(double s){sr=s; for(auto&v:voice)v.prepare(s); initialiseFactoryPatterns();}
    void trigger(int i,float vel){if(i>=0&&i<voices) voice[(size_t)i].trigger(vel,i);}
    void setPattern(int p){current=juce::jlimit(0,patterns-1,p); step=-1; samplesToStep=0;}
    int getPattern()const{return current;}
    Pattern& pattern(int p){return pats[(size_t)juce::jlimit(0,patterns-1,p)];}
    void setHost(double bpmIn,bool playing){bpm=bpmIn>20?bpmIn:120; hostPlaying=playing;}
    void process(juce::AudioBuffer<float>& b){
        const int n=b.getNumSamples(); b.clear();
        double stepSamples=sr*60.0/bpm/4.0;
        for(int s=0;s<n;++s){
            if(hostPlaying && samplesToStep<=0){advance(); samplesToStep+=stepSamples;}
            samplesToStep-=1.0;
            float x=0; for(auto&v:voice)x+=v.process()*0.20f;
            x=std::tanh(x*drive);
            for(int c=0;c<b.getNumChannels();++c)b.setSample(c,s,x);
        }
    }
    float drive=1.15f;
private:
    void advance(){
        auto&p=pats[(size_t)current]; step=(step+1)%juce::jmax(1,p.length);
        for(int i=0;i<voices;++i){auto&st=p.step[(size_t)i][(size_t)step]; if(st.on && random.nextFloat()<=st.probability)trigger(i,st.velocity);}
    }
    void initialiseFactoryPatterns(){
        for(int p=0;p<patterns;++p){auto&x=pats[(size_t)p];x.length=16;
            for(int s=0;s<16;s++){x.step[0][s].on=(s%4==0)||(p%3==1&&s==10);x.step[1][s].on=(s==4||s==12);x.step[3][s].on=(s%2==0);x.step[4][s].on=(s==7||s==15);x.step[6][s].on=(p%2&&s%4==2);}
        }
    }
    double sr=44100,bpm=120,samplesToStep=0; bool hostPlaying=false; int current=0,step=-1; juce::Random random;
    std::array<DrumVoice,voices> voice{}; std::array<Pattern,patterns> pats{};
};

#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Pure send FX: allocate ONLY in prepare(), never from the realtime callback.
// Existing dry voices, mixer stem outputs and VST3 parameter identities are intact.
class DrumExtendedFX {
public:
    void prepare(double fs) {
        rate=juce::jmax(8000.0,fs);
        chorusL.assign((size_t)std::ceil(rate*0.065)+8,0.f);
        chorusR.assign(chorusL.size(),0.f);
        tapeL.assign((size_t)std::ceil(rate*0.090)+8,0.f);
        tapeR.assign(tapeL.size(),0.f);
        reset();
        configureTape(.42f,.75f);
    }
    void reset() {
        std::fill(chorusL.begin(),chorusL.end(),0.f);
        std::fill(chorusR.begin(),chorusR.end(),0.f);
        std::fill(tapeL.begin(),tapeL.end(),0.f);
        std::fill(tapeR.begin(),tapeR.end(),0.f);
        chorusPos=tapePos=0;chorusPhase=wowPhase=flutterPhase=0;
        flutterValue=0;noiseState=0x7D2A4E91u;dropoutRemaining=0;
        tapeLowL=tapeLowR=0;
    }
    // All expensive tape filter coefficient work occurs ONCE per host block,
    // not in the per-sample process callback.
    void configureTape(float age,float filter) noexcept {
        const double cutoff=juce::jmap((double)juce::jlimit(0.f,1.f,filter),1300.0,18500.0)*
            (1.0-.65*juce::jlimit(0.f,1.f,age));
        tapeFilterCoefficient=juce::jlimit(.0001f,.98f,
             (float)(1.0-std::exp(-juce::MathConstants<double>::twoPi*cutoff/rate)));
        const float saturation=1.f+4.5f*juce::jlimit(0.f,1.f,age);
        tapeGain=1.f/juce::jmax(.1f,std::tanh(saturation));
    }
    // chorus: depth 0..1 (0.25 to 7ms), spread 0..1 (stereo offset),
    // width 0..1 (opposed channel crossfeed). Returns WET ONLY.
    std::array<float,2> chorus(float inL,float inR,float depth,float width) noexcept {
        if(chorusL.empty())return {0.f,0.f};
        depth=juce::jlimit(0.f,1.f,depth);width=juce::jlimit(0.f,1.f,width);
        const float phase=chorusPhase;
        chorusPhase+=1.0f/(float)(rate*1.6);if(chorusPhase>=1.f)chorusPhase-=1.f;
        const float triangle=1.f-4.f*std::abs(phase-.5f);
        const float shift=phase+.25f<1.f?phase+.25f:phase-.75f;
        const float shifted=1.f-4.f*std::abs(shift-.5f);
        const double centre=.010*rate,excursion=(.00025+.00675*depth)*rate;
        const float a=read(chorusL,chorusPos,centre+excursion*triangle);
        const float b=read(chorusR,chorusPos,centre+excursion*shifted);
        chorusL[chorusPos]=finite(inL);chorusR[chorusPos]=finite(inR);
        chorusPos=(chorusPos+1)%chorusL.size();
        const float wide=.8f*width;
        return {finite(a*(1.f+wide)-b*wide),finite(b*(1.f+wide)-a*wide)};
    }
    // Magnetic tape with sound-bearing wear, not just a random bitcrusher:
    // short modulated transport delay (wow/flutter), saturating recording,
    // filtered playback, dropout and low-level hiss. Returns WET ONLY.
    std::array<float,2> tape(float inL,float inR,float age,float wear,float filter) noexcept {
        if(tapeL.empty())return {0.f,0.f};
        age=juce::jlimit(0.f,1.f,age);
        wear=juce::jlimit(0.f,1.f,wear);
        (void)filter; // filter coefficient cached via configureTape()
        const float speed=1.2f+.55f*age;
        wowPhase+=speed/(float)rate;if(wowPhase>=1.f)wowPhase-=1.f;
        flutterPhase+=(9.f+12.f*age)/(float)rate;if(flutterPhase>=1.f)flutterPhase-=1.f;
        if(++flutterCounter>=juce::jmax(12,(int)(rate/200.f))){
            flutterCounter=0;flutterTarget=randSigned();
        }
        flutterValue+=.004f*(flutterTarget-flutterValue);
        const double wow=std::sin(juce::MathConstants<double>::twoPi*wowPhase);
        const double flutter=std::sin(juce::MathConstants<double>::twoPi*flutterPhase);
        const double delaySeconds=.024 + .0035*age*wow + .0009*wear*flutter +
                                   .0015*wear*flutterValue;
        const double delay=juce::jlimit(1.0,(double)tapeL.size()-3.0,delaySeconds*rate);
        const float sat=1.f+4.5f*age;
        const float recL=std::tanh(finite(inL)*sat)*tapeGain;
        const float recR=std::tanh(finite(inR)*sat)*tapeGain;
        tapeL[tapePos]=recL;tapeR[tapePos]=recR;
        const float rawL=read(tapeL,tapePos,delay);
        const float rawR=read(tapeR,tapePos,delay);
        tapePos=(tapePos+1)%tapeL.size();
        tapeLowL+=tapeFilterCoefficient*(rawL-tapeLowL);
        tapeLowR+=tapeFilterCoefficient*(rawR-tapeLowR);
        if(dropoutRemaining>0)--dropoutRemaining;
        else if(wear>.02f && rand01()<(.5e-5f*wear*wear)){
            dropoutRemaining=(int)(rate*(.010+.040*rand01()));
        }
        const float drop=dropoutRemaining>0?(1.f-.85f*wear):1.f;
        const float hiss=(rand01()-.5f)*.008f*wear*age;
        const float crackle=(wear>.2f&&rand01()<wear*.000005f)?(.12f*wear*randSigned()):0.f;
        return {finite(tapeLowL*drop+hiss+crackle),finite(tapeLowR*drop-hiss+crackle)};
    }
private:
    static float finite(float x) noexcept {
        return std::isfinite(x)?juce::jlimit(-8.f,8.f,x):0.f;
    }
    static float read(const std::vector<float>& line,size_t write,double delay) noexcept {
        const double len=(double)line.size();
        double pos=(double)write-delay;
        if(pos<0)pos+=len;
        const size_t a=(size_t)pos;
        const size_t b=(a+1)%line.size();
        const float t=(float)(pos-(double)a);
        return line[a]*(1.f-t)+line[b]*t;
    }
    float rand01() noexcept {
        noiseState^=noiseState<<13;noiseState^=noiseState>>17;noiseState^=noiseState<<5;
        return (float)(noiseState & 0x00ffffffu)*(1.0f/16777216.f);
    }
    float randSigned() noexcept {return 2.f*rand01()-1.f;}
    double rate=44100;
    std::vector<float> chorusL,chorusR,tapeL,tapeR;
    size_t chorusPos=0,tapePos=0;
    float chorusPhase=0,wowPhase=0,flutterPhase=0;
    float flutterTarget=0,flutterValue=0,tapeLowL=0,tapeLowR=0;
    int flutterCounter=0,dropoutRemaining=0;
    uint32_t noiseState=0x7D2A4E91u;
    float tapeFilterCoefficient=.1f,tapeGain=1.f;
};

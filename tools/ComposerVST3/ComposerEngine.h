#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace jerzy_composer {
enum class Mode : int { Drums=0, Bass=1, Chords=2, Arp=3, Generative=4 };
inline std::uint32_t hash(std::uint32_t x) noexcept {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; return x ^ (x >> 16);
}
inline float random01(std::uint32_t seed, std::uint32_t index) noexcept {
    return static_cast<float>(hash(seed ^ hash(index))) * (1.0f / 4294967296.0f);
}
inline int clampMidi(int note) noexcept { return std::clamp(note,0,127); }
inline int scaleNote(int root,int degree,bool minor) noexcept {
    constexpr int major[]{0,2,4,5,7,9,11};
    constexpr int min[]{0,2,3,5,7,8,10};
    const int octave=degree>=0?degree/7:-((-degree+6)/7);
    const int di=degree-octave*7;
    return clampMidi(root+(minor?min[di]:major[di])+octave*12);
}
inline bool hit(int stepIndex,int voice,float density,int seed) noexcept {
    const int s=(stepIndex%16+16)%16;
    const float amount=std::clamp(density,0.0f,1.0f);
    bool base=false;
    switch(voice) {
        case 0:base=s==0||s==8||s==10;break;          // kick
        case 1:base=s==4||s==12;break;                 // snare
        case 2:base=(s%2)==0;break;                   // hat
        case 3:base=s==7||s==15;break;                 // open hat
        default:return false;
    }
    if(!base)return false;
    if(voice==0||voice==1)return true;
    return random01(static_cast<std::uint32_t>(seed),static_cast<std::uint32_t>(stepIndex*13+voice*911))<amount;
}
inline double swingOffset(int stepIndex,double divisionPpq,double swing) noexcept {
    return (stepIndex&1)?divisionPpq*std::clamp(swing,0.0,0.49):0.0;
}
inline int baseBassDegree(int stepIndex,int style) noexcept {
    static constexpr int patterns[5][16]={
       {0,0,0,4,0,0,2,0, 0,0,4,0,0,2,4,6},
       {0,0,4,4,0,0,4,0, 0,0,4,4,0,2,4,0},
       {0,2,4,0,2,4,6,2, 0,4,2,0,6,0,4,2},
       {0,0,4,0,2,0,0,6, 0,4,2,0,0,6,4,2},
       {0,0,0,4,0,0,0,0, 0,0,2,0,0,0,4,0}
    };
    return patterns[std::clamp(style,0,4)][(stepIndex%16+16)%16];
}
inline bool bassGate(int stepIndex,int style,float density,int seed) noexcept {
    const int s=(stepIndex%16+16)%16;
    if(s==0)return true;
    const float rates[5][4]={{.8f,.5f,.8f,.3f},{.5f,.2f,.7f,.2f},{.95f,.8f,.9f,.65f},{.7f,.9f,.65f,.8f},{.4f,.1f,.3f,.1f}};
    return random01(static_cast<std::uint32_t>(seed),static_cast<std::uint32_t>(stepIndex*101+5))
          <std::clamp(density*rates[std::clamp(style,0,4)][s%4],0.0f,1.0f);
}
inline int arpNote(int index,const std::array<int,16>& held,int heldCount,int root,bool minor,int octaves,int direction) noexcept {
    std::array<int,48> tones{};
    int count=0;
    if(heldCount>0) {
        for(int oct=0;oct<std::clamp(octaves,1,3);++oct)
            for(int j=0;j<heldCount && count<48;++j)
                if(held[j]+oct*12<=127)tones[count++]=held[j]+oct*12;
    } else {
        const int triad[]{0,minor?3:4,7};
        for(int oct=0;oct<std::clamp(octaves,1,3);++oct)
            for(int interval:triad)if(root+oct*12+interval<=127)tones[count++]=root+oct*12+interval;
    }
    if(count==0)return clampMidi(root);
    if(direction==1)return tones[count-1-(index%count)];
    if(direction==2&&count>1){
        const int period=2*count-2;
        const int pos=index%period;
        return tones[pos<count?pos:period-pos];
    }
    return tones[index%count];
}
} // namespace jerzy_composer

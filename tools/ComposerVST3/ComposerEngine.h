#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace jerzy_composer {

// All times in quarter notes; musical logic is pure C++ and requires no JUCE.
struct Settings {
    int mode = 1, tonic = 0, octave = 1, scale = 0, progression = 1, style = 0;
    int division = 1, color = 0, voicing = 2, arpOctaves = 1, direction = 0, seed = 71;
    float density = 0.72f, swing = 0.15f, human = 0.06f;
    float variation = 0.35f, gate = 0.85f;
    double barLength = 4.0;
};
struct NoteEvent {
    int pitch = 60, velocity = 100;
    double position = 0.0, duration = 0.25;
};
struct Bar {
    std::array<NoteEvent, 192> notes{};
    int count = 0;
    void add(int pitch, double position, double duration, int velocity) noexcept {
        if (count >= static_cast<int>(notes.size()) || pitch < 0 || pitch > 127
            || !std::isfinite(position) || !std::isfinite(duration)) return;
        if (position >= 0 && position < 16 && duration > 0)
            notes[static_cast<std::size_t>(count++)] = {
                pitch, std::clamp(velocity, 1, 127), position, duration
            };
    }
};
inline std::uint32_t hash(std::uint32_t x) noexcept {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU;
    return x ^ (x >> 16);
}
inline float random01(int seed, int key) noexcept {
    return static_cast<float>(hash(static_cast<std::uint32_t>(seed) ^
            hash(static_cast<std::uint32_t>(key)))) * (1.0f / 4294967296.0f);
}
inline int midiRoot(const Settings& s) noexcept {
    // C2 is MIDI 36, C3 is 48 (international octave notation).
    return std::clamp(36 + 12 * std::clamp(s.octave, 0, 4) +
                      std::clamp(s.tonic, 0, 11), 0, 127);
}
inline int degreePitch(int root, int degree, int scale) noexcept {
    static constexpr int intervals[7][7] = {
        {0,2,3,5,7,8,10}, {0,2,4,5,7,9,11}, {0,2,3,5,7,9,10},
        {0,3,5,7,10,0,0}, {0,2,3,5,7,8,11}, {0,1,3,5,7,8,10},
        {0,2,4,5,7,9,10}
    };
    scale = std::clamp(scale, 0, 6);
    const int count = scale == 3 ? 5 : 7;
    const int octave = degree >= 0 ? degree/count : -((-degree+count-1)/count);
    const int index = degree - octave*count;
    return root + 12*octave + intervals[scale][index];
}
inline int chordDegree(int progression, int bar) noexcept {
    static constexpr int progressions[7][4] = {
        {0,0,0,0}, {0,5,2,6}, {0,3,4,0}, {0,4,5,3},
        {1,4,0,5}, {0,6,5,6}, {0,2,6,3}
    };
    return progressions[std::clamp(progression,0,6)][((bar%4)+4)%4];
}
inline double stepLength(int division) noexcept {
    static constexpr double values[] = {0.5,0.25,1.0/6.0,0.125,1.0/3.0};
    return values[std::clamp(division,0,4)];
}
inline double humanTime(const Settings& s, int bar, int key, double beat, double step) noexcept {
    const int idx = static_cast<int>(std::round(beat / step));
    const double swing = (idx & 1) ? step*std::clamp(double(s.swing),0.0,0.45) : 0;
    const double jitter = (random01(s.seed, bar*1009+key*37+19)*2.-1.) *
                          step * std::clamp(double(s.human),0.0,0.25)*0.25;
    return std::clamp(beat+swing+jitter, 0.0, std::max(0.,s.barLength-0.0001));
}
inline int velocity(const Settings& s, int bar, int key, int base) noexcept {
    const double diff = (random01(s.seed, bar*2003+key*41+73)*2.-1.)
                        * std::clamp(double(s.human),0.0,0.25)*30.;
    return std::clamp(static_cast<int>(std::lround(base+diff)), 1, 127);
}
inline bool chance(const Settings& s, int bar, int key, float threshold) noexcept {
    return random01(s.seed, bar*6271+key*131+271)
           < std::clamp(threshold,0.0f,1.0f);
}
inline int chordNotes(const Settings& s, int bar, std::array<int,5>& notes) noexcept {
    const int degree = chordDegree(s.progression,bar);
    const int count = s.color == 0 || s.color == 2 ? 3 : 4;
    std::array<int,4> offsets = {{0,2,4,6}};
    if (s.color == 2) offsets = {{0, (degree%2==0?1:3), 4, 6}};
    if (s.color == 3) offsets = {{0,2,4,8}};
    const int root = midiRoot(s)+12;
    for (int i=0;i<count;++i)
        notes[static_cast<std::size_t>(i)] = degreePitch(root,degree+offsets[static_cast<std::size_t>(i)],s.scale);
    if (s.voicing == 1 && count >= 3) notes[static_cast<std::size_t>(count-1)] += 12;
    if (s.voicing == 2) {
        // Choose inversion close to the prior chord, without memory across audio blocks.
        std::array<int,5> previous{};
        const int pd = chordDegree(s.progression,bar-1);
        for (int i=0;i<count;++i)
            previous[static_cast<std::size_t>(i)] =
                degreePitch(root,pd+offsets[static_cast<std::size_t>(i)],s.scale);
        std::array<int,5> best=notes;
        double bestCost=1.e9;
        for (int shift=-1;shift<=1;++shift) {
            for (int rotation=0;rotation<count;++rotation) {
                std::array<int,5> candidate{};
                for (int i=0;i<count;++i) {
                    const int j=(i+rotation)%count;
                    candidate[static_cast<std::size_t>(i)] =
                        notes[static_cast<std::size_t>(j)] + (i+rotation>=count?12:0) +shift*12;
                }
                if (candidate[0] < 36 || candidate[static_cast<std::size_t>(count-1)] > 108) continue;
                double cost=0;
                for (int i=0;i<count;++i)
                    cost+=std::abs(candidate[static_cast<std::size_t>(i)] -
                                    previous[static_cast<std::size_t>(i)]);
                if (cost < bestCost) {bestCost=cost;best=candidate;}
            }
        }
        notes=best;
    }
    return count;
}
struct BassHit {int sixteenth; int chordOffset;};
inline void generateDrums(const Settings& s, int bar, Bar& result) noexcept {
    static constexpr int kicks[8][5] = {
        {0,8,10,-1,-1}, {0,10,-1,-1,-1}, {0,4,8,12,-1},
        {0,6,10,-1,-1}, {0,8,-1,-1,-1}, {0,4,8,12,-1},
        {0,7,10,-1,-1}, {0,8,11,-1,-1}
    };
    static constexpr int snare[8][4] = {
        {4,12,-1,-1}, {4,12,-1,-1}, {4,12,-1,-1},
        {4,12,-1,-1}, {12,-1,-1,-1}, {4,12,-1,-1},
        {4,12,-1,-1}, {4,12,-1,-1}
    };
    const int st = std::clamp(s.style,0,7);
    const double unit = s.barLength/16.;
    for (int kind=0;kind<2;++kind) {
        const int n = kind==0?5:4;
        for (int j=0;j<n;++j) {
            const int pos = kind==0?kicks[st][j]:snare[st][j];
            if(pos<0)continue;
            if(pos==0 || chance(s,bar,100+kind*16+pos,.75f+.25f*s.density)) {
                const double at=humanTime(s,bar,300+kind*17+pos,pos*unit,unit);
                result.add(kind==0?36:37,at,unit*.55,
                           velocity(s,bar,kind*17+pos,kind==0?117:109));
            }
        }
    }
    const double step = stepLength(s.division);
    const int hats = std::clamp(static_cast<int>(std::round(s.barLength/step)),1,96);
    for (int i=0;i<hats;++i) {
        if (!chance(s,bar,400+i,.22f+.78f*s.density))continue;
        const double at = humanTime(s,bar,600+i,i*s.barLength/hats,step);
        result.add(39,at,step*.42,velocity(s,bar,800+i,i%2==0?88:63));
    }
    if (st!=4 && chance(s,bar,990,s.density*.7f))
        result.add(43,humanTime(s,bar,991,s.barLength*.875,unit),unit*1.6,75);
    if (bar%4==3 && chance(s,bar,992,s.variation*.7f))
        for (int i=13;i<16;++i)
            if (chance(s,bar,995+i,s.variation*.7f))
                result.add(37,i*unit,unit*.4,48+i%3*8);
}
inline void generateBass(const Settings& s, int bar, Bar& result) noexcept {
    static const BassHit groove[8][7] = {
        {{0,0},{4,0},{8,4},{12,0},{-1,0},{-1,0},{-1,0}},
        {{0,0},{6,4},{12,0},{-1,0},{-1,0},{-1,0},{-1,0}},
        {{0,0},{2,7},{4,4},{6,0},{10,7},{12,4},{-1,0}},
        {{0,0},{3,4},{6,7},{8,0},{11,2},{14,4},{-1,0}},
        {{0,0},{8,4},{-1,0},{-1,0},{-1,0},{-1,0},{-1,0}},
        {{0,0},{4,0},{8,4},{12,0},{-1,0},{-1,0},{-1,0}},
        {{0,0},{3,0},{8,4},{11,2},{14,0},{-1,0},{-1,0}},
        {{0,0},{6,4},{12,0},{-1,0},{-1,0},{-1,0},{-1,0}}
    };
    const int st=std::clamp(s.style,0,7);
    const int degree=chordDegree(s.progression,bar);
    const double unit=s.barLength/16.;
    for(int j=0;j<7;++j) {
        const auto hit=groove[st][j];
        if(hit.sixteenth<0)break;
        if(j>0 && !chance(s,bar,1100+j,.2f+.8f*s.density))continue;
        int noteDegree=degree+hit.chordOffset;
        if(bar%2==1 && j>0 && chance(s,bar,1120+j,s.variation*.30f))
            noteDegree+=(chance(s,bar,1150+j,.5f)?1:-1);
        const int pitch=degreePitch(midiRoot(s),noteDegree,s.scale);
        const double at=humanTime(s,bar,1200+j,hit.sixteenth*unit,unit);
        double length = s.style==4 ? s.barLength*.48 : unit*3.0;
        length*=std::clamp(double(s.gate),.25,1.15);
        result.add(pitch,at,length,velocity(s,bar,1300+j,j==0?117:96));
    }
    if (bar%4!=3 && chance(s,bar,1350,s.variation*.48f)) {
        const int next=chordDegree(s.progression,bar+1);
        const int approach=next>degree?next-1:next+1;
        result.add(degreePitch(midiRoot(s),approach,s.scale),
                   humanTime(s,bar,1370,15*unit,unit),unit*.50,72);
    }
}
inline void generateChords(const Settings& s, int bar, Bar& result) noexcept {
    std::array<int,5> pitches{};
    const int count=chordNotes(s,bar,pitches);
    std::array<double,4> positions{{0,.5,-1,-1}};
    double duration = s.barLength*.43;
    if(s.style==4){ positions={{0,-1,-1,-1}};duration=s.barLength*.99; }
    else if(s.style==1||s.style==5){positions={{.125,.375,.625,.875}};duration=s.barLength*.23;}
    else if(s.style==3||s.style==6){positions={{0,.375,.625,.875}};duration=s.barLength*.19;}
    else if(s.style==7){positions={{0,.625,-1,-1}};duration=s.barLength*.32;}
    for(int i=0;i<4;++i) {
        if(positions[static_cast<std::size_t>(i)]<0)continue;
        if(i>0 && !chance(s,bar,1400+i,.25f+.75f*s.density))continue;
        const double at=humanTime(s,bar,1450+i,positions[static_cast<std::size_t>(i)]*s.barLength,
                                  s.barLength/16.);
        for(int v=0;v<count;++v)
            result.add(pitches[static_cast<std::size_t>(v)],at,
                       duration*std::clamp(double(s.gate),.25,1.15),
                       velocity(s,bar,1500+i*5+v,i==0?99:82));
    }
}
inline void generateArp(const Settings& s, int bar, Bar& result,
                        const std::array<int,16>& held, int heldCount) noexcept {
    std::array<int,48> notes{};
    int size=0;
    if(heldCount>0){
        for(int oct=0;oct<std::clamp(s.arpOctaves,1,3);++oct)
            for(int i=0;i<heldCount && size<48;++i) {
                const int p=held[static_cast<std::size_t>(i)]+oct*12;
                if(p>=0 && p<=127)notes[static_cast<std::size_t>(size++)]=p;
            }
    }else{
        std::array<int,5> chord{};
        const int count=chordNotes(s,bar,chord);
        for(int oct=0;oct<std::clamp(s.arpOctaves,1,3);++oct)
            for(int i=0;i<count && size<48;++i){
                const int p=chord[static_cast<std::size_t>(i)]+oct*12;
                if(p>=0 && p<=127)notes[static_cast<std::size_t>(size++)]=p;
            }
    }
    if(size<1)return;
    const double unit=stepLength(s.division);
    const int steps=std::clamp(static_cast<int>(std::ceil(s.barLength/unit)),1,128);
    for(int i=0;i<steps;++i) {
        if(i && !chance(s,bar,1600+i,.18f+.82f*s.density))continue;
        int index=i;
        if(s.direction==1)index=size-1-i%size;
        else if(s.direction==2 && size>1){
            const int period=size*2-2;
            const int k=i%period;
            index=k<size?k:period-k;
        }else index=i%size;
        const double pos=i*unit;
        if(pos>=s.barLength)break;
        result.add(notes[static_cast<std::size_t>(index)],
                   humanTime(s,bar,1700+i,pos,unit),
                   unit*std::clamp(double(s.gate),.25,1.15),
                   velocity(s,bar,1800+i,i%4==0?112:88));
    }
}
inline void generateMelody(const Settings& s, int bar, Bar& result) noexcept {
    static constexpr int contour[16]={0,2,4,2,1,3,4,2,0,-1,0,1,2,1,0,-1};
    const double unit=stepLength(s.division);
    const int steps=std::clamp(static_cast<int>(std::ceil(s.barLength/unit)),1,128);
    const int degree=chordDegree(s.progression,bar);
    for(int i=0;i<steps;++i) {
        if(bar%4==3 && i>steps*3/4)continue; // musical ending with a breath
        const int motifIndex=(i*16)/steps;
        const bool strong=i==0 || i==steps/2;
        const float probability=strong?1.f:.16f+s.density*.62f;
        // Same motif decisions for consecutive bars; variation alters notes, not all rhythms.
        if(!chance(s,0,1900+i,probability))continue;
        int contourDegree=contour[std::clamp(motifIndex,0,15)];
        if(bar%2 && (s.style==4||s.style==7))contourDegree=-contourDegree;
        if(bar>=2 && !strong && chance(s,bar,2000+i,s.variation*.32f))
            contourDegree+=chance(s,bar,2100+i,.5f)?1:-1;
        if(i==0 || (bar%4==3 && i>=steps/2))contourDegree=0;
        const int pitch=degreePitch(midiRoot(s)+12,degree+contourDegree,s.scale);
        const double at=humanTime(s,bar,2200+i,i*unit,unit);
        result.add(pitch,at,unit*std::clamp(double(s.gate),.25,1.15),
                   velocity(s,bar,2300+i,strong?110:86));
    }
}
inline Bar generateBar(const Settings& s, int bar, const std::array<int,16>& held={},
                       int heldCount=0) noexcept {
    Bar notes;
    switch(std::clamp(s.mode,0,4)) {
        case 0: generateDrums(s,bar,notes);break;
        case 1: generateBass(s,bar,notes);break;
        case 2: generateChords(s,bar,notes);break;
        case 3: generateArp(s,bar,notes,held,std::clamp(heldCount,0,16));break;
        default:generateMelody(s,bar,notes);break;
    }
    return notes;
}
} // namespace jerzy_composer

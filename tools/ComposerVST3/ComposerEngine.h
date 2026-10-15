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
    int noteLength = 0, rhythm = 0; // 0 = musical auto length / style-based rhythm
    float density = 0.72f, swing = 0.15f, human = 0.06f;
    float variation = 0.35f, gate = 0.85f;
    float lengthVariation = 0.65f, syncopation = 0.3f;
    float dynamics = 0.70f, accents = 0.65f, ghosts = 0.22f;
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
// Musician-readable duration selections in quarter-note units.
// Auto uses each instrument's designed rhythmic note lengths.
inline double selectedNoteLength(int index, double barLength) noexcept {
    switch(index) {
        case 1:return .125;     // 1/32
        case 2:return 1./6.;   // 1/16 triplet
        case 3:return .25;      // 1/16
        case 4:return 1./3.;   // 1/8 triplet
        case 5:return .5;       // 1/8
        case 6:return 1.;       // 1/4
        case 7:return 2.;       // 1/2
        case 8:return barLength;// one measure (meter-aware)
        case 9:return barLength*2.; // two measures
        default:return 0.;
    }
}
inline double musicalLengthFactor(int motif, int bar, int seed) noexcept {
    // Stable rhythmic vocabulary: short pickups, sustains, dotted and tied notes.
    static constexpr double values[16] = {
        1., .5, 1.5, .75, .5, 1., 2., .5,
        1.5, 1., .5, 1., .75, 2., .5, 1.};
    // Phrase-aware, seeded but bounded: avoid unmusical white-noise lengths.
    const int phase=((motif%16)+16)%16;
    const int shift=(hash(static_cast<std::uint32_t>(seed + (bar/4)*101)) % 4);
    return values[(phase + static_cast<int>(shift))%16];
}
inline void finishBar(const Settings& s,int bar,Bar& result) noexcept {
    const float strength=std::clamp(s.dynamics,0.f,1.f);
    const float accent=std::clamp(s.accents,0.f,1.f);
    const float lenMix=std::clamp(s.lengthVariation,0.f,1.f);
    const double gate=std::clamp(double(s.gate),.1,2.);
    const double explicitLen=selectedNoteLength(s.noteLength,s.barLength);
    const double q=std::max(.03125,stepLength(s.division));
    for(int i=0;i<result.count;++i) {
        auto& n=result.notes[static_cast<std::size_t>(i)];
        const int pulse=static_cast<int>(std::round(n.position/q));
        const double variation=musicalLengthFactor(pulse,bar,s.seed);
        const double base=(s.noteLength==0?n.duration:explicitLen);
        // At zero variation, fixed length obeys note-value selection precisely.
        // At max, produces an intentional phrase of varied note values.
        const double factor=1.0+(variation-1.0)*lenMix;
        n.duration=std::clamp(base*factor*gate,0.015625,s.barLength*4.);
        // Drum samples should not hang: preserve short triggering of closed hats.
        if(s.mode==0 && n.pitch==39)n.duration=std::min(n.duration,q*.92);
        // Repeated melodic pitches can be retriggered without stuck notes.
        const double beatPos = std::fmod(n.position,1.0);
        const bool downbeat=std::abs(beatPos)<.09;
        const bool offbeat=std::abs(beatPos-.5)<.12;
        int target=90;
        if(downbeat)target=101+static_cast<int>(accent*19.);
        else if(offbeat)target=78+static_cast<int>(accent*4.);
        else target=72-static_cast<int>(accent*21.);
        if(s.mode==0) {
            if(n.pitch==36)target=108+static_cast<int>(accent*14.);
            else if(n.pitch==37)target=91+static_cast<int>(accent*21.);
            else if(n.pitch==39)target=downbeat?86:59;
            else target=63;
        }
        if(n.velocity < 70) target=std::min(target,static_cast<int>(n.velocity)+15);
        const double newVel=90*(1.-strength)+
                (n.velocity*.55+target*.45)*strength;
        n.velocity=std::clamp(static_cast<int>(std::lround(newVel)),1,127);
    }
}
// Change timing only on musically meaningful weak positions.
// Rhythm modes: 0=style, 1=half-time, 2=syncopated,
// 3=triplet pulse, 4=euclidean 5/16, 5=broken bounce.
inline bool rhythmHit(const Settings& s,int bar,int step,int total,bool strong) noexcept {
    if(strong)return true;
    const auto density=std::clamp(s.density,0.f,1.f);
    int phase=(step*16)/std::max(1,total);
    phase=std::clamp(phase,0,15);
    float bias=.55f;
    if(s.rhythm==1)bias=(phase==8 || phase==12)?1.f:.28f;
    if(s.rhythm==2)bias=(phase%4==3||phase%4==2)?1.f:.35f;
    if(s.rhythm==3)bias=(step%3==0)?1.f:.50f;
    if(s.rhythm==4)bias=((phase*5)%16<5)?1.f:.12f;
    if(s.rhythm==5)bias=(phase==3||phase==6||phase==11||phase==14)?1.f:.40f;
    return chance(s,bar,3900+step,density*(.30f+.70f*bias));
}
inline double syncopatedPosition(const Settings& s,int bar,int key,double pos,double unit) noexcept {
    if(s.syncopation<=0 || pos<unit*.5)return pos;
    const bool move=chance(s,bar,4100+key,s.syncopation*.58f);
    if(!move)return pos;
    // Pull a weak event half a rhythmic step early, without crossing the bar.
    return std::clamp(pos - unit*.5,0.0,std::max(0.,s.barLength-.001));
}

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
                const double at=humanTime(s,bar,300+kind*17+pos,
                    syncopatedPosition(s,bar,300+kind*17+pos,pos*unit,unit),unit);
                result.add(kind==0?36:37,at,unit*.55,
                           velocity(s,bar,kind*17+pos,kind==0?117:109));
            }
        }
    }
    const double step = s.rhythm==3 ? s.barLength/12. : stepLength(s.division);
    const int hats = std::clamp(static_cast<int>(std::round(s.barLength/step)),1,96);
    for (int i=0;i<hats;++i) {
        if (!chance(s,bar,400+i,.22f+.78f*s.density))continue;
        if(s.rhythm!=0 && !rhythmHit(s,bar,i,hats,i==0))continue;
        const double at = humanTime(s,bar,600+i,i*s.barLength/hats,step);
        result.add(39,at,step*.42,velocity(s,bar,800+i,i%2==0?88:63));
    }
    if (st!=4 && chance(s,bar,990,s.density*.7f))
        result.add(43,humanTime(s,bar,991,s.barLength*.875,unit),unit*1.6,75);
    if (bar%4==3 && chance(s,bar,992,s.variation*.7f))
        for (int i=13;i<16;++i)
            if (chance(s,bar,995+i,s.variation*.7f))
                result.add(37,i*unit,unit*.4,48+i%3*8);
    for (int pos : {3,11}) { // quiet ghost snares before backbeats
        if(chance(s,bar,1012+pos,s.ghosts*.9f))
            result.add(37,humanTime(s,bar,1020+pos,pos*unit,unit),
                       unit*.35,37 + (pos==11?8:0));
    }
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
    if(s.rhythm!=0) {
        // Each rhythm model generates its OWN pulse train, not merely a mask
        // over a pre-existing rock riff. Quarters are expressed in PPQ beats.
        const int model=std::clamp(s.rhythm,1,5);
        const int pattern[][8] = {
            {0,8,-1,-1,-1,-1,-1,-1},   // half-time
            {0,3,7,10,14,-1,-1,-1},  // syncopated
            {0,2,4,6,8,10,-1,-1},    // triplet 12-step
            {0,4,7,10,13,-1,-1,-1},  // Euclidean 5/16
            {0,2,5,8,11,13,-1,-1}   // broken / bounce
        };
        const int total=(model==3 ? 12 : 16);
        int count=0;
        for(int k=0;k<8;++k) {
            const int index=pattern[model-1][k];
            if(index<0)break;
            if(k>0 && !chance(s,bar,2800+k,
                    std::clamp(.38f+.62f*s.density,0.f,1.f)))continue;
            const double pos=index*s.barLength/total;
            const double pulse=s.barLength/total;
            const double t=humanTime(s,bar,2900+k,
                syncopatedPosition(s,bar,2900+k,pos,pulse),pulse);
            const int noteDegree=degree + ((k%4==2)?4:((k%4==3)?2:0));
            // Natural long first note, shorter syncopated pickups.
            const double duration=(k==0 ? s.barLength*.36 : pulse*(k%2?1.3:2.1));
            result.add(degreePitch(midiRoot(s),noteDegree,s.scale),
                       t,duration,velocity(s,bar,3000+k,k==0?114:((k%2)?76:96)));
            ++count;
        }
        if(count==0)
            result.add(degreePitch(midiRoot(s),degree,s.scale),0.,unit*3.,114);
        return;
    }
    for(int j=0;j<7;++j) {
        const auto hit=groove[st][j];
        if(hit.sixteenth<0)break;
        if(j>0 && !(s.rhythm==0
            ? chance(s,bar,1100+j,.2f+.8f*s.density)
            : rhythmHit(s,bar,hit.sixteenth,16,false)))continue;
        int noteDegree=degree+hit.chordOffset;
        if(bar%2==1 && j>0 && chance(s,bar,1120+j,s.variation*.30f))
            noteDegree+=(chance(s,bar,1150+j,.5f)?1:-1);
        const int pitch=degreePitch(midiRoot(s),noteDegree,s.scale);
        const double at=humanTime(s,bar,1200+j,
            syncopatedPosition(s,bar,1200+j,hit.sixteenth*unit,unit),unit);
        double length = s.style==4 ? s.barLength*.48 : unit*3.0;
        // Gate applied exactly once in finishBar, independent of note value.
        result.add(pitch,at,length,velocity(s,bar,1300+j,j==0?117:96));
    }
    if(s.ghosts>0.01f && chance(s,bar,1341,s.ghosts*.75f)) {
        result.add(degreePitch(midiRoot(s),degree+4,s.scale),
                   humanTime(s,bar,1342,s.barLength*11./16.,unit),unit*.32,43);
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
                       duration,
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
    const double unit=s.rhythm==3 ? s.barLength/12. : stepLength(s.division);
    const int steps=std::clamp(static_cast<int>(std::ceil(s.barLength/unit)),1,128);
    for(int i=0;i<steps;++i) {
        if(i && !(s.rhythm==0
            ? chance(s,bar,1600+i,.18f+.82f*s.density)
            : rhythmHit(s,bar,i,steps,false)))continue;
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
                   humanTime(s,bar,1700+i,
                       syncopatedPosition(s,bar,1700+i,pos,unit),unit),
                   unit,
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
        if(s.rhythm!=0 && !rhythmHit(s,bar,i,steps,strong))continue;
        int contourDegree=contour[std::clamp(motifIndex,0,15)];
        if(bar%2 && (s.style==4||s.style==7))contourDegree=-contourDegree;
        if(bar>=2 && !strong && chance(s,bar,2000+i,s.variation*.32f))
            contourDegree+=chance(s,bar,2100+i,.5f)?1:-1;
        if(i==0 || (bar%4==3 && i>=steps/2))contourDegree=0;
        const int pitch=degreePitch(midiRoot(s)+12,degree+contourDegree,s.scale);
        const double at=humanTime(s,bar,2200+i,
            syncopatedPosition(s,bar,2200+i,i*unit,unit),unit);
        result.add(pitch,at,unit*std::clamp(double(s.gate),.25,1.15),
                   velocity(s,bar,2300+i,strong?110:86));
    }
    if(s.ghosts>0.01f && bar%2==1 && chance(s,bar,2401,s.ghosts*.6f)) {
        const int next=chordDegree(s.progression,bar+1);
        const double at=std::max(0.,s.barLength-stepLength(s.division)*.5);
        result.add(degreePitch(midiRoot(s)+12,next-1,s.scale),
                   at,stepLength(s.division)*.4,46);
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
    finishBar(s,bar,notes);
    return notes;
}
} // namespace jerzy_composer

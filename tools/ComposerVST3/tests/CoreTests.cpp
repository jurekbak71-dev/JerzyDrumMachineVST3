#include "ComposerEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

static void check(bool value,const char* description) {
    if(!value) {
        std::cerr << "FAIL: " << description << '\n';
        std::exit(1);
    }
}
static bool same(const jerzy_composer::Bar& a,const jerzy_composer::Bar& b) {
    if(a.count!=b.count)return false;
    for(int i=0;i<a.count;++i) {
        const auto& x=a.notes[static_cast<std::size_t>(i)];
        const auto& y=b.notes[static_cast<std::size_t>(i)];
        if(x.pitch!=y.pitch || x.position!=y.position ||
           x.duration!=y.duration || x.velocity!=y.velocity)return false;
    }
    return true;
}
int main(){
    using namespace jerzy_composer;
    Settings s;
    check(midiRoot(s)==48,"default C3");
    s.octave=0;check(midiRoot(s)==36,"C2 MIDI 36");
    s.tonic=4;check(midiRoot(s)==40,"E2 instead of raw MIDI value");
    check(degreePitch(36,-1,0)==34,"negative minor degree floor");
    check(degreePitch(36,2,0)==39,"minor third");
    check(degreePitch(36,2,1)==40,"major third");
    check(degreePitch(36,7,1)==48,"octave");
    check(chordDegree(1,0)==0 && chordDegree(1,1)==5 &&
          chordDegree(1,2)==2 && chordDegree(1,3)==6,"i-VI-III-VII");
    check(std::abs(stepLength(2)-1./6.)<1.e-10,"16th note triplets");
    check(random01(71,123)==random01(71,123),"deterministic PRNG");
    s = Settings{};
    int tested=0;
    for(int mode=0;mode<5;++mode) {
        for(int style=0;style<8;++style) {
            for(int scale=0;scale<7;++scale) {
                for(int division=0;division<5;++division) {
                    s.mode=mode;s.style=style;s.scale=scale;s.division=division;
                    const auto a=generateBar(s,0),b=generateBar(s,0);
                    check(a.count>0,"each supported mode generates notes");
                    check(same(a,b),"repeatable pattern");
                    check(a.count<=192,"bounded notes no audio thread heap");
                    for(int i=0;i<a.count;++i) {
                        const auto& n=a.notes[static_cast<std::size_t>(i)];
                        check(n.pitch>=0&&n.pitch<=127,"MIDI pitch valid");
                        check(n.position>=0&&n.position<s.barLength,"note starts inside bar");
                        check(n.duration>0 && n.duration<8.0,"sane gate");
                        check(n.velocity>=1&&n.velocity<=127,"valid velocity");
                    }
                    ++tested;
                }
            }
        }
    }
    s=Settings{};s.mode=2;s.style=4;s.progression=1;
    check(!same(generateBar(s,0),generateBar(s,1)),
          "chord progression changes voicings");
    s.mode=1;
    check(!same(generateBar(s,0),generateBar(s,1)),
          "bass follows chord progression");
    s.mode=4;s.variation=0;
    check(same(generateBar(s,0),generateBar(s,0)),
          "motif repeats with variation zero");
    s.mode=3;
    std::array<int,16> held{};held[0]=60;held[1]=64;held[2]=67;
    const auto arp=generateBar(s,0,held,3);
    check(arp.count>2,"held MIDI chord arpeggiates");
    for(int i=0;i<arp.count;++i) {
        const int pitch=arp.notes[static_cast<std::size_t>(i)].pitch;
        check(pitch==60 || pitch==64 || pitch==67,
              "live arpeggiator uses held input");
    }
    s.barLength=3.;s.mode=0;
    const auto odd=generateBar(s,3);
    check(odd.count>0,"3/4 generated");
    for(int i=0;i<odd.count;++i)
        check(odd.notes[static_cast<std::size_t>(i)].position<3.,
              "3/4 event not outside bar");
    std::cout<<"PASS: "<<tested<<" full mode/style/scale/rhythm matrices; "
             <<"musical root, progression, input arp and 3/4 checked\n";
    return 0;
}

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
    // Fixed note values must be independent of the rhythm division.
    s=Settings{};s.mode=4;s.density=1.f;s.ghosts=0.f;
    s.gate=1.f;s.lengthVariation=0.f;
    for(int choice=1;choice<=9;++choice) {
        s.noteLength=choice;
        for(int division=0;division<5;++division) {
            s.division=division;
            const auto pattern=generateBar(s,0);
            check(pattern.count>0,"fixed value mode has notes");
            const double expected=selectedNoteLength(choice,s.barLength);
            for(int i=0;i<pattern.count;++i)
                check(std::abs(pattern.notes[static_cast<std::size_t>(i)].duration-expected)<1.e-9,
                      "note-value selection independent from gate and division");
        }
    }
    s.noteLength=6;s.division=1;s.gate=.10f;
    const auto shortNotes=generateBar(s,0);
    s.gate=2.f;
    const auto legatoNotes=generateBar(s,0);
    check(shortNotes.count==legatoNotes.count,"gate does not change note rhythm");
    for(int i=0;i<shortNotes.count;++i) {
        const auto a=shortNotes.notes[static_cast<std::size_t>(i)].duration;
        const auto b=legatoNotes.notes[static_cast<std::size_t>(i)].duration;
        check(std::abs(a-.1)<1.e-5,"gate 10 percent yields 0.1 quarter");
        check(std::abs(b-2.)<1.e-5,"gate 200 percent yields 2 quarters");
    }
    s.lengthVariation=1.f;s.gate=1.f;
    const auto vari=generateBar(s,0);
    std::set<int> lengths;
    for(int i=0;i<vari.count;++i)
        lengths.insert(static_cast<int>(std::lround(vari.notes[static_cast<std::size_t>(i)].duration*1000)));
    check(lengths.size()>1,"individual notes have different duration values");
    // Accents and dynamic range modify actual MIDI velocities.
    s.lengthVariation=0.f;s.dynamics=0.f;s.accents=1.f;
    const auto flat=generateBar(s,0);
    for(int i=0;i<flat.count;++i)
        check(flat.notes[static_cast<std::size_t>(i)].velocity==90,"zero dynamics flattens velocity");
    s.dynamics=1.f;
    const auto expressive=generateBar(s,0);
    std::set<int> velocities;
    for(int i=0;i<expressive.count;++i)
        velocities.insert(expressive.notes[static_cast<std::size_t>(i)].velocity);
    check(velocities.size()>1,"dynamics produces distinct velocity accents");
    s.accents=0.f;
    check(!same(expressive,generateBar(s,0)),"accent knob audibly modifies velocity");
    // Independent rhythm models and syncopation must move notes, not be decorative.
    s=Settings{};s.mode=1;s.density=1.f;s.ghosts=0.f;s.syncopation=0.f;
    const auto straight=generateBar(s,1);
    s.syncopation=1.f;
    bool moved=false;
    for(int bar=1;bar<10;++bar) {
        s.syncopation=0.f;const auto before=generateBar(s,bar);
        s.syncopation=1.f;const auto after=generateBar(s,bar);
        if(!same(before,after))moved=true;
    }
    check(moved,"syncopation physically shifts bass timings");
    (void)straight;
    s.syncopation=0.f;s.rhythm=0;
    const auto standard=generateBar(s,1);
    s.rhythm=4;
    check(!same(standard,generateBar(s,1)),"Euclidean rhythm differs from stock bass");
    // Ghost triggering changes event count or velocity under some bars.
    s=Settings{};s.mode=0;s.ghosts=0.f;
    bool newGhost=false;
    for(int bar=0;bar<12;++bar) {
        const auto a=generateBar(s,bar);
        s.ghosts=1.f;const auto b=generateBar(s,bar);
        s.ghosts=0.f;
        if(!same(a,b))newGhost=true;
    }
    check(newGhost,"ghost notes have a measurable MIDI effect");
    // Keep deterministic while varying rhythm/duration/velocity.
    s=Settings{};s.noteLength=9;s.lengthVariation=1.f;s.gate=2.f;
    s.syncopation=1.f;s.dynamics=1.f;s.accents=1.f;s.rhythm=5;
    check(same(generateBar(s,0),generateBar(s,0)),
          "advanced expressive mode remains deterministic");

    std::cout<<"PASS: "<<tested<<" full mode/style/scale/rhythm matrices; "
             <<"musical root, progression, input arp and 3/4 checked\n";
    return 0;
}

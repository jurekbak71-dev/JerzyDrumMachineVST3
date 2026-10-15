#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

JerzyComposerProcessor::JerzyComposerProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output",juce::AudioChannelSet::stereo(),true)),
      state(*this,nullptr,"PARAMS",makeLayout()) {
    held.fill(false);
    activeUntil.fill(-1.);
}

juce::AudioProcessorValueTreeState::ParameterLayout
JerzyComposerProcessor::makeLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto choices=[&](const char* id,const char* title,StringArray names,int def) {
        p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{id,1},title,names,def));
    };
    auto integer=[&](const char* id,const char* title,int lo,int hi,int def) {
        p.push_back(std::make_unique<AudioParameterInt>(ParameterID{id,1},title,lo,hi,def));
    };
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"enabled",1},
                                                     "Włącz generator MIDI",true));
    choices("mode","Partia muzyczna",
            {"Perkusja","Bas","Akordy","Arpeggio","Melodia"},1);
    choices("tonic","Tonika - dźwięk",
            {"C","Cis / Des","D","Dis / Es","E","F","Fis / Ges",
             "G","Gis / As","A","Ais / B","H"},0);
    choices("octave","Rejestr - oktawa",{"C2 - nisko","C3","C4","C5","C6 - wysoko"},1);
    choices("scale","Skala",{"Moll","Dur","Dorycka","Pentatonika moll",
                              "Moll harmoniczna","Frygijska","Miksolidyjska"},0);
    choices("progression","Progresja akordów",
            {"Jedna tonika","i - VI - III - VII","i - iv - V - i",
             "I - V - vi - IV","ii - V - I - vi","i - VII - VI - VII",
             "i - III - VII - iv"},1);
    choices("style","Styl / groove",{"Rock","Dub / reggae","Acid / 303",
                                      "Funk","Ambient","House","Breakbeat","Lo-fi hip-hop"},0);
    choices("division","Podział rytmiczny",
            {"1/8","1/16","1/16T - triole","1/32","1/8T - triole"},1);
    choices("noteLength","Wartość nuty",
            {"Automatyczna / muzyczna","1/32","1/16T","1/16","1/8T",
             "1/8","1/4","1/2","1 takt","2 takty"},0);
    choices("rhythm","Model rytmu",
            {"Styl muzyczny","Half-time","Synkopowany","Triolowy",
             "Euklidesowy 5/16","Broken / bounce"},0);
    choices("color","Rodzaj akordu",
            {"Trójdźwięk","Septymowy","Sus2 / Sus4","Add9"},0);
    choices("voicing","Rozłożenie akordów",
            {"Blisko","Szeroko","Płynne przejścia"},2);
    choices("direction","Kierunek arpeggio",{"W górę","W dół","Góra / dół"},0);
    integer("octaves","Oktawy arpeggio",1,3,1);
    integer("density","Gęstość nut (%)",0,100,72);
    integer("swing","Swing (%)",0,45,15);
    integer("human","Naturalność (%)",0,25,6);
    integer("variation","Wariacje frazy (%)",0,100,35);
    integer("gate","Gate / artykulacja (%)",10,200,85);
    integer("lengthVariation","Zmienność długości (%)",0,100,65);
    integer("syncopation","Synkopy / przesunięcia (%)",0,100,30);
    integer("dynamics","Dynamika velocity (%)",0,100,70);
    integer("accents","Akcenty metryczne (%)",0,100,65);
    integer("ghosts","Ghost notes (%)",0,100,22);
    integer("seed","Wariant / seed",0,1000000,71);
    integer("channel","Kanał MIDI",1,16,1);
    return {p.begin(),p.end()};
}

jerzy_composer::Settings JerzyComposerProcessor::settings(double barLength) const noexcept {
    jerzy_composer::Settings s;
    s.mode=(int)get("mode"); s.tonic=(int)get("tonic"); s.octave=(int)get("octave");
    s.scale=(int)get("scale"); s.progression=(int)get("progression");
    s.style=(int)get("style"); s.division=(int)get("division");
    s.noteLength=(int)get("noteLength"); s.rhythm=(int)get("rhythm");
    s.color=(int)get("color"); s.voicing=(int)get("voicing");
    s.direction=(int)get("direction"); s.arpOctaves=(int)get("octaves");
    s.density=get("density")/100.f; s.swing=get("swing")/100.f;
    s.human=get("human")/100.f; s.variation=get("variation")/100.f;
    s.gate=get("gate")/100.f; s.seed=(int)get("seed");
    s.lengthVariation=get("lengthVariation")/100.f;
    s.syncopation=get("syncopation")/100.f;
    s.dynamics=get("dynamics")/100.f;
    s.accents=get("accents")/100.f;
    s.ghosts=get("ghosts")/100.f;
    s.barLength=barLength;
    return s;
}

void JerzyComposerProcessor::prepareToPlay(double sampleRate,int) {
    rate=juce::jmax(1000.,sampleRate);
    localPpq=0.;
    lastEndPpq=std::numeric_limits<double>::quiet_NaN();
    lastChannel=1;
    wasPlaying=false;
    held.fill(false);
    activeUntil.fill(-1.);
    output.ensureSize(131072);
}

void JerzyComposerProcessor::stopNotes(juce::MidiBuffer& target,int offset) noexcept {
    for(int note=0;note<128;++note) {
        if(activeUntil[static_cast<std::size_t>(note)]<0)continue;
        target.addEvent(juce::MidiMessage::noteOff(lastChannel,note),offset);
        activeUntil[static_cast<std::size_t>(note)]=-1.;
    }
}

void JerzyComposerProcessor::fire(const jerzy_composer::NoteEvent& ev,int channel,
                                  double absoluteBeat,double startPpq,double endPpq,
                                  double samplesPerQuarter,int samples) {
    if(ev.pitch<0 || ev.pitch>127 || absoluteBeat<startPpq || absoluteBeat>=endPpq)
        return;
    const auto index=static_cast<std::size_t>(ev.pitch);
    const int offset=juce::jlimit(0,samples-1,
        juce::roundToInt((absoluteBeat-startPpq)*samplesPerQuarter));
    if(activeUntil[index]>=0.)
        output.addEvent(juce::MidiMessage::noteOff(channel,ev.pitch),offset);
    output.addEvent(juce::MidiMessage::noteOn(
        channel,ev.pitch,(juce::uint8)juce::jlimit(1,127,ev.velocity)),offset);
    activeUntil[index]=absoluteBeat+ev.duration;
    if(activeUntil[index]<endPpq) {
        const int off=juce::jlimit(offset,samples-1,
            juce::roundToInt((activeUntil[index]-startPpq)*samplesPerQuarter));
        output.addEvent(juce::MidiMessage::noteOff(channel,ev.pitch),off);
        activeUntil[index]=-1.;
    }
}

void JerzyComposerProcessor::processBlock(juce::AudioBuffer<float>& audio,
                                          juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int samples=audio.getNumSamples();
    audio.clear(); // This plugin transmits MIDI only. It is not an audio instrument.
    output.clear();
    if(samples<=0){midi.clear();return;}
    for(const auto metadata:midi) {
        const auto& m=metadata.getMessage();
        if(m.isNoteOn())held[static_cast<std::size_t>(m.getNoteNumber())]=true;
        if(m.isNoteOff())held[static_cast<std::size_t>(m.getNoteNumber())]=false;
        if(m.isAllNotesOff()||m.isAllSoundOff())held.fill(false);
    }
    double bpm=120.,start=localPpq,barLength=4.;
    bool playing=true,hasPosition=false;
    if(auto* playhead=getPlayHead()) {
        if(auto pos=playhead->getPosition()) {
            playing=pos->getIsPlaying();
            if(auto b=pos->getBpm())bpm=*b;
            if(auto time=pos->getPpqPosition()) {
                start=*time;
                hasPosition=true;
            }
            if(auto sig=pos->getTimeSignature()) {
                if(sig->numerator>0 && sig->denominator>0)
                    barLength=4.*sig->numerator/sig->denominator;
            }
        }
    }
    bpm=std::isfinite(bpm)?juce::jlimit(20.,400.,bpm):120.;
    if(!std::isfinite(start))start=0.;
    barLength=juce::jlimit(0.25,16.,barLength);
    const double samplesPerQuarter=rate*60./bpm;
    const double end=start+samples/samplesPerQuarter;
    const int channel=juce::jlimit(1,16,(int)get("channel"));

    // Flush on channel changes, transport discontinuities and stop to avoid stuck notes.
    if(channel!=lastChannel)stopNotes(output);
    if(!playing || get("enabled")<.5f) {
        stopNotes(output);
        midi.swapWith(output);
        wasPlaying=false;
        lastChannel=channel;
        lastEndPpq=std::numeric_limits<double>::quiet_NaN();
        localPpq=hasPosition?end:localPpq;
        return;
    }
    if(wasPlaying && std::isfinite(lastEndPpq) &&
       std::abs(start-lastEndPpq)>.02)
        stopNotes(output);
    lastChannel=channel;
    wasPlaying=true;

    for(int pitch=0;pitch<128;++pitch) {
        const double off=activeUntil[static_cast<std::size_t>(pitch)];
        if(off>=0 && off<end) {
            const int at=juce::jlimit(0,samples-1,
                     juce::roundToInt((off-start)*samplesPerQuarter));
            output.addEvent(juce::MidiMessage::noteOff(channel,pitch),at);
            activeUntil[static_cast<std::size_t>(pitch)]=-1.;
        }
    }

    std::array<int,16> heldKeys{};
    int heldCount=0;
    for(int key=0;key<128 && heldCount<16;++key)
        if(held[static_cast<std::size_t>(key)])
            heldKeys[static_cast<std::size_t>(heldCount++)]=key;

    const auto s=settings(barLength);
    const int firstBar=juce::jmax(0,(int)std::floor(start/barLength));
    const int lastBar=juce::jmax(firstBar,(int)std::floor((end-1.e-10)/barLength));
    for(int bar=firstBar;bar<=lastBar && bar<firstBar+12;++bar) {
        const auto events=jerzy_composer::generateBar(s,bar,heldKeys,heldCount);
        const double barBegin=bar*barLength;
        for(int i=0;i<events.count;++i) {
            const auto& ev=events.notes[static_cast<std::size_t>(i)];
            fire(ev,channel,barBegin+ev.position,start,end,samplesPerQuarter,samples);
        }
    }
    localPpq=end;
    lastEndPpq=end;
    midi.swapWith(output);
}

juce::AudioProcessorEditor* JerzyComposerProcessor::createEditor() {
    return new JerzyComposerEditor(*this);
}
void JerzyComposerProcessor::getStateInformation(juce::MemoryBlock& data) {
    if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,data);
}
void JerzyComposerProcessor::setStateInformation(const void* data,int size) {
    if(auto xml=getXmlFromBinary(data,size))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new JerzyComposerProcessor();
}

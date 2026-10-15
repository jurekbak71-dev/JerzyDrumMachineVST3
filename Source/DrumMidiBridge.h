#pragma once
#include "DrumEngine.h"

// SMF MIDI interchange is intentionally separate from the DSP engine and kit
// format. The proprietary .jdmkit file remains the only lossless source for
// ratchets, fills, per-step sound automation and the song chain.
namespace JerzyDrumMidi {
constexpr int kPPQ=960;
inline juce::MidiMessageSequence sequenceForVoice(const Pattern& p,int v) {
    juce::MidiMessageSequence seq;
    const bool triplet=p.trackTriplet[(size_t)v];
    const int ticks=triplet?160:240;
    const int channel=v==11?2:10;
    const int noteDefault=36+v;
    const int length=juce::jlimit(1,64,p.trackLength[(size_t)v]);
    for(int step=0;step<length;++step){
        const auto& s=p.step[(size_t)v][(size_t)step];
        if(!s.on)continue;
        const int note=v==11?juce::jlimit(0,127,s.note):noteDefault;
        const int vel=juce::jlimit(1,127,juce::roundToInt(s.velocity*(s.accent?1.13f:1.0f)*127));
        const double start=step*ticks;
        auto on=juce::MidiMessage::noteOn(channel,note,(juce::uint8)vel);
        on.setTimeStamp(start);
        auto off=juce::MidiMessage::noteOff(channel,note);
        off.setTimeStamp(start+ticks/2);
        seq.addEvent(on);
        seq.addEvent(off);
    }
    seq.sort();
    return seq;
}
inline bool exportPattern(DrumEngine& engine,int index,const juce::File& target){
    if(index<0||index>=DrumEngine::patterns||target.getFullPathName().isEmpty())return false;
    const auto& p=engine.pattern(index);
    juce::MidiFile file;file.setTicksPerQuarterNote(kPPQ);
    for(int v=0;v<DrumEngine::voices;++v)file.addTrack(sequenceForVoice(p,v));
    juce::TemporaryFile tmp(target);
    {
        juce::FileOutputStream stream(tmp.getFile());
        if(!stream.openedOk()||!file.writeTo(stream))return false;
        stream.flush();
    }
    return tmp.overwriteTargetFileWithTemporary();
}
inline bool importPattern(DrumEngine& engine,int index,const juce::File& source){
    if(index<0||index>=DrumEngine::patterns)return false;
    juce::FileInputStream input(source);
    juce::MidiFile midi;
    if(!input.openedOk()||!midi.readFrom(input)||midi.getTimeFormat()<=0)return false;
    auto p=engine.pattern(index); // don't destroy the existing pattern on failure
    bool any=false;
    // Recover independent triplets when source MIDI contains off-grid notes.
    // Fully coincident positions (downbeats) retain the user's current choice.
    std::array<double,DrumEngine::voices> errorStraight{},errorTriplet{};
    for(int trackIndex=0;trackIndex<midi.getNumTracks();++trackIndex){
        auto* track=midi.getTrack(trackIndex);if(!track)continue;
        for(int i=0;i<track->getNumEvents();++i){
            const auto& m=track->getEventPointer(i)->message;
            if(!m.isNoteOn())continue;
            const int voice=m.getChannel()==2?11:m.getNoteNumber()-36;
            if(voice<0||voice>=DrumEngine::voices)continue;
            const double tick=m.getTimeStamp()*kPPQ/midi.getTimeFormat();
            errorStraight[(size_t)voice]+=std::abs(tick-std::round(tick/240.0)*240.0);
            errorTriplet[(size_t)voice]+=std::abs(tick-std::round(tick/160.0)*160.0);
        }
    }
    for(int v=0;v<DrumEngine::voices;++v){
        if(errorTriplet[(size_t)v]+4.0<errorStraight[(size_t)v])
            p.trackTriplet[(size_t)v]=true;
        else if(errorStraight[(size_t)v]+4.0<errorTriplet[(size_t)v])
            p.trackTriplet[(size_t)v]=false;
    }
    std::array<int,DrumEngine::voices> greatestStep{};
    for(int trackIndex=0;trackIndex<midi.getNumTracks();++trackIndex){
        auto* track=midi.getTrack(trackIndex);
        if(!track)continue;
        for(int i=0;i<track->getNumEvents();++i){
            const auto m=track->getEventPointer(i)->message;
            if(!m.isNoteOn())continue;
            const int voice=m.getChannel()==2?11:m.getNoteNumber()-36;
            if(voice<0||voice>=DrumEngine::voices)continue;
            const double tick=m.getTimeStamp()*kPPQ/midi.getTimeFormat();
            // Prefer the subdivision that best matches the source note timing.
            const bool triplet=p.trackTriplet[(size_t)voice];
            const int stride=triplet?160:240;
            const int step=juce::roundToInt(tick/stride);
            if(step<0||step>=64)continue;
            auto& dest=p.step[(size_t)voice][(size_t)step];
            dest.on=true;dest.velocity=juce::jlimit(.01f,1.f,m.getFloatVelocity());
            dest.accent=dest.velocity>.95f;
            if(voice==11)dest.note=m.getNoteNumber();
            greatestStep[(size_t)voice]=juce::jmax(greatestStep[(size_t)voice],step+1);
            any=true;
        }
    }
    if(!any)return false;
    for(int v=0;v<DrumEngine::voices;++v)
        if(greatestStep[(size_t)v]>0)p.trackLength[(size_t)v]=juce::jlimit(1,64,greatestStep[(size_t)v]);
    engine.pattern(index)=p;
    return true;
}
} // namespace JerzyDrumMidi

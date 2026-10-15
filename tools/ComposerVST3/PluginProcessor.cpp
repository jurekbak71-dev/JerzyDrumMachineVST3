#include "PluginProcessor.h"
#include <cmath>
#include <vector>

JerzyComposerProcessor::JerzyComposerProcessor():
    juce::AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"PARAMS",makeLayout()) {
    held.fill(false);
    activeUntil.fill(-1.0);
}
juce::AudioProcessorValueTreeState::ParameterLayout JerzyComposerProcessor::makeLayout(){
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"enabled",1},"Generate MIDI",true));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"mode",1},"Mode",StringArray{"Drums","Bass","Chords","Arp","Generative"},0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"division",1},"Note division",StringArray{"1/8","1/16","1/16 triplet","1/32","1/8 triplet"},1));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"root",1},"Root MIDI note",24,84,36));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"scale",1},"Scale",StringArray{"Major","Minor"},1));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"style",1},"Bass style",0,4,0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"density",1},"Density percent",0,100,75));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"swing",1},"Swing percent",0,49,0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"gate",1},"Note gate percent",10,95,70));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"seed",1},"Variation seed",0,1000000,71));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"octaves",1},"Arp octaves",1,3,1));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"direction",1},"Arp direction",StringArray{"Up","Down","Up-Down"},0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"channel",1},"MIDI channel",1,16,1));
    return {p.begin(),p.end()};
}
void JerzyComposerProcessor::prepareToPlay(double sampleRate,int) {
    rate=juce::jmax(1000.0,sampleRate);lastEndPpq=std::numeric_limits<double>::quiet_NaN();
    localPpq=0.0;wasPlaying=false;held.fill(false);activeUntil.fill(-1.0);
    output.ensureSize(32768);
}
void JerzyComposerProcessor::stopNotes(int channel,juce::MidiBuffer& target,int offset) noexcept {
    for(int note=0;note<128;++note)if(activeUntil[(size_t)note]>=0) {
        target.addEvent(juce::MidiMessage::noteOff(channel,note),offset);
        activeUntil[(size_t)note]=-1.0;
    }
}
void JerzyComposerProcessor::fire(int note,int velocity,int channel,double atPpq,double lengthPpq,
                                 double startPpq,double endPpq,double samplesPerQuarter,int numSamples) {
    if(note<0||note>127)return;
    auto offset=juce::jlimit(0,numSamples-1,juce::roundToInt((atPpq-startPpq)*samplesPerQuarter));
    if(activeUntil[(size_t)note]>=0.0){
        output.addEvent(juce::MidiMessage::noteOff(channel,note),offset);
    }
    output.addEvent(juce::MidiMessage::noteOn(channel,note,(juce::uint8)juce::jlimit(1,127,velocity)),offset);
    activeUntil[(size_t)note]=atPpq+lengthPpq;
    if(activeUntil[(size_t)note]<endPpq) {
        const auto off=juce::jlimit(0,numSamples-1,juce::roundToInt((activeUntil[(size_t)note]-startPpq)*samplesPerQuarter));
        output.addEvent(juce::MidiMessage::noteOff(channel,note),off);
        activeUntil[(size_t)note]=-1.0;
    }
}
void JerzyComposerProcessor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
    juce::ScopedNoDenormals noDenormals;
    const int count=audio.getNumSamples();
    audio.clear();
    output.clear();
    if(count<=0){midi.clear();return;}
    for(const auto meta:midi){
        const auto& m=meta.getMessage();
        if(m.isNoteOn())held[(size_t)m.getNoteNumber()]=true;
        if(m.isNoteOff())held[(size_t)m.getNoteNumber()]=false;
        if(m.isAllNotesOff()||m.isAllSoundOff())held.fill(false);
    }
    double bpm=120.0, start=localPpq;
    bool playing=true,haveHost=false;
    if(auto* ph=getPlayHead()) if(auto pos=ph->getPosition()){
        haveHost=true;playing=pos->getIsPlaying();
        if(auto b=pos->getBpm())bpm=*b;
        if(auto p=pos->getPpqPosition())start=*p;
    }
    bpm=std::isfinite(bpm)?juce::jlimit(20.0,400.0,bpm):120.0;
    const double samplesPerQuarter=rate*60.0/bpm;
    const double end=start+static_cast<double>(count)/samplesPerQuarter;
    const int channel=juce::jlimit(1,16,(int)get("channel"));
    if(!playing||get("enabled")<0.5f) {
        stopNotes(channel,output);
        midi.swapWith(output);
        wasPlaying=false;lastEndPpq=std::numeric_limits<double>::quiet_NaN();
        return;
    }
    // Seek, loop and transport restart: flush any pending notes, avoid stuck MIDI.
    if(wasPlaying&&std::isfinite(lastEndPpq)&&std::abs(start-lastEndPpq)>0.02)
        stopNotes(channel,output);
    wasPlaying=true;
    for(int note=0;note<128;++note){
        const double off=activeUntil[(size_t)note];
        if(off>=0&&off<end){
            auto offset=juce::jlimit(0,count-1,juce::roundToInt((off-start)*samplesPerQuarter));
            output.addEvent(juce::MidiMessage::noteOff(channel,note),offset);
            activeUntil[(size_t)note]=-1.0;
        }
    }
    std::array<int,16> chord{};int chordCount=0;
    for(int key=0;key<128&&chordCount<16;++key)if(held[(size_t)key])chord[(size_t)chordCount++]=key;
    const int mode=juce::jlimit(0,4,(int)get("mode"));
    const int division=juce::jlimit(0,4,(int)get("division"));
    const double divisions[]={0.5,0.25,1.0/6.0,0.125,1.0/3.0};
    const double step=divisions[division];
    const double swing=get("swing")/100.0;
    const double gate=get("gate")/100.0;
    const float density=get("density")/100.0f;
    const int root=(int)get("root"),seed=(int)get("seed"),style=(int)get("style");
    const bool minor=(int)get("scale")==1;
    const int notesPerBar=juce::jmax(1,juce::roundToInt(4.0/step));
    const int first=juce::jmax(0,(int)std::floor(start/step)-1);
    const int last=juce::jmax(first,(int)std::floor(end/step)+1);
    for(int index=first;index<=last && index<first+256;++index) {
        const double at=index*step+jerzy_composer::swingOffset(index,step,swing);
        if(at+1e-10<start||at>=end)continue;
        const double duration=step*gate;
        if(mode==0) {
            constexpr int notes[]={36,37,39,43};
            for(int voice=0;voice<4;++voice)
                if(jerzy_composer::hit(index,voice,density,seed))
                    fire(notes[voice],voice==0?115:voice==1?110:78,channel,at,duration,start,end,samplesPerQuarter,count);
        }else if(mode==1) {
            if(jerzy_composer::bassGate(index,style,density,seed))
                fire(jerzy_composer::scaleNote(root,jerzy_composer::baseBassDegree(index,style),minor),
                    index%4==0?111:87,channel,at,duration,start,end,samplesPerQuarter,count);
        }else if(mode==2){
            if(index%notesPerBar==0){
                for(int degree: {0,2,4})
                    fire(jerzy_composer::scaleNote(root,degree,minor),83,channel,at,4.0*gate,start,end,samplesPerQuarter,count);
            }
        }else if(mode==3){
            const int note=jerzy_composer::arpNote(index,chord,chordCount,root,minor,
                        (int)get("octaves"),(int)get("direction"));
            fire(note,index%4==0?110:89,channel,at,duration,start,end,samplesPerQuarter,count);
        }else{
            if(jerzy_composer::random01((std::uint32_t)seed,(std::uint32_t)(index*31+7))<density) {
                const int degree=(int)(jerzy_composer::random01((std::uint32_t)seed,(std::uint32_t)(index*53+13))*7);
                fire(jerzy_composer::scaleNote(root,degree,minor),88,channel,at,duration,start,end,samplesPerQuarter,count);
            }
        }
    }
    localPpq=haveHost?end:end;
    lastEndPpq=end;
    midi.swapWith(output);
}
juce::AudioProcessorEditor* JerzyComposerProcessor::createEditor() {
    auto* e=new juce::GenericAudioProcessorEditor(*this);
    e->setResizable(true,true);
    e->setResizeLimits(420,410,960,1100);
    return e;
}
void JerzyComposerProcessor::getStateInformation(juce::MemoryBlock& out){
    if(auto xml=state.copyState().createXml())copyXmlToBinary(*xml,out);
}
void JerzyComposerProcessor::setStateInformation(const void* data,int count){
    if(auto xml=getXmlFromBinary(data,count))state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new JerzyComposerProcessor();}

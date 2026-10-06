#include <JuceHeader.h>
#include "DrumEngine.h"
#include <iostream>
int main(){
 DrumEngine e;e.prepare(48000.0);e.setHost(120.0,true);
 auto &p=e.pattern(0);p.length=16;for(int v=0;v<12;v++)for(int s=0;s<64;s++)p.step[v][s]=PatternStep{};
 p.step[0][0].on=true;p.step[0][0].velocity=1.0f;e.setPattern(0);
 juce::AudioBuffer<float> b(2,24000);e.process(b);
 double energy=0.0;for(int ch=0;ch<2;ch++)for(int i=0;i<b.getNumSamples();i++)energy+=std::abs(b.getSample(ch,i));
 if(!(energy>1.0)){std::cerr<<"FAIL: engine produced silence\n";return 2;}
 e.gain[0]=0.42f;e.panorama[0]=-0.25f;auto state=e.saveState();
 DrumEngine copy;copy.prepare(48000.0);copy.loadState(state);
 if(std::abs(copy.gain[0]-0.42f)>.001f||std::abs(copy.panorama[0]+0.25f)>.001f){std::cerr<<"FAIL: state restore mismatch\n";return 3;}
 std::cout<<"PASS energy="<<energy<<" state=ok\n";return 0;
}
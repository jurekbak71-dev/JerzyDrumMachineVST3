#include "ComposerEngine.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace jerzy_composer;
 assert(scaleNote(36,0,true)==36);
 assert(scaleNote(36,2,true)==39);
 assert(scaleNote(36,2,false)==40);
 assert(scaleNote(36,7,false)==48);
 assert(hit(0,0,0.0f,71));
 assert(hit(4,1,0.0f,71));
 assert(!hit(3,0,1.0f,71));
 assert(swingOffset(1,0.25,0.2)==0.05);
 assert(swingOffset(0,0.25,0.2)==0.0);
 std::array<int,16> held{};
 held[0]=60;held[1]=64;held[2]=67;
 assert(arpNote(0,held,3,36,false,1,0)==60);
 assert(arpNote(1,held,3,36,false,1,0)==64);
 assert(arpNote(1,held,3,36,false,1,1)==64);
 assert(arpNote(3,held,3,36,false,1,2)==64);
 assert(baseBassDegree(3,0)==4);
 assert(random01(71,123)==random01(71,123));
 std::cout<<"PASS Composer deterministic groove, bass and arp core\n";
}

#include <initializer_list>
#include "../hook/ip-addresses.h"
#include "../hook/match-ui.h"
#include "../hook/native-catalog.h"
#include "../hook/lan-ports.h"
#include <assert.h>
#include <stdio.h>
int main(){
    char ip[128]="";
    for(const char* candidate:{"127.0.0.1","0.0.0.0","169.254.2.1","100.75.133.117","192.168.31.151","192.168.31.151"})LanIP::consider(ip,sizeof(ip),candidate);
    assert(!strcmp(ip,"192.168.31.151"));
    for(const char* invalid:{"","1.2.3","1.2.3.4.5","999.0.0.1","1..2.3","-1.2.3.4","1.2.3.a","0000.1.2.3"})assert(!LanIP::rank(invalid));
    char fallback[128]="";LanIP::consider(fallback,sizeof(fallback),"100.75.133.117");assert(!strcmp(fallback,"100.75.133.117"));
    assert(LanIP::rank("172.16.0.1")>LanIP::rank("100.75.133.117"));
    assert(MatchUI::menuAt(965,8) && MatchUI::menuAt(1054,40));
    assert(!MatchUI::menuAt(964,8) && !MatchUI::menuAt(1055,40) && !MatchUI::menuAt(965,41));
    for(int row=0;row<5;++row)for(int col=0;col<9;++col){
        int x=40+80*col,y=80+100*row;
        assert(MatchUI::inside(x,y,x,y,80,100));assert(MatchUI::inside(x+79,y+99,x,y,80,100));
        assert(!MatchUI::inside(x+80,y,x,y,80,100));assert(!MatchUI::inside(x,y+100,x,y,80,100));
    }
    for(unsigned seed:{0u,1u,9u,10u,1000000u,0xfffffffeu,0xffffffffu}){unsigned seen=0;
        for(unsigned attempt=0;attempt<LanPorts::Count;++attempt){unsigned port=LanPorts::candidate(seed,attempt);assert(port>=35900 && port<=35909);seen|=1u<<(port-35900);}
        assert(seen==1023);
    }
    NativeCatalog::Deck plants,zombies;
    assert(!plants.set(0,0,7,0) && !plants.set(0,6,6,0));
    for(unsigned i=0;i<6;++i)assert(plants.set(0,i,6,(int)i));
    assert(plants.complete() && plants.find(5)==5 && !plants.set(0,0,6,7));
    assert(zombies.set(1,0,8,60) && !zombies.set(1,1,8,60));
    assert(zombies.set(1,1,8,63) && zombies.set(1,2,8,69) && !zombies.complete());
    for(unsigned i=3;i<8;++i)assert(zombies.set(1,i,8,NativeCatalog::ZombieCards[i+5]));
    assert(zombies.complete());
    assert(!zombies.set(1,0,8,0) && !NativeCatalog::plant(48));
    for(int i=0;i<NativeCatalog::ZombieCount;++i)assert(NativeCatalog::zombieType(NativeCatalog::chooserCard(1,i))>=0);
    puts("LAN ports/address preference, native loadouts and HUD/tile boundaries OK.");
}

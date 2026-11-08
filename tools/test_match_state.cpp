#include "../hook/match-state.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
static void check(bool ok,const char* why){if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
int main(){
    unsigned char wire[Match::MaxStateBytes];Match::StateData s={},copy={};s.match=0x11223344;s.tick=125;s.sun=300;s.brains=200;
    s.plants=2;s.plant[0]={65536,1,2,300,200000,180000,1,0};s.plant[1]={65537,2,2,90,200000,280000,1,0};
    s.zombies=1;s.zombie[0]={65536,1,0,230,-12000,178000,0,1};
    unsigned n=Match::encode(s,wire,sizeof(wire));check(n==Match::HeaderBytes+3*Match::EntityBytes,"fixed wire size");
    check(wire[0]==0x11 && wire[1]==0x22 && wire[2]==0x33 && wire[3]==0x44,"network byte order");
    check(Match::decode(wire,n,copy) && copy.zombie[0].x==-12000 && copy.plant[1].hp==90,"signed position and health survive serialization");
    for(unsigned cut=0;cut<n;++cut)check(!Match::decode(wire,cut,copy),"truncated state rejected");
    check(!Match::decode(wire,n+1,copy),"trailing state data rejected");
    Match::put(wire+8,0xffffffff);check(!Match::decode(wire,n,copy),"overflowing count rejected");
    check(Match::encode(s,wire,16)==0,"short destination rejected");
    s.plant[1].id=s.plant[0].id;check(!Match::valid(s),"duplicate entity id rejected");s.plant[1].id=65537;
    s.plant[1].row=s.plant[0].row;check(!Match::valid(s),"overlapping plants rejected");s.plant[1].row=2;
    s.plant[1].row=5;check(!Match::valid(s),"bad lane rejected");s.plant[1].row=2;
    s.zombie[0].x=3000000;check(!Match::valid(s),"bad coordinates rejected");s.zombie[0].x=100000;
    s.sun=-1;check(!Match::valid(s),"negative resources rejected");s.sun=100;
    int row,col;check(Match::parseAction(Match::action(Match::Plant,4,8),0,row,col) && row==4 && col==8,"plant action decoded");
    check(!Match::parseAction(Match::action(Match::Plant,4,8),1,row,col),"zombie cannot plant");
    check(!Match::parseAction(Match::action(Match::Zombie,4,8),0,row,col),"plant player cannot spawn zombie");
    check(Match::parseAction(Match::action(Match::Zombie,4,8),1,row,col),"zombie action decoded");
    check(Match::parseAction(Match::action(Match::Zombie,4,3),1,row,col) && col==3,"zombie deployment retains chosen road column");
    check(!Match::parseAction(0xff000001,0,row,col),"reserved command bits rejected");
    s.coins=1;s.coin[0]={65536,1000,123,0,300000,200000,400,4,4,-500,1500};
    n=Match::encode(s,wire,sizeof(wire));check(n && Match::decode(wire,n,copy) && copy.coin[0].armor==-500 && copy.coin[0].row==1000,"sun motion/scale/type survive serialization");
    s.coin[0].unit=1;check(!Match::valid(s),"money cannot be replicated as sun");s.coin[0].unit=4;
    s.coin[0].row=2001;check(!Match::valid(s),"sun scale bounded");s.coin[0].row=1000;
    check(Match::parseAction(Match::action(Match::Plant,1,2,5),0,row,col),"native snowpea action accepted");
    check(!Match::parseAction(Match::action(Match::Plant,1,2,48),0,row,col),"unsupported imitater rejected");
    check(Match::parseAction(Match::action(Match::Zombie,1,8,69),1,row,col),"native door zombie card accepted");
    check(NativeCatalog::ZombieCount==23 && NativeCatalog::deckSize(1)==8,"23 normal zombie types and eight slots");
    for(int type=0;type<=32;++type){bool excluded=type==10 || type==11 || type==14 || (type>=25 && type<=31);
        check(NativeCatalog::zombie(type)==!excluded,"pool, boss and hybrids excluded; all other types included");
    }
    NativeCatalog::Deck deck;
    for(unsigned i=0;i<8;++i)check(deck.set(1,i,8,NativeCatalog::ZombieCards[i]),"eight distinct selected zombies");
    check(deck.complete() && deck.find(105)==5,"expanded deck complete and virtual card searchable");
    check(!deck.set(1,8,8,60) && !deck.set(1,0,3,60),"slot overflow and old deck size rejected");
    s.zombie[0].height=4;s.zombie[0].altitude=3000000;s.zombie[0].verticalSpeed=-1200;s.zombie[0].phaseTicks=237;
    s.zombie[0].flags=2;s.zombie[0].hp=0;s.zombie[0].phase=1;
    for(unsigned i=0;i<5;++i)s.zombie[0].links[i]=0x11223344+i;
    n=Match::encode(s,wire,sizeof(wire));check(n && Match::decode(wire,n,copy),"confirmed death encoded");
    check(copy.zombie[0].flags==2 && copy.zombie[0].hp==0 && copy.zombie[0].phase==1,"death flag and phase survive repair");
    check(copy.zombie[0].height==4 && copy.zombie[0].altitude==3000000 && copy.zombie[0].verticalSpeed==-1200 && copy.zombie[0].phaseTicks==237,"airborne native state survives wire");
    for(unsigned i=0;i<5;++i)check(copy.zombie[0].links[i]==0x11223344+i,"native parent/follower host IDs survive wire");
    puts("Match codec OK: wire order, negative positions, health, truncation, counts, bounds, duplicates, roles and command restrictions.");
}

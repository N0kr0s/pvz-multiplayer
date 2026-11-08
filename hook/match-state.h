#pragma once
// Stable, bounded wire state. Coordinates use 1/1000 of a native lawn pixel.
#include "native-catalog.h"
namespace Match {
constexpr unsigned Protocol=13,MaxPlants=45,MaxZombies=48,MaxShots=96,MaxCoins=64,HeaderBytes=36,EntityBytes=80,MaxStateBytes=HeaderBytes+EntityBytes*(MaxPlants+MaxZombies+MaxShots+MaxCoins);
enum Event { Start=10,Ready=11,Go=12,Command=13,Stop=14,State=15,Finish=16,Pick=17,Spawn=18,Collect=19,Collected=20,Intro=21,IntroDone=22,Death=23,Mower=24,Home=25,RoadSpawn=26,RoadPosition=27,RoadDone=28 };
enum Action { Plant=1,Zombie=2 };
struct Entity {unsigned id;int row,col,hp,x,y,phase,flags,unit=0,armor=0,shield=0;unsigned links[5]={};int height=0,altitude=0,verticalSpeed=0,phaseTicks=0;};
struct StateData {
    unsigned match,tick,plants,zombies,shots;
    int sun,brains,winner;
    unsigned coins;
    Entity plant[MaxPlants],zombie[MaxZombies],shot[MaxShots],coin[MaxCoins];
};
inline void put(unsigned char* p,unsigned v){p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;}
inline unsigned get(const unsigned char* p){return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3];}
inline bool validEntities(const Entity* e,unsigned n,int kind){
    for(unsigned i=0;i<n;++i){
        if(!e[i].id || e[i].row<0 || e[i].row>=(kind==3?2001:5) || e[i].col<0 || e[i].col>=(kind==3?2000000000:9) || e[i].hp<0 || e[i].hp>20000 ||
           e[i].x< -300000 || e[i].x>2000000 || e[i].y< -300000 || e[i].y>1200000 || e[i].phase<0 || e[i].phase>(kind==3?2000000000:128) || e[i].flags<0 || e[i].flags>255 || e[i].unit<0 || e[i].unit>100 || e[i].armor<(kind==3?-20000:0) || e[i].armor>20000 || e[i].shield<(kind==3?-20000:0) || e[i].shield>20000)return false;
        if(e[i].height<0 || e[i].height>32 || e[i].altitude< -500000 || e[i].altitude>4000000 || e[i].verticalSpeed< -100000 || e[i].verticalSpeed>100000 || e[i].phaseTicks<0 || e[i].phaseTicks>1000000)return false;
        if((kind==0 && !NativeCatalog::plant(e[i].unit)) || (kind==1 && (e[i].unit>32)) || (kind==3 && (e[i].unit<4 || e[i].unit>6)))return false;
        for(unsigned j=0;j<i;++j)if(e[i].id==e[j].id || (kind==0 && e[i].row==e[j].row && e[i].col==e[j].col))return false;
    }
    return true;
}
inline bool valid(const StateData& s){
    return s.match && s.plants<=MaxPlants && s.zombies<=MaxZombies && s.shots<=MaxShots && s.coins<=MaxCoins && s.sun>=0 && s.sun<=10000 && s.brains>=0 && s.brains<=10000 &&
        s.winner>=0 && s.winner<=2 && validEntities(s.plant,s.plants,0) && validEntities(s.zombie,s.zombies,1) && validEntities(s.shot,s.shots,2) && validEntities(s.coin,s.coins,3);
}
inline unsigned encode(const StateData& s,unsigned char* out,unsigned cap){
    unsigned n=HeaderBytes+EntityBytes*(s.plants+s.zombies+s.shots+s.coins);if(!valid(s) || n>cap)return 0;
    unsigned header[9]={s.match,s.tick,s.plants,s.zombies,s.shots,(unsigned)s.sun,(unsigned)s.brains,(unsigned)s.winner,s.coins};
    for(unsigned i=0;i<9;++i)put(out+4*i,header[i]);
    unsigned pos=HeaderBytes;
    for(int kind=0;kind<4;++kind){const Entity* es=kind==0?s.plant:kind==1?s.zombie:kind==2?s.shot:s.coin;unsigned count=kind==0?s.plants:kind==1?s.zombies:kind==2?s.shots:s.coins;
        for(unsigned i=0;i<count;++i){const Entity& e=es[i];unsigned words[20]={e.id,(unsigned)e.row,(unsigned)e.col,(unsigned)e.hp,(unsigned)e.x,(unsigned)e.y,(unsigned)e.phase,(unsigned)e.flags,(unsigned)e.unit,(unsigned)e.armor,(unsigned)e.shield,e.links[0],e.links[1],e.links[2],e.links[3],e.links[4],(unsigned)e.height,(unsigned)e.altitude,(unsigned)e.verticalSpeed,(unsigned)e.phaseTicks};
            for(unsigned j=0;j<20;++j)put(out+pos+4*j,words[j]);
            pos+=EntityBytes;
        }
    }
    return n;
}
inline bool decode(const unsigned char* in,unsigned n,StateData& s){
    if(n<HeaderBytes)return false;
    s.match=get(in);s.tick=get(in+4);s.plants=get(in+8);s.zombies=get(in+12);s.shots=get(in+16);
    s.sun=(int)get(in+20);s.brains=(int)get(in+24);s.winner=(int)get(in+28);s.coins=get(in+32);
    if(s.plants>MaxPlants || s.zombies>MaxZombies || s.shots>MaxShots || s.coins>MaxCoins || n!=HeaderBytes+EntityBytes*(s.plants+s.zombies+s.shots+s.coins))return false;
    unsigned pos=HeaderBytes;
    for(int kind=0;kind<4;++kind){Entity* es=kind==0?s.plant:kind==1?s.zombie:kind==2?s.shot:s.coin;unsigned count=kind==0?s.plants:kind==1?s.zombies:kind==2?s.shots:s.coins;
        for(unsigned i=0;i<count;++i){es[i]={get(in+pos),(int)get(in+pos+4),(int)get(in+pos+8),(int)get(in+pos+12),(int)get(in+pos+16),(int)get(in+pos+20),(int)get(in+pos+24),(int)get(in+pos+28),(int)get(in+pos+32),(int)get(in+pos+36),(int)get(in+pos+40)};for(unsigned j=0;j<5;++j)es[i].links[j]=get(in+pos+44+j*4);es[i].height=(int)get(in+pos+64);es[i].altitude=(int)get(in+pos+68);es[i].verticalSpeed=(int)get(in+pos+72);es[i].phaseTicks=(int)get(in+pos+76);pos+=EntityBytes;}
    }
    return valid(s);
}
inline unsigned action(unsigned kind,unsigned row,unsigned col,unsigned unit=255){
    if(unit==255)unit=kind==Zombie?60:0;
    return kind|(row<<8)|(col<<16)|(unit<<24);
}
inline bool parseAction(unsigned value,int role,int& row,int& col){
    unsigned kind=value&255;row=(value>>8)&255;col=(value>>16)&255;
    return row<5 && col<9 && NativeCatalog::card(role,value>>24) &&
        ((role==0 && kind==Plant) || (role==1 && kind==Zombie));
}
}

#pragma once
// Stable, bounded wire state. Coordinates use 1/1000 of a native lawn pixel.
namespace Match {
constexpr unsigned Protocol=3,MaxPlants=45,MaxZombies=48,MaxShots=96,MaxStateBytes=32+32*(MaxPlants+MaxZombies+MaxShots);
enum Event { Start=10,Ready=11,Go=12,Command=13,Stop=14,State=15,Finish=16 };
enum Action { Plant=1,Zombie=2 };
struct Entity {unsigned id;int row,col,hp,x,y,phase,flags;};
struct StateData {
    unsigned match,tick,plants,zombies,shots;
    int sun,brains,winner;
    Entity plant[MaxPlants],zombie[MaxZombies],shot[MaxShots];
};
inline void put(unsigned char* p,unsigned v){p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;}
inline unsigned get(const unsigned char* p){return (unsigned(p[0])<<24)|(unsigned(p[1])<<16)|(unsigned(p[2])<<8)|p[3];}
inline bool validEntities(const Entity* e,unsigned n,int kind){
    for(unsigned i=0;i<n;++i){
        if(!e[i].id || e[i].row<0 || e[i].row>=5 || e[i].col<0 || e[i].col>=9 || e[i].hp<0 || e[i].hp>10000 ||
           e[i].x< -300000 || e[i].x>2000000 || e[i].y< -300000 || e[i].y>1200000 || e[i].phase<0 || e[i].phase>128 || e[i].flags<0 || e[i].flags>255)return false;
        for(unsigned j=0;j<i;++j)if(e[i].id==e[j].id || (kind==0 && e[i].row==e[j].row && e[i].col==e[j].col))return false;
    }
    return true;
}
inline bool valid(const StateData& s){
    return s.match && s.plants<=MaxPlants && s.zombies<=MaxZombies && s.shots<=MaxShots && s.sun>=0 && s.sun<=10000 && s.brains>=0 && s.brains<=10000 &&
        s.winner>=0 && s.winner<=2 && validEntities(s.plant,s.plants,0) && validEntities(s.zombie,s.zombies,1) && validEntities(s.shot,s.shots,2);
}
inline unsigned encode(const StateData& s,unsigned char* out,unsigned cap){
    unsigned n=32+32*(s.plants+s.zombies+s.shots);if(!valid(s) || n>cap)return 0;
    unsigned header[8]={s.match,s.tick,s.plants,s.zombies,s.shots,(unsigned)s.sun,(unsigned)s.brains,(unsigned)s.winner};
    for(unsigned i=0;i<8;++i)put(out+4*i,header[i]);
    unsigned pos=32;
    for(int kind=0;kind<3;++kind){const Entity* es=kind==0?s.plant:kind==1?s.zombie:s.shot;unsigned count=kind==0?s.plants:kind==1?s.zombies:s.shots;
        for(unsigned i=0;i<count;++i){const Entity& e=es[i];unsigned words[8]={e.id,(unsigned)e.row,(unsigned)e.col,(unsigned)e.hp,(unsigned)e.x,(unsigned)e.y,(unsigned)e.phase,(unsigned)e.flags};
            for(unsigned j=0;j<8;++j)put(out+pos+4*j,words[j]);
            pos+=32;
        }
    }
    return n;
}
inline bool decode(const unsigned char* in,unsigned n,StateData& s){
    if(n<32)return false;
    s.match=get(in);s.tick=get(in+4);s.plants=get(in+8);s.zombies=get(in+12);s.shots=get(in+16);
    s.sun=(int)get(in+20);s.brains=(int)get(in+24);s.winner=(int)get(in+28);
    if(s.plants>MaxPlants || s.zombies>MaxZombies || s.shots>MaxShots || n!=32+32*(s.plants+s.zombies+s.shots))return false;
    unsigned pos=32;
    for(int kind=0;kind<3;++kind){Entity* es=kind==0?s.plant:kind==1?s.zombie:s.shot;unsigned count=kind==0?s.plants:kind==1?s.zombies:s.shots;
        for(unsigned i=0;i<count;++i){es[i]={get(in+pos),(int)get(in+pos+4),(int)get(in+pos+8),(int)get(in+pos+12),(int)get(in+pos+16),(int)get(in+pos+20),(int)get(in+pos+24),(int)get(in+pos+28)};pos+=32;}
    }
    return valid(s);
}
inline unsigned action(unsigned kind,unsigned row,unsigned col){return kind|(row<<8)|(col<<16);}
inline bool parseAction(unsigned value,int role,int& row,int& col){
    unsigned kind=value&255;row=(value>>8)&255;col=(value>>16)&255;
    return (value>>24)==0 && row<5 && col<9 && ((role==0 && kind==Plant) || (role==1 && kind==Zombie && col==8));
}
}

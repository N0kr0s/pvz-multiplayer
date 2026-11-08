#pragma once
namespace NativeCatalog {
// Native seed/card IDs. Terrain, upgrade and layered plants need additional
// state replication and are deliberately unavailable on this daylight board.
constexpr int ZombieCards[]={60,101,61,62,63,105,69,67,72,109,70,113,115,68,65,71,119,66,64,122,73,74,132};
constexpr int ZombieTypes[]={0,1,2,3,4,5,6,7,8,9,12,13,15,16,17,18,19,20,21,22,23,24,32};
constexpr int ZombieCount=sizeof(ZombieTypes)/sizeof(*ZombieTypes);
constexpr unsigned MaxDeck=8;
inline bool zombie(int type){for(int value:ZombieTypes)if(value==type)return true;return false;}
// Prices for units without an original I, Zombie seed packet.
inline int extraCost(int card){switch(card){case 101:return 75;case 105:return 125;case 109:return 50;case 113:return 300;case 115:return 175;case 119:return 200;case 122:return 300;case 132:return 500;default:return -1;}}
inline bool plant(int type){
    switch(type){
    case 0:case 1:case 2:case 3:case 4:case 5:case 6:case 7:
    case 17:case 18:case 20:case 21:case 22:case 23:case 25:case 26:
    case 27:case 28:case 29:case 31:case 32:case 34:case 36:case 37:case 38:case 39:return true;
    default:return false;
    }
}
inline int zombieType(int card){for(int i=0;i<ZombieCount;++i)if(card==ZombieCards[i])return ZombieTypes[i];return -1;}
inline bool card(int role,int type){return role?zombieType(type)>=0:plant(type);}
inline int chooserCard(int role,int index){return role?(index>=0 && index<ZombieCount?ZombieCards[index]:-1):index;}
inline unsigned deckSize(int role){return role?8:6;}
struct Deck {
    int types[MaxDeck]={-1,-1,-1,-1,-1,-1,-1,-1};unsigned count=0,mask=0;
    bool set(int role,unsigned index,unsigned total,int type){
        if(total!=deckSize(role) || index>=total || !card(role,type))return false;
        if((mask&(1u<<index)) && types[index]!=type)return false;
        for(unsigned i=0;i<total;++i)if(i!=index && (mask&(1u<<i)) && types[i]==type)return false;
        count=total;types[index]=type;mask|=1u<<index;return true;
    }
    bool complete()const{return count && mask==(1u<<count)-1;}
    int find(int type)const{for(unsigned i=0;i<count;++i)if((mask&(1u<<i)) && types[i]==type)return (int)i;return -1;}
};
}

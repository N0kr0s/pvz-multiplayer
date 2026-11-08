#pragma once
#include <string.h>
namespace LanIP {
// Prefer a home LAN address over VPN/CGNAT when advertising the host.
inline int rank(const char* ip){
    unsigned parts[4]={};int index=0,digits=0;
    for(const char* p=ip;;++p){
        if(*p>='0' && *p<='9'){if(++digits>3)return 0;parts[index]=parts[index]*10+(*p-'0');if(parts[index]>255)return 0;}
        else if(*p=='.' || !*p){if(!digits)return 0;if(!*p){if(index!=3)return 0;break;}if(++index>3)return 0;digits=0;}
        else return 0;
    }
    if(parts[0]==0 || parts[0]==127 || parts[0]>=224 || (parts[0]==169 && parts[1]==254))return 0;
    if(parts[0]==192 && parts[1]==168)return 5;
    if(parts[0]==10)return 4;
    if(parts[0]==172 && parts[1]>=16 && parts[1]<=31)return 3;
    if(parts[0]==100 && parts[1]>=64 && parts[1]<=127)return 1;
    return 2;
}
inline void consider(char* primary,unsigned capacity,const char* candidate){
    if(rank(candidate)>rank(primary) && strlen(candidate)<capacity)strcpy(primary,candidate);
}
}

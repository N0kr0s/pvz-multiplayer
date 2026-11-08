#include "../hook/reconcile.h"
#include "../hook/native-layout.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
    using namespace Reconcile;
    Error error={10,0};
    assert(plan(2,-3,false,false,error)==Ignore && error.x==0 && error.y==0);
    assert(plan(6,0,false,false,error)==Ignore);
    assert(plan(10,0,false,false,error)==Smooth);
    float moved=0;
    for(int i=0;i<200;++i){Error delta=step(error);assert(delta.x>=0 && delta.x<=1.25f);moved+=delta.x;}
    assert(absolute(moved-10)<0.001f && error.x==0);
    assert(plan(-20,40,false,false,error)==Smooth);
    Error delta=step(error);assert(absolute(delta.x+0.625f)<0.0001f && delta.y==1.25f);
    assert(plan(1,0,false,false,error)==Ignore && error.x==0 && error.y==0);
    assert(plan(0,0,true,false,error)==Snap);
    assert(plan(0,0,false,true,error)==Snap);
    assert(plan(97,0,false,false,error)==Snap);
    assert(plan(96,0,false,false,error)==Smooth);
    // A moving native zombie continues walking during correction; do not lerp
    // every update towards an obsolete absolute snapshot position.
    assert(plan(20,0,false,false,error)==Smooth);
    float x=100;
    for(int i=0;i<200;++i){x-=0.25f;x+=step(error).x;}
    assert(absolute(x-70)<0.001f);
    // Coin's actual ID precedes four bytes of tail padding. The old stride-4
    // assumption reads zero and prevents capture, selection and collection.
    unsigned char coin[NativeLayout::CoinStride]={};unsigned id=0x12340002;
    memcpy(coin+NativeLayout::CoinID,&id,4);
    unsigned correct,old;memcpy(&correct,coin+NativeLayout::idOffset(3),4);memcpy(&old,coin+NativeLayout::CoinStride-4,4);
    assert(correct==id && old==0);
    puts("Reconciliation OK: deadband, gradual moving correction, teleports; sun ID skips tail padding.");
}

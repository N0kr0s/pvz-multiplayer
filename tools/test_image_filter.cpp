#include "../hook/image-filter.h"
#include <cassert>
#include <iostream>
int main() {
    unsigned int out[9]={};
    unsigned int checker[]={0xff000000,0xffffffff,0xffffffff,0xff000000};
    assert(reduceTexture(checker,2,2,out,1,1));
    assert(out[0]==0xff808080); // Averaging retains detail that nearest-neighbor drops.
    unsigned int alphaEdge[]={0xffff0000,0x000000ff};
    assert(reduceTexture(alphaEdge,2,1,out,1,1));
    assert(out[0]==0x80ff0000); // Transparent blue must not create a dark/blue edge.
    unsigned int flat[35];for(auto& pixel:flat) pixel=0x7f254669;
    assert(reduceTexture(flat,7,5,out,3,3));
    for(auto pixel:out) assert(pixel==0x7f254669); // Fractional footprints remain normalized.
    assert(!reduceTexture(flat,7,5,out,8,1));
    std::cout<<"Alpha-aware texture reduction OK\n";
}

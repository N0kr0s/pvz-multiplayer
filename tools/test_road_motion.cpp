#include "../hook/road-motion.h"
#include "../hook/road-layout.h"
#include <cassert>
#include <cstdio>
int main(){
    assert(RoadMotion::speed(0)==0.23f && RoadMotion::speed(7)==0.40f);
    for(int row=0;row<RoadLayout::Rows;++row){
        float curb=RoadLayout::leftCurbX(row),exit=RoadLayout::transferX(row);
        assert(curb>189 && curb<266);
        assert(exit+RoadLayout::FootX==curb);
        if(row)assert(exit<RoadLayout::transferX(row-1));
        for(int col=0;col<RoadLayout::Columns;++col){
            float feet=RoadLayout::spawnX(col)+RoadLayout::FootX;
            int baseline=RoadLayout::bodyY(row)+RoadLayout::FootY;
            assert(feet==RoadLayout::cellCenterX(col));
            assert(baseline==RoadLayout::cellCenterY(row));
            assert(feet>RoadLayout::Left+col*RoadLayout::CellWidth);
            assert(feet<RoadLayout::Left+(col+1)*RoadLayout::CellWidth);
            assert(baseline>RoadLayout::Top+row*RoadLayout::RowHeight);
            assert(baseline<RoadLayout::Top+(row+1)*RoadLayout::RowHeight);
            float x=RoadLayout::spawnX(col);unsigned ticks=0;
            while(x>exit){x-=RoadMotion::speed(0);++ticks;}
            assert(x<=exit && x+RoadLayout::FootX<=curb);
            assert(x+RoadLayout::FootX>curb-1);
            assert(ticks>0 && ticks<4000);
        }
    }
    float decoded=0;
    assert(RoadMotion::decode((unsigned)-159000,decoded) && decoded== -159.0f);
    assert(RoadMotion::decode(960000,decoded) && decoded==960.0f);
    assert(!RoadMotion::decode((unsigned)-201000,decoded));
    assert(!RoadMotion::decode(1100001,decoded));
    puts("Road layout OK: all 45 cell centers, row-specific curb transfers and unchanged road speed.");
}

#pragma once
namespace RoadLayout {
// zombie_map2.png, fitted to the native 1067x600 viewport.
constexpr int Left=336,Top=88,CellWidth=58,RowHeight=96,Columns=9,Rows=5;
constexpr int Width=CellWidth*Columns;
constexpr int FootX=60,FootY=100;
inline float cellCenterX(int col){return Left+CellWidth*0.5f+col*CellWidth;}
inline int cellCenterY(int row){return Top+RowHeight/2+row*RowHeight;}
// Native drawing uses a sprite origin; place the feet, not that origin, in the cell.
inline float spawnX(int col){return cellCenterX(col)-FootX;}
inline int bodyY(int row){return cellCenterY(row)-FootY;}
// Perspective left curb of the road: x=266 at y=50, x=189 at y=600.
inline float leftCurbX(int row){return 266.0f-(cellCenterY(row)-50)*77.0f/550.0f;}
inline float transferX(int row){return leftCurbX(row)-FootX;}
}

#pragma once
namespace MatchUI {
// PvZWidescreenForSteam logical canvas (16:9 rounded to whole pixels).
constexpr int Width=1067,Height=600;
constexpr int MenuW=90,MenuH=33,MenuX=Width-MenuW-12,MenuY=8;
inline bool menuAt(int x,int y){return x>=MenuX && x<MenuX+MenuW && y>=MenuY && y<MenuY+MenuH;}
struct Cell {int row=-1,col=-1,x=0,y=0;};
inline bool inside(int x,int y,int left,int top,int width,int height){return x>=left && x<left+width && y>=top && y<top+height;}
}

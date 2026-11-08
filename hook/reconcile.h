#pragma once
// Positions are logical PvZ pixels, independent of display resolution.
namespace Reconcile {
constexpr float Tolerance=6.0f,Teleport=96.0f;
inline float absolute(float x){return x<0?-x:x;}
struct Error {float x=0,y=0;};
enum Mode { Ignore,Smooth,Snap };
inline Mode plan(float dx,float dy,bool first,bool changedLane,Error& error){
    float distance2=dx*dx+dy*dy;
    if(first || changedLane || distance2>Teleport*Teleport){error={};return Snap;}
    if(distance2<=Tolerance*Tolerance){error={};return Ignore;}
    error={dx,dy};return Smooth;
}
inline Error step(Error& error){
    Error delta={error.x*0.10f,error.y*0.10f};
    float largest=absolute(delta.x)>absolute(delta.y)?absolute(delta.x):absolute(delta.y);
    if(largest>1.25f){delta.x*=1.25f/largest;delta.y*=1.25f/largest;}
    if(absolute(error.x)<0.05f)delta.x=error.x;
    if(absolute(error.y)<0.05f)delta.y=error.y;
    error.x-=delta.x;error.y-=delta.y;return delta;
}
}

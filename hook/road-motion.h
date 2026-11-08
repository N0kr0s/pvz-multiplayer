#pragma once
namespace RoadMotion {
// Only the deployment road uses these values; the lawn remains native PvZ.
constexpr float MinWireX=-200.0f,MaxWireX=1100.0f;
inline float speed(int zombieType){return zombieType==7?0.40f:0.23f;}
inline bool decode(unsigned wire,float& x){int signedX=(int)wire;if(signedX< -200000 || signedX>1100000)return false;x=signedX/1000.0f;return true;}
}

#pragma once
namespace NativeLayout {
constexpr unsigned CoinStride=0xd8,CoinID=0xd0;
// Coin has tail padding AFTER its DataArray ID; its ID is not stride-4.
inline unsigned idOffset(int kind){return kind==3?CoinID:(kind==0?0x148:kind==1?0x158:0x90);}
}

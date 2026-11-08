#pragma once
namespace LanPorts {
// A small known range allows a precise LAN firewall rule, unlike ephemeral ports.
constexpr unsigned First=35900,Count=10;
inline unsigned candidate(unsigned seed,unsigned attempt){return First+((seed%Count)+attempt)%Count;}
}

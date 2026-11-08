#pragma once
// Native lobby transport. No game objects are read or mutated by the worker.
enum LanPhase { LAN_IDLE, LAN_STARTING, LAN_LISTENING, LAN_CONNECTING, LAN_HANDSHAKE, LAN_CONNECTED, LAN_ERROR };
struct LanSnapshot {
    LanPhase phase;
    bool host;
    int role; // 0 plants, 1 zombies; host chooses, client receives the opposite.
    unsigned short port;
    char addresses[128], peer[64], message[128];
    unsigned rtt;
};
struct LanService;
LanService* lanCreate();
void lanDestroy(LanService*);
void lanHost(LanService*, int role);
void lanJoin(LanService*, const char* ipv4, unsigned short port);
void lanStop(LanService*);
LanSnapshot lanSnapshot(LanService*);

#include "match-state.h"
struct LanEvent {unsigned type,value,nonce;};
bool lanSendEvent(LanService*,unsigned type,unsigned value,unsigned nonce);
bool lanPollEvent(LanService*,LanEvent*);
bool lanPublishState(LanService*,const Match::StateData&);
bool lanReadState(LanService*,Match::StateData*);

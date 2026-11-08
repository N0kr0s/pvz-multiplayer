// Integration tests use the exact transport compiled into pvzmp-hook.dll.
#include "../hook/lan.h"
#include "../hook/lan-ports.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#ifdef PVZMP_LAN_TEST_NETWORK
#include <netdb.h>
#include <fcntl.h>
#include "lan_test_network.h"
#endif
static void require(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
static LanSnapshot waitFor(LanService* s,LanPhase phase,int ms=3000){
    for(int i=0;i<ms/10;++i){auto v=lanSnapshot(s);if(v.phase==phase)return v;std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    auto v=lanSnapshot(s);fprintf(stderr,"Expected phase %d, got %d: %s\n",phase,v.phase,v.message);exit(1);
}
static int rawConnect(unsigned short port){
    int fd=socket(AF_INET,SOCK_STREAM,0);require(fd>=0,"raw socket");
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&address.sin_addr);
    require(connect(fd,(sockaddr*)&address,sizeof(address))==0,"raw connect");
    timeval timeout={2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));return fd;
}
static void sendAll(int fd,const void* data,unsigned length){
    auto p=(const char*)data;
    while(length){int n=send(fd,p,length,MSG_NOSIGNAL);require(n>0,"raw send");p+=n;length-=n;}
}
static LanEvent waitEvent(LanService* s,unsigned type){
    for(int i=0;i<400;++i){LanEvent e;if(lanPollEvent(s,&e)){require(e.type==type,"ordered match event");return e;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    auto v=lanSnapshot(s);fprintf(stderr,"Waiting for event %u: phase=%d %s\n",type,(int)v.phase,v.message);
    require(false,"match event timeout");return {};
}
static void matchTraffic(LanService* host,LanService* client){
    require(lanSendEvent(host,Match::Start,123,0),"host START queued");
    require(waitEvent(client,Match::Start).value==123,"client sees match id");
    require(!lanSendEvent(client,Match::Start,123,0),"client cannot START");
    unsigned pick=60|(0u<<8)|(1u<<16)|(8u<<24);
    require(lanSendEvent(host,Match::Pick,pick,123),"host loadout event");require(waitEvent(client,Match::Pick).value==pick,"client sees native loadout");
    require(lanSendEvent(client,Match::Pick,5|(0u<<8)|(0u<<16)|(6u<<24),123),"client loadout event");waitEvent(host,Match::Pick);
    require(lanSendEvent(client,Match::Ready,123,0),"client READY queued");waitEvent(host,Match::Ready);
    require(lanSendEvent(host,Match::Intro,123,0),"host releases native intros");waitEvent(client,Match::Intro);
    require(!lanSendEvent(client,Match::Intro,123,0),"client cannot release chooser barrier");
    require(lanSendEvent(client,Match::IntroDone,123,0),"client intro finished");waitEvent(host,Match::IntroDone);
    require(lanSendEvent(host,Match::IntroDone,123,0),"host intro finished");waitEvent(client,Match::IntroDone);
    require(lanSendEvent(host,Match::Death,1,65536),"host confirms native death");
    require(waitEvent(client,Match::Death).nonce==65536,"death carries stable authoritative ID");
    require(!lanSendEvent(client,Match::Death,1,65536),"client cannot confirm death");
    require(lanSendEvent(host,Match::Go,123,0),"host GO queued");waitEvent(client,Match::Go);
    unsigned home=7u|(2u<<8)|(unsigned(0xff51)<<16);
    require(lanSendEvent(host,Match::Home,home,65536),"host house presentation metadata");
    require(waitEvent(client,Match::Home).value==home,"house type row and signed X preserved");
    require(!lanSendEvent(client,Match::Home,home,65536),"client cannot announce house entry");
    require(lanSendEvent(host,Match::Mower,2|(2u<<8),350000),"authoritative mower update");
    require(waitEvent(client,Match::Mower).nonce==350000,"mower position preserved");
    require(!lanSendEvent(client,Match::Mower,2,0),"client cannot authorize mower state");
    unsigned deployment=Match::action(Match::Zombie,3,5,60);
    require(lanSendEvent(host,Match::RoadSpawn,deployment,1),"road deployment authorized by host");
    require(waitEvent(client,Match::RoadSpawn).value==deployment,"road lane column and card preserved");
    require(!lanSendEvent(client,Match::RoadSpawn,deployment,1),"client cannot deploy without authority");
    require(lanSendEvent(host,Match::RoadPosition,330000,1),"road position correction");waitEvent(client,Match::RoadPosition);
    require(lanSendEvent(host,Match::RoadDone,123,1),"road transfer ordered before lawn spawn");waitEvent(client,Match::RoadDone);
    unsigned action=Match::action(Match::Plant,2,3);
    require(lanSendEvent(client,Match::Command,action,1),"client command queued");
    auto command=waitEvent(host,Match::Command);require(command.value==action && command.nonce==1,"command payload and sequence");
    require(lanSendEvent(host,Match::Spawn,Match::action(Match::Plant,2,3,5),65536),"native spawn event");
    require(waitEvent(client,Match::Spawn).nonce==65536,"spawn has authoritative entity id");
    require(!lanSendEvent(client,Match::Spawn,action,65536),"client cannot authorize native spawn");
    require(lanSendEvent(client,Match::Collect,65537,123),"sun collection request");waitEvent(host,Match::Collect);
    require(!lanSendEvent(host,Match::Collect,65537,123),"host sends collection confirmation, not requests");
    require(lanSendEvent(host,Match::Collected,65537,123),"sun collection confirmation");waitEvent(client,Match::Collected);
    Match::StateData s={};s.match=123;s.tick=44;s.sun=200;s.brains=100;s.plants=Match::MaxPlants;s.zombies=Match::MaxZombies;s.shots=Match::MaxShots;s.coins=Match::MaxCoins;
    for(unsigned i=0;i<s.plants;++i)s.plant[i]={1000+i,(int)(i/9),(int)(i%9),300,40000+(int)(i%9)*80000,80000+(int)(i/9)*100000,1,0};
    for(unsigned i=0;i<s.zombies;++i)s.zombie[i]={2000+i,(int)(i%5),0,270,800000-(int)i*1000,80000+(int)(i%5)*100000,0,0};
    for(unsigned i=0;i<s.shots;++i)s.shot[i]={3000+i,(int)(i%5),0,0,300000+(int)i*100,100000+(int)(i%5)*100000,0,0};
    for(unsigned i=0;i<s.coins;++i)s.coin[i]={4000+i,1000,30,0,300000+(int)i*100,100000,400,4,4,-500,1500};
    require(lanPublishState(host,s),"maximum snapshot queued");require(!lanPublishState(client,s),"client cannot publish authority state");
    Match::StateData received={};bool got=false;
    for(int i=0;i<800;++i){if(lanReadState(client,&received)){got=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(got && received.match==123 && received.tick==44 && received.plants==45 && received.zombies==48 && received.shots==96 && received.coins==64,"maximum snapshot received across fragmented TCP");
    require(received.shot[95].id==s.shot[95].id && received.shot[95].x==s.shot[95].x,"snapshot tail preserved");
    for(unsigned i=45;i<145;++i){s.tick=i;require(lanPublishState(host,s),"snapshot coalescing");}
    got=false;
    for(int i=0;i<800;++i){if(lanReadState(client,&received) && received.tick==144){got=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    require(got,"latest snapshot is eventually delivered under backpressure");
    require(lanSendEvent(client,Match::Stop,0,123),"STOP queued");require(waitEvent(host,Match::Stop).nonce==123,"STOP identifies match");
}
int main(){
    auto host=lanCreate(),client=lanCreate();require(host && client,"worker initialization");
    for(int role=0;role<=1;++role){
        lanHost(host,role);auto h=waitFor(host,LAN_LISTENING);require(h.port>=LanPorts::First && h.port<LanPorts::First+LanPorts::Count,"generated port within firewall range");
        lanJoin(client,"127.0.0.1",h.port);auto c=waitFor(client,LAN_CONNECTED);h=waitFor(host,LAN_CONNECTED);
        require(h.host && !c.host && h.role==role && c.role==1-role,"opposite roles negotiated");
        matchTraffic(host,client);
        std::this_thread::sleep_for(std::chrono::milliseconds(2300));
        require(lanSnapshot(host).phase==LAN_CONNECTED && lanSnapshot(client).phase==LAN_CONNECTED,"heartbeat keeps session alive");
        // A third TCP client must not replace the authenticated peer.
        int extra=rawConnect(h.port);close(extra);
        require(lanSnapshot(host).phase==LAN_CONNECTED,"third peer cannot replace client");
        lanStop(client);waitFor(client,LAN_IDLE);waitFor(host,LAN_ERROR);lanStop(host);waitFor(host,LAN_IDLE);
    }
    // Partial reads are common on TCP; deliver HELLO a byte at a time.
    lanHost(host,0);auto h=waitFor(host,LAN_LISTENING);int fd=rawConnect(h.port);
    unsigned words[4]={htonl(0x50565a4c),htonl((Match::Protocol<<16)|1),0,0};
    for(unsigned i=0;i<sizeof(words);++i){sendAll(fd,(char*)words+i,1);std::this_thread::sleep_for(std::chrono::milliseconds(12));}
    waitFor(host,LAN_CONNECTED);
    unsigned char reply[16];int received=0;
    for(int attempts=0;received<16 && attempts<2000;++attempts){
        int n=recv(fd,reply+received,16-received,0);
        if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}
        require(n>0,"WELCOME response");received+=n;
    }
    require(received==16,"complete WELCOME response");
    unsigned assigned;memcpy(&assigned,reply+8,4);require(ntohl(assigned)==1,"wire role");
    // No heartbeat response: detect a silent peer.
    waitFor(host,LAN_ERROR,7500);require(strstr(lanSnapshot(host).message,"responding"),"silent peer timeout");close(fd);
    lanHost(host,0);h=waitFor(host,LAN_LISTENING);fd=rawConnect(h.port);
    words[1]=htonl(((Match::Protocol+1)<<16)|1);sendAll(fd,words,sizeof(words));
    waitFor(host,LAN_ERROR);require(strstr(lanSnapshot(host).message,"protocol"),"version rejection");close(fd);
    lanJoin(client,"300.1.2.3",42);waitFor(client,LAN_ERROR);
    lanJoin(client,"127.0.0.1",0);waitFor(client,LAN_ERROR);
    lanStop(host);waitFor(host,LAN_IDLE);
    lanJoin(client,"127.0.0.1",h.port);waitFor(client,LAN_ERROR);
    lanDestroy(client);lanDestroy(host);
    LanService* occupied[10]={};unsigned seen=0;
    for(unsigned i=0;i<10;++i){occupied[i]=lanCreate();require(occupied[i]!=nullptr,"concurrent host creation");lanHost(occupied[i],i%2);
        auto listening=waitFor(occupied[i],LAN_LISTENING);unsigned bit=1u<<(listening.port-LanPorts::First);
        require(!(seen&bit),"occupied ports skipped");seen|=bit;
    }
    require(seen==1023,"entire configured host port range used");
    auto extraHost=lanCreate();require(extraHost!=nullptr,"extra host creation");lanHost(extraHost,0);
    waitFor(extraHost,LAN_ERROR);lanDestroy(extraHost);
    for(auto instance:occupied)lanDestroy(instance);
#ifdef PVZMP_LAN_TEST_NETWORK
    puts("Mode: deterministic simulated sockets (real TCP still requires an unrestricted machine).");
#endif
    puts("LAN match traffic OK: native loadouts, READY/GO, spawn/collect authority, max snapshots, backpressure and host port collisions.");
    puts("LAN transport OK: both roles, generated ports, handshake, heartbeat, disconnect, reconnect, third-peer rejection, fragmented TCP, timeout, bad version and address.");
}

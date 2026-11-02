// Integration tests use the exact transport compiled into pvzmp-hook.dll.
#include "../hook/lan.h"
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
int main(){
    auto host=lanCreate(),client=lanCreate();require(host && client,"worker initialization");
    for(int role=0;role<=1;++role){
        lanHost(host,role);auto h=waitFor(host,LAN_LISTENING);require(h.port!=0,"OS-generated port");
        lanJoin(client,"127.0.0.1",h.port);auto c=waitFor(client,LAN_CONNECTED);h=waitFor(host,LAN_CONNECTED);
        require(h.host && !c.host && h.role==role && c.role==1-role,"opposite roles negotiated");
        std::this_thread::sleep_for(std::chrono::milliseconds(2300));
        require(lanSnapshot(host).phase==LAN_CONNECTED && lanSnapshot(client).phase==LAN_CONNECTED,"heartbeat keeps session alive");
        // A third TCP client must not replace the authenticated peer.
        int extra=rawConnect(h.port);close(extra);
        require(lanSnapshot(host).phase==LAN_CONNECTED,"third peer cannot replace client");
        lanStop(client);waitFor(client,LAN_IDLE);waitFor(host,LAN_ERROR);lanStop(host);waitFor(host,LAN_IDLE);
    }
    // Partial reads are common on TCP; deliver HELLO a byte at a time.
    lanHost(host,0);auto h=waitFor(host,LAN_LISTENING);int fd=rawConnect(h.port);
    unsigned words[4]={htonl(0x50565a4c),htonl(0x10001),0,0};
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
    words[1]=htonl(0x20001);sendAll(fd,words,sizeof(words));
    waitFor(host,LAN_ERROR);require(strstr(lanSnapshot(host).message,"protocol"),"version rejection");close(fd);
    lanJoin(client,"300.1.2.3",42);waitFor(client,LAN_ERROR);
    lanJoin(client,"127.0.0.1",0);waitFor(client,LAN_ERROR);
    lanStop(host);waitFor(host,LAN_IDLE);
    lanJoin(client,"127.0.0.1",h.port);waitFor(client,LAN_ERROR);
    lanDestroy(client);lanDestroy(host);
#ifdef PVZMP_LAN_TEST_NETWORK
    puts("Mode: deterministic simulated sockets (real TCP still requires an unrestricted machine).");
#endif
    puts("LAN transport OK: both roles, generated ports, handshake, heartbeat, disconnect, reconnect, third-peer rejection, fragmented TCP, timeout, bad version and address.");
}

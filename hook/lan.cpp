#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lan.h"
#ifdef PVZMP_LAN_TEST_NETWORK
#include "../tools/lan_test_network.h"
#endif
#ifdef _WIN32
using Socket=SOCKET;
static const Socket invalid=INVALID_SOCKET;
using Lock=CRITICAL_SECTION;
static void lock(Lock* p){EnterCriticalSection(p);}
static void unlock(Lock* p){LeaveCriticalSection(p);}
static void closeSocket(Socket s){if(s!=invalid)closesocket(s);}
static unsigned now(){return GetTickCount();}
static bool pending(){int e=WSAGetLastError();return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS;}
static bool nonblock(Socket s){u_long n=1;return ioctlsocket(s,FIONBIO,&n)==0;}
#else
using Socket=int;
static const Socket invalid=-1;
using Lock=pthread_mutex_t;
static void lock(Lock* p){pthread_mutex_lock(p);}
static void unlock(Lock* p){pthread_mutex_unlock(p);}
static void closeSocket(Socket s){if(s!=invalid)close(s);}
static unsigned now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return unsigned(t.tv_sec*1000+t.tv_nsec/1000000);}
static bool pending(){return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINPROGRESS;}
static bool nonblock(Socket s){return fcntl(s,F_SETFL,O_NONBLOCK)==0;}
#endif
static void copy(char* dst,const char* src,unsigned cap){if(!cap)return;unsigned i=0;for(;i+1<cap && src[i];++i)dst[i]=src[i];dst[i]=0;}
struct LanService {
    Lock mutex;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
    int command, role;
    bool quit;
    char ip[64];
    unsigned short port;
    LanSnapshot snapshot;
};
static void publish(LanService* s,const LanSnapshot& v){lock(&s->mutex);s->snapshot=v;unlock(&s->mutex);}
LanSnapshot lanSnapshot(LanService* s){LanSnapshot v={};if(s){lock(&s->mutex);v=s->snapshot;unlock(&s->mutex);}return v;}
static void addresses(char* out,unsigned cap){
    out[0]=0;
#ifdef _WIN32
    // Enumerate actual IPv4 interfaces; DNS can fail or resolve to loopback under Proton.
    ULONG size=16384;
    IP_ADAPTER_INFO* adapters=(IP_ADAPTER_INFO*)malloc(size);
    if(adapters){
        DWORD result=GetAdaptersInfo(adapters,&size);
        if(result==ERROR_BUFFER_OVERFLOW){free(adapters);adapters=(IP_ADAPTER_INFO*)malloc(size);result=adapters?GetAdaptersInfo(adapters,&size):ERROR_NOT_ENOUGH_MEMORY;}
        if(result==NO_ERROR)for(IP_ADAPTER_INFO* a=adapters;a;a=a->Next)
            for(IP_ADDR_STRING* address=&a->IpAddressList;address;address=address->Next){
                const char* p=address->IpAddress.String;
                if(!p[0] || !strcmp(p,"0.0.0.0") || !strncmp(p,"127.",4))continue;
                unsigned n=strlen(out),m=strlen(p);
                if(n+m+3<cap){if(n)strcat(out,", ");strcat(out,p);}
            }
        free(adapters);
    }
#else
    char name[256];
    if(gethostname(name,sizeof(name))==0){
        hostent* entry=gethostbyname(name);
        if(entry && entry->h_addrtype==AF_INET) for(int i=0;entry->h_addr_list[i];++i){
            in_addr a;memcpy(&a,entry->h_addr_list[i],4);
            const char* p=inet_ntoa(a);
            if(!p || !strncmp(p,"127.",4))continue;
            unsigned n=strlen(out),m=strlen(p);
            if(n+m+3<cap){if(n)strcat(out,", ");strcat(out,p);}
        }
    }
#endif
    if(!out[0])copy(out,"See host network settings",cap);
}
// Fixed 16-byte messages, network byte order: magic, version, type, value, nonce.
static void frame(unsigned char* b,unsigned type,unsigned value,unsigned nonce){
    const unsigned words[4]={0x50565a4c,0x00010000|type,value,nonce};
    for(int i=0;i<4;++i){unsigned v=htonl(words[i]);memcpy(b+4*i,&v,4);}
}
static unsigned word(const unsigned char* p){unsigned n;memcpy(&n,p,4);return ntohl(n);}
struct Wire {
    unsigned char input[16],output[256];
    unsigned used,queued,sent;
};
static bool queue(Wire& w,unsigned type,unsigned value,unsigned nonce){
    if(w.sent==w.queued)w.sent=w.queued=0;
    if(w.queued+16>sizeof(w.output))return false;
    frame(w.output+w.queued,type,value,nonce);w.queued+=16;return true;
}
#ifdef _WIN32
static DWORD WINAPI worker(void* arg){
#else
static void* worker(void* arg){
#endif
    LanService* s=(LanService*)arg;
    Socket listener=invalid,peer=invalid;
    LanSnapshot v={};Wire wire={};
    unsigned since=now(),lastRx=since,lastPing=since,pingNonce=0;
    bool connecting=false;
    for(;;){
        int command,role;char ip[64];unsigned short port;bool quit;
        lock(&s->mutex);command=s->command;s->command=0;role=s->role;copy(ip,s->ip,sizeof(ip));port=s->port;quit=s->quit;unlock(&s->mutex);
        if(quit)break;
        if(command){
            closeSocket(listener);closeSocket(peer);listener=peer=invalid;wire={};connecting=false;v={};
            since=lastRx=lastPing=now();
            if(command==1){
                v.host=true;v.role=role;v.phase=LAN_LISTENING;
                listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                sockaddr_in addr={};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);
                if(listener==invalid || !nonblock(listener) || bind(listener,(sockaddr*)&addr,sizeof(addr)) || listen(listener,1)){
                    v.phase=LAN_ERROR;copy(v.message,"Could not create LAN host",sizeof(v.message));
                }else{
#ifdef _WIN32
                    int length=sizeof(addr);
#else
                    socklen_t length=sizeof(addr);
#endif
                    if(getsockname(listener,(sockaddr*)&addr,&length)){
                        v.phase=LAN_ERROR;copy(v.message,"Could not read host port",sizeof(v.message));
                    }else{v.port=ntohs(addr.sin_port);addresses(v.addresses,sizeof(v.addresses));}
                }
            }else if(command==2){
                v.phase=LAN_CONNECTING;v.port=port;copy(v.peer,ip,sizeof(v.peer));
                sockaddr_in addr={};addr.sin_family=AF_INET;addr.sin_port=htons(port);
                if(!port || inet_pton(AF_INET,ip,&addr.sin_addr)!=1){
                    v.phase=LAN_ERROR;copy(v.message,"Enter a valid IPv4 address and port",sizeof(v.message));
                }else{
                    peer=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                    if(peer==invalid || !nonblock(peer)){v.phase=LAN_ERROR;copy(v.message,"Could not create socket",sizeof(v.message));}
                    else {int result=connect(peer,(sockaddr*)&addr,sizeof(addr));
                        if(result && !pending()){v.phase=LAN_ERROR;copy(v.message,"Host is unavailable",sizeof(v.message));}
                        else if(result)connecting=true;
                        else {v.phase=LAN_HANDSHAKE;queue(wire,1,0,0);}
                    }
                }
            }
        }
        if(v.phase==LAN_ERROR){closeSocket(peer);closeSocket(listener);peer=listener=invalid;connecting=false;}
        fd_set reads,writes;FD_ZERO(&reads);FD_ZERO(&writes);Socket maxfd=0;
        if(listener!=invalid){FD_SET(listener,&reads);if(listener>maxfd)maxfd=listener;}
        if(peer!=invalid){FD_SET(peer,&reads);if(connecting || wire.sent<wire.queued)FD_SET(peer,&writes);if(peer>maxfd)maxfd=peer;}
        timeval timeout={0,50000};
        if(listener==invalid && peer==invalid){
#ifdef _WIN32
            Sleep(50);
#else
            timespec delay={0,50000000};nanosleep(&delay,0);
#endif
        }else if(select((int)maxfd+1,&reads,&writes,0,&timeout)<0){
            v.phase=LAN_ERROR;copy(v.message,"Network polling failed",sizeof(v.message));publish(s,v);continue;
        }
        if(listener!=invalid && FD_ISSET(listener,&reads)){
            sockaddr_in addr={};
#ifdef _WIN32
            int length=sizeof(addr);
#else
            socklen_t length=sizeof(addr);
#endif
            Socket accepted=accept(listener,(sockaddr*)&addr,&length);
            if(accepted!=invalid){
                if(peer!=invalid || !nonblock(accepted))closeSocket(accepted);
                else{peer=accepted;v.phase=LAN_HANDSHAKE;copy(v.peer,inet_ntoa(addr.sin_addr),sizeof(v.peer));since=lastRx=now();wire={};}
            }
        }
        if(peer!=invalid && connecting && (FD_ISSET(peer,&writes)||FD_ISSET(peer,&reads))){
            int error=0;
#ifdef _WIN32
            int length=sizeof(error);
#else
            socklen_t length=sizeof(error);
#endif
            if(getsockopt(peer,SOL_SOCKET,SO_ERROR,(char*)&error,&length) || error){v.phase=LAN_ERROR;copy(v.message,"Connection refused or host unavailable",sizeof(v.message));}
            else{connecting=false;v.phase=LAN_HANDSHAKE;since=now();queue(wire,1,0,0);}
        }
        if(peer!=invalid && !connecting && v.phase!=LAN_ERROR && FD_ISSET(peer,&writes) && wire.sent<wire.queued){
#ifdef _WIN32
            int flags=0;
#else
            int flags=MSG_NOSIGNAL;
#endif
            int n=send(peer,(char*)wire.output+wire.sent,wire.queued-wire.sent,flags);
            if(n>0)wire.sent+=n;
            else if(n==0 || !pending()){v.phase=LAN_ERROR;copy(v.message,"Connection lost",sizeof(v.message));}
        }
        if(peer!=invalid && !connecting && v.phase!=LAN_ERROR && FD_ISSET(peer,&reads)){
            int n=recv(peer,(char*)wire.input+wire.used,16-wire.used,0);
            if(n>0){wire.used+=n;
                if(wire.used==16){
                    unsigned kind=word(wire.input+4),value=word(wire.input+8),nonce=word(wire.input+12);wire.used=0;
                    bool valid=word(wire.input)==0x50565a4c && (kind>>16)==1;kind&=65535;
                    if(valid && v.phase==LAN_HANDSHAKE && v.host && kind==1 && value==0){
                        valid=queue(wire,2,1-v.role,0);v.phase=LAN_CONNECTED;lastPing=now();
                    }else if(valid && v.phase==LAN_HANDSHAKE && !v.host && kind==2 && value<=1){v.role=value;v.phase=LAN_CONNECTED;lastPing=now();}
                    else if(valid && v.phase==LAN_CONNECTED && kind==3)valid=queue(wire,4,0,nonce);
                    else if(valid && v.phase==LAN_CONNECTED && kind==4 && nonce==pingNonce)v.rtt=now()-nonce;
                    else valid=false;
                    if(!valid){v.phase=LAN_ERROR;copy(v.message,"Incompatible lobby protocol",sizeof(v.message));}
                    else lastRx=now();
                }
            }else if(n==0 || !pending()){v.phase=LAN_ERROR;copy(v.message,"Other player disconnected",sizeof(v.message));}
        }
        unsigned tick=now();
        if((v.phase==LAN_CONNECTING || v.phase==LAN_HANDSHAKE) && tick-since>8000){v.phase=LAN_ERROR;copy(v.message,"Connection timed out (8 seconds)",sizeof(v.message));}
        if(v.phase==LAN_CONNECTED){
            if(tick-lastRx>6000){v.phase=LAN_ERROR;copy(v.message,"Other player stopped responding",sizeof(v.message));}
            else if(tick-lastPing>=1000){pingNonce=tick;lastPing=tick;if(!queue(wire,3,0,pingNonce)){v.phase=LAN_ERROR;copy(v.message,"Send queue overflow",sizeof(v.message));}}
        }
        publish(s,v);
    }
    closeSocket(listener);closeSocket(peer);
    return 0;
}
LanService* lanCreate(){
#ifdef _WIN32
    WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data))return 0;
#endif
    LanService* s=(LanService*)calloc(1,sizeof(LanService));
    if(!s){
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
#ifdef _WIN32
    InitializeCriticalSection(&s->mutex);s->thread=CreateThread(0,0,worker,s,0,0);
    if(!s->thread){DeleteCriticalSection(&s->mutex);free(s);WSACleanup();return 0;}
#else
    pthread_mutex_init(&s->mutex,0);
    if(pthread_create(&s->thread,0,worker,s)){pthread_mutex_destroy(&s->mutex);free(s);return 0;}
#endif
    return s;
}
void lanDestroy(LanService* s){if(!s)return;lock(&s->mutex);s->quit=true;unlock(&s->mutex);
#ifdef _WIN32
    WaitForSingleObject(s->thread,INFINITE);CloseHandle(s->thread);DeleteCriticalSection(&s->mutex);WSACleanup();
#else
    pthread_join(s->thread,0);pthread_mutex_destroy(&s->mutex);
#endif
    free(s);
}
static void command(LanService* s,int cmd,int role,const char* ip,unsigned short port){
    if(!s)return;
    lock(&s->mutex);s->role=role;s->port=port;copy(s->ip,ip,sizeof(s->ip));s->command=cmd;
    s->snapshot={};s->snapshot.phase=cmd==3?LAN_IDLE:LAN_STARTING;unlock(&s->mutex);
}
void lanHost(LanService* s,int role){command(s,1,role?1:0,"",0);}
void lanJoin(LanService* s,const char* ip,unsigned short port){command(s,2,0,ip,port);}
void lanStop(LanService* s){command(s,3,0,"",0);}

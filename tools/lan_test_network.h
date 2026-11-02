#pragma once
// Deterministic in-memory socket substitute for restricted build environments.
// Never compiled into the DLL. The default integration test uses real TCP.
#include <mutex>
#include <vector>
#include <deque>
#include <algorithm>
#include <thread>
#include <chrono>
namespace TestNet {
struct FD {
    bool open=true, listening=false;
    unsigned short port=0;
    int other=-1;
    std::deque<int> accepts;
    std::deque<char> bytes;
};
inline std::mutex guard;
inline std::vector<FD> fds(10);
inline unsigned short nextPort=30000;
inline bool valid(int fd){return fd>=10 && (unsigned)fd<fds.size() && fds[fd].open;}
inline int socket(int,int,int){std::lock_guard<std::mutex> g(guard);fds.emplace_back();return fds.size()-1;}
inline int close(int fd){std::lock_guard<std::mutex> g(guard);if(!valid(fd))return -1;fds[fd].open=false;return 0;}
inline int fcntl(int,int,...){return 0;}
inline int bind(int fd,const sockaddr* address,socklen_t){
    std::lock_guard<std::mutex> g(guard);if(!valid(fd))return -1;
    unsigned short p=ntohs(((const sockaddr_in*)address)->sin_port);fds[fd].port=p?p:nextPort++;return 0;
}
inline int listen(int fd,int){std::lock_guard<std::mutex> g(guard);if(!valid(fd))return -1;fds[fd].listening=true;return 0;}
inline int getsockname(int fd,sockaddr* address,socklen_t*){
    std::lock_guard<std::mutex> g(guard);if(!valid(fd))return -1;
    auto a=(sockaddr_in*)address;a->sin_family=AF_INET;a->sin_port=htons(fds[fd].port);a->sin_addr.s_addr=htonl(INADDR_LOOPBACK);return 0;
}
inline int connect(int fd,const sockaddr* address,socklen_t){
    std::lock_guard<std::mutex> g(guard);unsigned short port=ntohs(((const sockaddr_in*)address)->sin_port);
    int host=-1;
    for(unsigned i=10;i<fds.size();++i)if(fds[i].open && fds[i].listening && fds[i].port==port){host=i;break;}
    if(host<0){errno=ECONNREFUSED;return -1;}
    int server=fds.size();fds.emplace_back();fds[server].other=fd;fds[fd].other=server;fds[host].accepts.push_back(server);return 0;
}
inline int accept(int fd,sockaddr* address,socklen_t*){
    std::lock_guard<std::mutex> g(guard);
    if(!valid(fd) || fds[fd].accepts.empty()){errno=EAGAIN;return -1;}
    int peer=fds[fd].accepts.front();fds[fd].accepts.pop_front();
    ((sockaddr_in*)address)->sin_addr.s_addr=htonl(INADDR_LOOPBACK);return peer;
}
inline int send(int fd,const void* data,size_t length,int){
    std::lock_guard<std::mutex> g(guard);
    if(!valid(fd) || !valid(fds[fd].other)){errno=EPIPE;return -1;}
    // Deliberately force short writes in the transport, including multi-message queues.
    size_t count=std::min(length,(size_t)8);auto& bytes=fds[fds[fd].other].bytes;
    auto p=(const char*)data;bytes.insert(bytes.end(),p,p+count);return count;
}
inline int recv(int fd,void* data,size_t length,int){
    std::lock_guard<std::mutex> g(guard);
    if(!valid(fd)){errno=EBADF;return -1;}
    auto& state=fds[fd];auto p=(char*)data;
    if(state.bytes.empty()){if(!valid(state.other))return 0;errno=EAGAIN;return -1;}
    size_t count=std::min({length,state.bytes.size(),(size_t)3});
    for(size_t i=0;i<count;++i){p[i]=state.bytes.front();state.bytes.pop_front();}return count;
}
inline int getsockopt(int,int,int,void* value,socklen_t*){*(int*)value=0;return 0;}
inline int setsockopt(int,int,int,const void*,socklen_t){return 0;}
inline int select(int n,fd_set* reads,fd_set* writes,fd_set*,timeval*){
    // Small delay avoids a busy loop while still exposing interleaved reads/writes.
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::lock_guard<std::mutex> g(guard);int count=0;
    for(int fd=0;fd<n;++fd){
        if(FD_ISSET(fd,reads)){
            bool ready=valid(fd) && (fds[fd].listening?!fds[fd].accepts.empty():!fds[fd].bytes.empty() || !valid(fds[fd].other));
            if(!ready)FD_CLR(fd,reads);else ++count;
        }
        if(FD_ISSET(fd,writes)){if(!valid(fd))FD_CLR(fd,writes);else ++count;}
    }
    return count;
}
}
#define socket TestNet::socket
#define close TestNet::close
#define fcntl TestNet::fcntl
#define bind TestNet::bind
#define listen TestNet::listen
#define getsockname TestNet::getsockname
#define connect TestNet::connect
#define accept TestNet::accept
#define send TestNet::send
#define recv TestNet::recv
#define getsockopt TestNet::getsockopt
#define setsockopt TestNet::setsockopt
#define select TestNet::select

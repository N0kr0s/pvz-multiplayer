// Standalone C++20 LAN rules/server prototype. No game assets or executable patches.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using Socket = SOCKET;
constexpr Socket bad_socket = INVALID_SOCKET;
static void close_socket(Socket fd) { closesocket(fd); }
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket bad_socket = -1;
static void close_socket(Socket fd) { close(fd); }
#endif

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
static constexpr int rows = 5, cols = 9;

struct Plant { int id, row, col, hp; std::string kind; double fire=0, income=0; };
struct Zombie { int id, row, hp; std::string kind; double x, speed, bite=0; };
struct Building { int id, row, col, hp; double timer=0; };

static std::string quote(std::string_view in) {
    std::string out = "\"";
    for (char c : in) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else if (static_cast<unsigned char>(c) < 32) out += ' ';
        else out += c;
    }
    return out + '"';
}

static std::optional<std::string> field(const std::string& body, std::string_view name) {
    const auto key = body.find('"' + std::string(name) + '"');
    if (key == std::string::npos) return std::nullopt;
    auto pos = body.find(':', key + name.size() + 2);
    if (pos == std::string::npos) return std::nullopt;
    pos = body.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos) return std::nullopt;
    if (body[pos] == '"') {
        auto end = body.find('"', pos + 1);
        if (end == std::string::npos) return std::nullopt;
        return body.substr(pos + 1, end - pos - 1);
    }
    auto end = body.find_first_of(",}\r\n \t", pos);
    return body.substr(pos, end - pos);
}

static std::optional<int> int_field(const std::string& body, std::string_view name) {
    auto value = field(body, name);
    if (!value || value->empty()) return std::nullopt;
    char* end = nullptr;
    long n = std::strtol(value->c_str(), &end, 10);
    if (*end || n < -1000 || n > 1000) return std::nullopt;
    return static_cast<int>(n);
}

class Match {
public:
    std::string plant_token, zombie_token;
    std::string phase="waiting", winner, message="Ожидание второго игрока";
    double sun=150, brains=80, seconds=0;
    std::vector<Plant> plants;
    std::vector<Zombie> zombies;
    std::vector<Building> graves, farms, special;
    int next_id=1;
    Clock::time_point last=Clock::now();

    Match() {
        for (int row=0; row<rows; ++row) special.push_back({next_id++, row, 8, 260});
    }

    std::string join(const std::string& role) {
        std::string* slot = role == "plants" ? &plant_token : role == "zombies" ? &zombie_token : nullptr;
        if (!slot || !slot->empty()) return "";
        std::random_device random;
        std::ostringstream token;
        for (int i=0; i<6; ++i) token << std::hex << std::setw(8) << std::setfill('0') << random();
        *slot = token.str();
        if (!plant_token.empty() && !zombie_token.empty()) {
            phase="playing"; message="Матч начался"; last=Clock::now();
        }
        return *slot;
    }

    bool occupied(int row, int col) const {
        for (const auto& p : plants) if (p.row==row && p.col==col) return true;
        for (const auto* list : {&graves, &farms, &special})
            for (const auto& b : *list) if (b.row==row && b.col==col) return true;
        return false;
    }

    std::string action(const std::string& token, const std::string& kind, int row, int col) {
        tick();
        if (phase != "playing") return "Матч не начался";
        if (row<0 || row>=rows || col<0 || col>=cols) return "Неверная клетка";
        if (token == plant_token && !token.empty()) {
            if (col>6 || occupied(row,col)) return "Клетка недоступна";
            int cost=0, hp=0;
            if (kind=="peashooter") cost=100, hp=120;
            else if (kind=="sunflower") cost=50, hp=100;
            else if (kind=="wallnut") cost=50, hp=400;
            else return "Недоступное растение";
            if (sun<cost) return "Не хватает солнца";
            sun-=cost;
            plants.push_back({next_id++,row,col,hp,kind});
            return "";
        }
        if (token == zombie_token && !token.empty()) {
            if (kind=="grave" || kind=="farm") {
                if (col<6 || col>7 || occupied(row,col)) return "Клетка недоступна";
                int cost=kind=="grave" ? 65 : 90;
                if (brains<cost) return "Не хватает мозгов";
                brains-=cost;
                (kind=="grave" ? graves : farms).push_back({next_id++,row,col,kind=="grave" ? 180 : 150});
                return "";
            }
            if (col!=8) return "Зомби выходят справа";
            int cost=0, hp=0; double speed=0;
            if (kind=="basic") cost=25,hp=140,speed=.23;
            else if (kind=="cone") cost=50,hp=300,speed=.20;
            else if (kind=="bucket") cost=80,hp=540,speed=.17;
            else return "Неизвестный зомби";
            if (brains<cost) return "Не хватает мозгов";
            brains-=cost;
            zombies.push_back({next_id++,row,hp,kind,8.8,speed});
            return "";
        }
        return "Неверный игрок";
    }

    void tick() {
        auto now=Clock::now();
        double dt=std::clamp(std::chrono::duration<double>(now-last).count(),0.0,.25);
        last=now;
        if (phase!="playing") return;
        seconds+=dt;
        sun=std::min(999.0,sun+5*dt);
        brains=std::min(999.0,brains+2*farms.size()*dt);
        for (auto& p : plants) {
            if (p.kind=="sunflower") {
                p.income+=dt;
                if (p.income>=7) { p.income-=7; sun=std::min(999.0,sun+25); }
            }
            if (p.kind!="peashooter") continue;
            p.fire+=dt;
            if (p.fire<1.2) continue;
            p.fire-=1.2;
            Zombie* target=nullptr;
            for (auto& z : zombies) if (z.row==p.row && z.x>p.col && (!target || z.x<target->x)) target=&z;
            if (target) { target->hp-=24; continue; }
            Building* building=nullptr;
            for (auto* list : {&graves,&farms,&special})
                for (auto& b : *list)
                    if (b.row==p.row && b.col>p.col && (!building || b.col<building->col)) building=&b;
            if (building) building->hp-=24;
        }
        for (auto& g : graves) {
            g.timer+=dt;
            if (g.timer>=14) {
                g.timer-=14;
                zombies.push_back({next_id++,g.row,140,"basic",g.col+.3,.23});
            }
        }
        for (auto& z : zombies) {
            Plant* blocking=nullptr;
            for (auto& p : plants) if (p.row==z.row && std::abs(p.col+.6-z.x)<.5) { blocking=&p; break; }
            if (blocking) {
                z.bite+=25*dt;
                int damage=static_cast<int>(z.bite);
                z.bite-=damage;
                blocking->hp-=damage;
            }
            else z.x-=z.speed*dt;
            if (z.x<-.3) { phase="finished"; winner="zombies"; message="Зомби добрались до дома"; }
        }
        for (const auto& p : plants) if (p.hp<=0) brains=std::min(999.0,brains+35);
        auto erase_dead=[](auto& list) { std::erase_if(list,[](const auto& obj){return obj.hp<=0;}); };
        erase_dead(plants); erase_dead(zombies); erase_dead(graves); erase_dead(farms); erase_dead(special);
        if (special.empty() && phase=="playing") { phase="finished"; winner="plants"; message="Особые могилы разрушены"; }
    }

    std::string snapshot() {
        tick();
        std::ostringstream s;
        s << "{\"phase\":" << quote(phase) << ",\"winner\":" << (winner.empty()?"null":quote(winner))
          << ",\"message\":" << quote(message) << ",\"sun\":" << sun << ",\"brains\":" << brains
          << ",\"seconds\":" << seconds << ",\"slots\":{\"plants\":" << (!plant_token.empty()?"true":"false")
          << ",\"zombies\":" << (!zombie_token.empty()?"true":"false") << '}';
        auto plant_json=[](const Plant& p) {
            std::ostringstream o; o << "{\"id\":"<<p.id<<",\"row\":"<<p.row<<",\"col\":"<<p.col
                <<",\"kind\":"<<quote(p.kind)<<",\"hp\":"<<p.hp<<'}'; return o.str();
        };
        auto zombie_json=[](const Zombie& z) {
            std::ostringstream o; o << "{\"id\":"<<z.id<<",\"row\":"<<z.row<<",\"x\":"<<z.x
                <<",\"kind\":"<<quote(z.kind)<<",\"hp\":"<<z.hp<<'}'; return o.str();
        };
        auto building_json=[](const Building& b) {
            std::ostringstream o; o << "{\"id\":"<<b.id<<",\"row\":"<<b.row<<",\"col\":"<<b.col
                <<",\"hp\":"<<b.hp<<'}'; return o.str();
        };
        auto emit=[&](std::string_view name,const auto& list,auto convert) {
            s << ",\"" << name << "\":[";
            bool first=true;
            for (const auto& item : list) { if (!first) s << ','; first=false; s << convert(item); }
            s << ']';
        };
        emit("plants",plants,plant_json); emit("zombies",zombies,zombie_json);
        emit("graves",graves,building_json); emit("farms",farms,building_json);
        emit("special",special,building_json);
        return s.str()+'}';
    }
};

struct Response { int status=200; std::string type="application/json; charset=utf-8", body; };
static std::string read_file(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(file),{}};
}

static Response route(Match& match,const fs::path& root,std::string_view method,
                      std::string_view path,const std::string& body) {
    if (method=="GET" && path=="/api/state") return {200,"application/json; charset=utf-8",match.snapshot()};
    if (method=="POST" && path=="/api/join") {
        std::string token=match.join(field(body,"role").value_or(""));
        return {token.empty()?409:200,"application/json; charset=utf-8",
                token.empty()?"{\"error\":\"Место занято\"}":"{\"token\":"+quote(token)+'}'};
    }
    if (method=="POST" && path=="/api/action") {
        auto row=int_field(body,"row"), col=int_field(body,"col");
        std::string error=(!row||!col) ? "Неверная клетка" :
            match.action(field(body,"token").value_or(""),field(body,"kind").value_or(""),*row,*col);
        return {error.empty()?200:400,"application/json; charset=utf-8",
                error.empty()?"{\"ok\":true}":"{\"ok\":false,\"message\":"+quote(error)+'}'};
    }
    if (method=="OPTIONS") return {200,"application/json","{}"};
    for (const auto& [url,name,type] : std::array<std::tuple<std::string_view,std::string_view,std::string_view>,4>{
            {{"/","index.html","text/html; charset=utf-8"},
             {"/client.html","client.html","text/html; charset=utf-8"},
             {"/game.js","game.js","text/javascript; charset=utf-8"},
             {"/style.css","style.css","text/css; charset=utf-8"}}})
        if (method=="GET" && path==url) return {200,std::string(type),read_file(root/name)};
    return {404,"application/json","{\"error\":\"Not found\"}"};
}

static void serve_client(Socket client,Match& match,const fs::path& root) {
    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n")==std::string::npos && request.size()<8192) {
        int n=recv(client,buffer,sizeof(buffer),0);
        if (n<=0) return;
        request.append(buffer,n);
    }
    auto headers_end=request.find("\r\n\r\n");
    if (headers_end==std::string::npos) return;
    auto first_end=request.find("\r\n");
    std::istringstream first(request.substr(0,first_end));
    std::string method,path,version; first>>method>>path>>version;
    if (path.find('?')!=std::string::npos) path.resize(path.find('?'));
    size_t length=0;
    std::istringstream headers(request.substr(first_end+2,headers_end-first_end-2));
    std::string line;
    while (std::getline(headers,line)) {
        auto colon=line.find(':');
        if (colon!=std::string::npos && line.substr(0,colon)=="Content-Length") {
            try { length=std::stoul(line.substr(colon+1)); } catch (...) { return; }
        }
    }
    if (length>4096) return;
    std::string body=request.substr(headers_end+4);
    while (body.size()<length) {
        int n=recv(client,buffer,sizeof(buffer),0);
        if (n<=0) return;
        body.append(buffer,n);
    }
    body.resize(length);
    Response response=route(match,root,method,path,body);
    std::ostringstream head;
    head << "HTTP/1.1 "<<response.status<< (response.status==200?" OK":response.status==400?" Bad Request":response.status==409?" Conflict":" Not Found")
         << "\r\nContent-Type: "<<response.type<<"\r\nContent-Length: "<<response.body.size()
         << "\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
    std::string payload=head.str()+response.body;
    size_t sent=0;
    while (sent<payload.size()) {
        int n=send(client,payload.data()+sent,static_cast<int>(payload.size()-sent),0);
        if (n<=0) break;
        sent+=n;
    }
}

static std::string lan_ip() {
    Socket probe=socket(AF_INET,SOCK_DGRAM,0);
    if (probe==bad_socket) return "IP_ХОСТА";
    sockaddr_in remote{}; remote.sin_family=AF_INET;
    remote.sin_port=htons(80); inet_pton(AF_INET,"192.0.2.1",&remote.sin_addr);
    std::string result="IP_ХОСТА";
    if (connect(probe,reinterpret_cast<sockaddr*>(&remote),sizeof(remote))==0) {
        sockaddr_in local{};
#ifdef _WIN32
        int length=sizeof(local);
#else
        socklen_t length=sizeof(local);
#endif
        if (getsockname(probe,reinterpret_cast<sockaddr*>(&local),&length)==0) {
            char buffer[INET_ADDRSTRLEN]{};
            if (inet_ntop(AF_INET,&local.sin_addr,buffer,sizeof(buffer))) result=buffer;
        }
    }
    close_socket(probe);
    return result;
}

int main(int argc,char** argv) {
    if (argc==2 && std::string_view(argv[1])=="--self-test") {
        Match match;
        auto plants=match.join("plants"), zombies=match.join("zombies");
        if (plants.empty() || zombies.empty() || match.phase!="playing") return 1;
        if (!match.action(zombies,"peashooter",0,0).size()) return 2;
        if (!match.action(plants,"peashooter",0,0).empty()) return 3;
        if (match.sun<50 || match.sun>50.1 || match.action(plants,"peashooter",0,0).empty()) return 4;
        if (!match.action(zombies,"grave",0,7).empty()) return 5;
        if (match.graves.size()!=1 || match.action(zombies,"farm",0,7).empty()) return 6;
        match.special.clear(); match.last-=std::chrono::milliseconds(100);
        if (match.snapshot().find("\"winner\":\"plants\"")==std::string::npos) return 7;
        Match http;
        fs::path root=fs::absolute(argv[0]).parent_path();
        if (route(http,root,"GET","/"," ").body.find("Multiplayer")==std::string::npos) return 8;
        auto plant_join=route(http,root,"POST","/api/join","{\"role\":\"plants\"}");
        auto zombie_join=route(http,root,"POST","/api/join","{\"role\":\"zombies\"}");
        if (plant_join.status!=200 || zombie_join.status!=200 ||
            route(http,root,"GET","/api/state","").body.find("\"phase\":\"playing\"")==std::string::npos) return 9;
        auto token=field(plant_join.body,"token").value_or("");
        if (route(http,root,"POST","/api/action",
            "{\"token\":"+quote(token)+",\"kind\":\"peashooter\",\"row\":0,\"col\":0}").status!=200) return 10;
        std::cout << "C++ self-test OK\n";
        return 0;
    }
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2),&wsa)!=0) return 1;
#endif
    int port=0;
    if (argc==3 && std::string_view(argv[1])=="--port") {
        try { port=std::stoi(argv[2]); } catch (...) { port=-1; }
    }
    if (port<0 || port>65535 || (argc!=1 && argc!=3)) {
        std::cerr << "Usage: pvzmp [--port 0..65535]\n"; return 1;
    }
    Socket server=socket(AF_INET,SOCK_STREAM,0);
    if (server==bad_socket) { std::cerr<<"socket failed\n"; return 1; }
    int reuse=1;
    setsockopt(server,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY); addr.sin_port=htons(port);
    if (bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0 || listen(server,8)!=0) {
        std::cerr<<"bind/listen failed\n"; close_socket(server); return 1;
    }
#ifdef _WIN32
    int addr_len=sizeof(addr);
#else
    socklen_t addr_len=sizeof(addr);
#endif
    getsockname(server,reinterpret_cast<sockaddr*>(&addr),&addr_len);
    std::cout << "Хост: http://127.0.0.1:"<<ntohs(addr.sin_port)<<"/\n"
              << "Клиент: http://"<<lan_ip()<<':'<<ntohs(addr.sin_port)<<"/\n"<<std::flush;
    fs::path root=fs::absolute(argv[0]).parent_path();
    Match match;
    for (;;) {
        Socket client=accept(server,nullptr,nullptr);
        if (client==bad_socket) continue;
        serve_client(client,match,root);
        close_socket(client);
    }
}

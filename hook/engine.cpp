// LAN extension of the original PvZ Board, chooser, object updates and sounds.
#include <windows.h>
#include <string.h>
#include "engine.h"
#include "reconcile.h"
#include "native-layout.h"
#include "road-motion.h"
#include "road-layout.h"
template<class T> static T& at(void* p,unsigned offset){return *(T*)((BYTE*)p+offset);}
extern "C" {
void pvzMakeBoard(void*);void pvzInitBoard(void*);void pvzStartPlaying(void*);void pvzKillSelector(void*);void pvzShowMenu(void*);
void* pvzAddPlant(void*,int,int,int);void* pvzAddZombie(void*,int,int);void* pvzAddShot(void*,int,int,int);
void pvzUpdateEffects(void*);void pvzUpdateLayers(void*);void pvzBottomGate();
void pvzRoadWalk(void*);void pvzRoadAnimRate(void*,float);
void* pvzAddRoadVisual(void*,int,int);void pvzRoadReanim(void*);void pvzRoadUpdateGate();void pvzRoadDrawGate();
void pvzWalkIntoHouse(void*);
void pvzSunSound(void*);void pvzStartMower(void*);void pvzMowGate();
void pvzZombieDeathAnim(void*);void pvzZombieDamageStates(void*);void pvzZombieDeathGate();void pvzPortraitGate();
void pvzDeleteQueue(void*);void pvzPlantDie(void*);void pvzZombieDie(void*);
void pvzPreloadPlant(int);void pvzPreloadZombie(int);
void pvzNewGame(void*);void pvzKillChooser(void*);void pvzCloseChooser(void*);
int pvzCanPlant(void*,int,int,int);int pvzSeedCost(int);
void* pvzAddCoin(void*,int,int,int,int);void pvzCollectCoin(void*);
void pvzSeedAvailableGate();void pvzSeedCountGate();void pvzChooserCardGate();void pvzChooserStartGate();
int pvzGridX(void*,int,int);int pvzGridY(void*,int,int);
}
using Update=void (__thiscall*)(void*);
using Draw=void (__thiscall*)(void*,Graphics*);
static Update originalUpdate,originalLostFocus;
static Draw originalDraw,originalChooserDraw;
static LanService* service;
static void (*logLine)(const char*);
static void* board;
static bool active=false,running=false,startRequest=false,leaveRequest=false,available=false;
static bool polling=false,localPrepared=false,peerPrepared=false;
static NativeCatalog::Deck decks[2];
static unsigned readyAt[2][NativeCatalog::MaxDeck]={};
static bool authority=false;
static bool introClosed=false,introDone=false,peerIntroDone=false;
static unsigned introAt=0,goAt=0;
extern void engineSetReadyLabel(void*,const char*);
static unsigned observedZombies[1024]={};
static bool observedDying[1024]={};
static bool dying(void* p){int phase=at<int>(p,0x28);return at<int>(p,0xc8)<=0 || (phase>=1 && phase<=3);}
int engineTypeCost(int type){int extra=NativeCatalog::extraCost(type);return extra>=0?extra:pvzSeedCost(type);}
static int selected=-1,cardPressed=-1;
static unsigned callbackDepth=0;
struct CallbackGuard {CallbackGuard(){++callbackDepth;}~CallbackGuard(){--callbackDepth;}};
struct PollGuard {PollGuard(){polling=true;}~PollGuard(){polling=false;}};
static unsigned resultAt=0,houseInfo=0,houseID=0;
static bool resultPressed=false;
static int savedMode=0,savedResult=0,localRole=0,winner=0;
static unsigned matchId=0,startedAt=0,lastPublish=0,lastIncome=0,lastCommand[2]={},requestSeq=0,remoteSeq=0;
static char status[128]="";
static MatchUI::Cell hoverCell={},pressedCell={};
static bool menuHover=false,menuPressed=false;
static Match::StateData state={};
struct Mirror {unsigned hostId,localId;void* object;Reconcile::Error error={};bool aligned=false,terminal=false;};
struct SunDraw {unsigned id,time;float tx,ty;};
static SunDraw sunDraws[1024]={};
static Mirror plants[Match::MaxPlants],zombies[Match::MaxZombies],coins[Match::MaxCoins];
struct RoadUnit {unsigned id=0,localId=0;void* visual=0;int unit=0,row=0;float x=0,speed=0.6f;};
static RoadUnit road[48];static unsigned roadSequence=0;
static bool roadView=false,arrowPressed=false;static float cameraFrom=0;static unsigned cameraAt=0;
static bool roadObject(void* object){for(const RoadUnit& u:road)if(u.id && u.visual==object && at<unsigned>(object,0x158)==u.localId)return true;return false;}
float engineCameraOffset(){float t=(GetTickCount()-cameraAt)/500.0f;if(t>1)t=1;t=t*t*(3-2*t);return cameraFrom+((roadView?1067.0f:0.0f)-cameraFrom)*t;}
bool engineRoadView(){return roadView;}
unsigned engineRoadCount(){return 48;}
void* engineRoadVisual(unsigned i,float* x,int* row){if(i>=48 || !road[i].id)return 0;*x=road[i].x;*row=road[i].row;return road[i].visual;}
extern "C" int engineUpdateRoadObject(void* p){if(!active || !roadObject(p))return 0;return 1;}
extern "C" int engineHideRoadObject(void* p){return active && roadObject(p);}
static RoadUnit* roadFind(unsigned id){for(RoadUnit& u:road)if(u.id==id)return &u;return 0;}
static RoadUnit* roadAdd(unsigned id,unsigned action){
    if(RoadUnit* u=roadFind(id))return u;
    int row,col;if(!Match::parseAction(action,1,row,col))return 0;
    for(RoadUnit& u:road)if(!u.id){
        int type=NativeCatalog::zombieType(action>>24);void* p=pvzAddRoadVisual(board,row,type);if(!p)return 0;
        float speed=RoadMotion::speed(type);pvzRoadWalk(p);at<float>(p,0x34)=speed;pvzRoadAnimRate(p,type==7?18.0f:12.0f);
        u={id,at<unsigned>(p,0x158),p,(int)(action>>24),row,RoadLayout::spawnX(col),speed};
        at<float>(p,0x84)=type==16?25.0f:0.0f;at<int>(p,0x64)=9;at<float>(p,0x2c)=3000;at<int>(p,8)=3000;at<bool>(p,0x18)=false;return &u;
    }
    return 0;
}
static void roadRemove(RoadUnit& u){if(u.visual && at<unsigned>(u.visual,0x158)==u.localId)pvzZombieDie(u.visual);u={};}
static void roadUpdate(){
    for(RoadUnit& u:road)if(u.id){u.x-=u.speed;pvzRoadReanim(u.visual);
        if(authority && u.x<=RoadLayout::transferX(u.row)){
            void* p=pvzAddZombie(board,u.row,NativeCatalog::zombieType(u.unit));if(!p){leaveRequest=true;return;}
            lanSendEvent(service,Match::RoadDone,matchId,u.id);
            lanSendEvent(service,Match::Spawn,Match::action(Match::Zombie,u.row,8,u.unit),at<unsigned>(p,0x158));roadRemove(u);
        }else if(u.x<RoadLayout::transferX(u.row))u.x=RoadLayout::transferX(u.row);
    }
}
static void roadPublish(){for(const RoadUnit& u:road)if(u.id)lanSendEvent(service,Match::RoadPosition,(unsigned)(u.x*1000),u.id);}
static void* app(){return *(void**)0x6a9ec0;}
static void say(const char* s){lstrcpynA(status,s,sizeof(status));if(logLine){logLine(s);logLine("\r\n");}}
static bool slot(DWORD table,unsigned index,DWORD expected,void* replacement){
    DWORD* p=(DWORD*)(table+index*4);if(*p!=expected)return false;
    DWORD old;if(!VirtualProtect(p,4,PAGE_READWRITE,&old))return false;
    InterlockedExchange((LONG*)p,(LONG)replacement);DWORD unused;VirtualProtect(p,4,old,&unused);return true;
}
static unsigned arrayOffset(int kind){return kind==0?0xac:kind==1?0x90:kind==2?0xc8:0xe4;}
static unsigned stride(int kind){return kind==0?0x14c:kind==1?0x15c:kind==2?0x94:NativeLayout::CoinStride;}
static unsigned deadOffset(int kind){return kind==0?0x141:kind==1?0xec:kind==2?0x50:0x38;}
static bool exists(void* object,int kind,unsigned id){
    return object && at<unsigned>(object,NativeLayout::idOffset(kind))==id && (id&0xffff0000) && !at<bool>(object,deadOffset(kind));
}
static unsigned enumerate(int kind,void** result,unsigned cap){
    if(!board)return 0;unsigned off=arrayOffset(kind),step=stride(kind),count=0,used=at<unsigned>(board,off+4);
    if(used>1024)return 0;BYTE* base=at<BYTE*>(board,off);
    for(unsigned i=0;i<used && count<cap;++i){void* p=base+i*step;
        if((at<unsigned>(p,NativeLayout::idOffset(kind))&0xffff0000) && !at<bool>(p,deadOffset(kind)) && (kind!=1 || !roadObject(p)) && (kind!=3 || (at<int>(p,0x58)>=4 && at<int>(p,0x58)<=6)))result[count++]=p;
    }
    return count;
}
static void removeObject(void* p,int kind){
    if(kind==0)pvzPlantDie(p);else if(kind==1)pvzZombieDie(p);else at<bool>(p,deadOffset(kind))=true;
}
static unsigned limit(int kind){return kind==0?Match::MaxPlants:kind==1?Match::MaxZombies:kind==2?Match::MaxShots:Match::MaxCoins;}
static Mirror* mapping(int kind){return kind==0?plants:kind==1?zombies:coins;}
static void capture(){
    void* objects[1024];state.match=matchId;state.tick=at<unsigned>(board,0x5568);state.winner=winner;
    state.sun=at<int>(board,0x5560);if(state.sun>9990)state.sun=at<int>(board,0x5560)=9990;
    // Projectiles are native, locally simulated effects, not streamed objects.
    state.shots=0;
    for(int kind=0;kind<4;++kind){if(kind==2)continue;
        unsigned cap=limit(kind),n=enumerate(kind,objects,1024);
        for(unsigned i=cap;i<n;++i)removeObject(objects[i],kind);
        if(n>cap)n=cap;
        Match::Entity* out=kind==0?state.plant:kind==1?state.zombie:state.coin;
        if(kind==0)state.plants=n;else if(kind==1)state.zombies=n;else state.coins=n;
        for(unsigned i=0;i<n;++i){void* p=objects[i];
            int x=kind==0?at<int>(p,8)*1000:(int)(at<float>(p,kind==1?0x2c:0x24)*1000);
            int y=kind==0?at<int>(p,12)*1000:(int)(at<float>(p,kind==1?0x30:0x28)*1000);
            int hp=kind==0?at<int>(p,0x40):kind==1?at<int>(p,0xc8):0;if(hp<0)hp=0;
            int row=kind==3?(int)(at<float>(p,0x34)*1000):at<int>(p,0x1c),col=kind==0?at<int>(p,0x28):kind==3?at<int>(p,0x4c):0;
            int unit=at<int>(p,kind==3?0x58:0x24);
            int armor=kind==1?at<int>(p,0xd0):kind==3?(int)(at<float>(p,0x2c)*1000):0,shield=kind==1?at<int>(p,0xdc):kind==3?(int)(at<float>(p,0x30)*1000):0;
            out[i]={at<unsigned>(p,NativeLayout::idOffset(kind)),row,col,hp,x,y,
                kind==0?at<int>(p,0x3c):kind==1?at<int>(p,0x28):at<int>(p,0x48),
                kind==1?(at<bool>(p,0x51)?1:0):kind==3?((at<bool>(p,0x50)?1:0)|(at<int>(p,0x5c)<<1)):0,
                unit,kind!=3 && armor<0?0:armor,kind!=3 && shield<0?0:shield};
            if(kind==1){if(dying(p))out[i].flags|=2;for(unsigned j=0;j<5;++j)out[i].links[j]=at<unsigned>(p,0xf0+j*4);
                out[i].height=at<int>(p,0x64);out[i].altitude=(int)(at<float>(p,0x84)*1000);out[i].verticalSpeed=(int)(at<float>(p,0x120)*1000);out[i].phaseTicks=at<int>(p,0x68);if(out[i].phaseTicks<0)out[i].phaseTicks=0;}
        }
    }
}
static Mirror* findMap(int kind,unsigned id){
    Mirror* maps=mapping(kind);for(unsigned i=0;i<limit(kind);++i)if(maps[i].hostId==id)return &maps[i];return 0;
}
extern "C" int engineAllowZombieDeath(void* p){
    if(!active || !running || authority || at<void*>(p,4)!=board)return 1;
    for(Mirror& m:zombies)if(m.hostId && m.object==p && m.localId==at<unsigned>(p,0x158)){
        if(m.terminal)return 1;
        // Native movement/combat keeps predicting, but lethal transitions wait
        // for the host. A repair packet cannot resurrect a finished corpse.
        if(at<int>(p,0xc8)<=0)at<int>(p,0xc8)=1;
        return 0;
    }
    return 1;
}
static Mirror* addMirror(int kind,unsigned id,int row,int col,int unit,int x=0,int y=0){
    Mirror* maps=mapping(kind);Mirror* map=findMap(kind,id);if(map && (map->terminal || exists(map->object,kind,map->localId)))return map;
    if(!map)for(unsigned i=0;i<limit(kind);++i)if(!maps[i].hostId){map=&maps[i];break;}
    if(!map)return 0;
    // Adopt native children (dancers, imps, bobsled followers) before allocating.
    // This keeps the engine-created parent/child objects and their animations.
    void* adopted=0;
    if(kind==1){void* candidates[1024];unsigned count=enumerate(1,candidates,1024);float nearest=160.0f;
        for(unsigned i=0;i<count;++i){void* candidate=candidates[i];
            if(at<int>(candidate,0x24)!=unit || at<int>(candidate,0x1c)!=row || dying(candidate))continue;
            bool tracked=false;for(unsigned j=0;j<limit(1);++j)if(zombies[j].hostId && zombies[j].localId==at<unsigned>(candidate,0x158))tracked=true;
            float distance=at<float>(candidate,0x2c)-x;if(distance<0)distance=-distance;
            if(!tracked && distance<nearest){adopted=candidate;nearest=distance;}
        }
    }
    void* p=adopted?adopted:kind==0?pvzAddPlant(board,col,row,unit):kind==1?pvzAddZombie(board,row,unit):pvzAddCoin(board,x,y,unit,2);
    if(!p)return 0;*map={id,at<unsigned>(p,NativeLayout::idOffset(kind)),p};return map;
}
static void collectLocal(void* coin){if(!at<bool>(coin,0x50)){if(!localRole)pvzSunSound(coin);pvzCollectCoin(coin);}}
extern "C" int engineAllowMower(void* mower){return !active || at<void*>(mower,4)!=board || authority;}
static void* mowerInRow(unsigned row){
    BYTE* base=at<BYTE*>(board,0x100);unsigned count=at<unsigned>(board,0x104);if(count>32)return 0;
    for(unsigned i=0;i<count;++i){void* m=base+i*0x48;
        if((at<unsigned>(m,0x44)&0xffff0000) && !at<bool>(m,0x30) && at<unsigned>(m,0x14)==row)return m;}
    return 0;
}
static void publishMowers(){
    for(unsigned row=0;row<5;++row){void* m=mowerInRow(row);
        lanSendEvent(service,Match::Mower,row|((m?at<unsigned>(m,0x2c):4)<<8),m?(unsigned)(int)(at<float>(m,8)*1000):0);}
}
static void playLoss(void* zombie){
    if(!zombie){zombie=pvzAddZombie(board,(houseInfo>>8)&255,houseInfo&255);}
    if(zombie){at<float>(zombie,0x2c)=(float)(short)(houseInfo>>16);at<int>(zombie,8)=(short)(houseInfo>>16);pvzWalkIntoHouse(zombie);}
    // Keep SCENE_PLAYING. The survival GameOverDialog / Try Again path must
    // never be entered for a LAN board: it owns a single-player restart.
    winner=state.winner=2;resultAt=GetTickCount();resultPressed=false;
}
static void __stdcall zombiesWon(void* self,void* zombie){
    if(!active || self!=board){((void (__stdcall*)(void*,void*))0x413400)(self,zombie);return;}
    if(!authority || !running || winner)return;
    houseID=at<unsigned>(zombie,0x158);
    houseInfo=(unsigned)at<int>(zombie,0x24)|((unsigned)at<int>(zombie,0x1c)<<8)|((unsigned)(unsigned short)at<int>(zombie,8)<<16);
    lanSendEvent(service,Match::Home,houseInfo,houseID);
    lanSendEvent(service,Match::Finish,houseID,matchId);playLoss(zombie);
}
static bool mirror(const Match::StateData& received){
    for(int kind=0;kind<4;++kind){if(kind==2 || (kind==3 && localRole==1))continue;
        Mirror* maps=mapping(kind);
        const Match::Entity* data=kind==0?received.plant:kind==1?received.zombie:received.coin;
        unsigned cap=limit(kind),n=kind==0?received.plants:kind==1?received.zombies:received.coins;
        for(unsigned i=0;i<cap;++i)if(maps[i].hostId){
            bool keep=false;for(unsigned j=0;j<n;++j)if(maps[i].hostId==data[j].id)keep=true;
            if(!keep){if(exists(maps[i].object,kind,maps[i].localId))removeObject(maps[i].object,kind);maps[i]={};}
        }
        pvzDeleteQueue(board);
        for(unsigned i=0;i<n;++i){const Match::Entity& e=data[i];
            if(kind==1 && (e.flags&2)){
                Mirror* dead=findMap(1,e.id);
                if(!dead)for(unsigned j=0;j<cap;++j)if(!maps[j].hostId){dead=&maps[j];dead->hostId=e.id;break;}
                if(!dead)return false;
                if(!dead->terminal){dead->terminal=true;if(exists(dead->object,1,dead->localId)){at<int>(dead->object,0xc8)=0;pvzZombieDeathAnim(dead->object);}}
                continue;
            }
            Mirror* map=addMirror(kind,e.id,e.row,e.col,e.unit,e.x/1000,e.y/1000);
            if(!map)return false;void* p=map->object;
            if(map->terminal)continue;
            if(kind==0)at<int>(p,0x40)=e.hp;
            else if(kind==1){
                // Local native AI handles eating, firing, sounds and animation transitions.
                // Periodic correction repairs position/health; it never restarts animations.
                float x=e.x/1000.0f,y=e.y/1000.0f;
                bool lane=e.row!=at<int>(p,0x1c);
                Reconcile::Mode mode=Reconcile::plan(x-at<float>(p,0x2c),y-at<float>(p,0x30),!map->aligned,lane,map->error);
                if(lane){at<int>(p,0x20)+=(e.row-at<int>(p,0x1c))*10000;at<int>(p,0x1c)=e.row;}
                if(mode==Reconcile::Snap){at<float>(p,0x2c)=x;at<float>(p,0x30)=y;}
                float altitude=e.altitude/1000.0f,error=altitude-at<float>(p,0x84);
                if(!map->aligned || at<int>(p,0x64)!=e.height || error>30 || error< -30){
                    at<int>(p,0x64)=e.height;at<float>(p,0x84)=altitude;at<float>(p,0x120)=e.verticalSpeed/1000.0f;
                }
                if(!map->aligned && at<int>(p,0x28)==e.phase)at<int>(p,0x68)=e.phaseTicks;
                map->aligned=true;
                at<int>(p,0xc8)=e.hp;at<int>(p,0xd0)=e.armor;at<int>(p,0xdc)=e.shield;pvzZombieDamageStates(p);
            }else {
                if((e.flags&1) && !at<bool>(p,0x50))collectLocal(p);
                if(!at<bool>(p,0x50)){
                    at<float>(p,0x24)=e.x/1000.0f;at<float>(p,0x28)=e.y/1000.0f;
                    at<float>(p,0x2c)=e.armor/1000.0f;at<float>(p,0x30)=e.shield/1000.0f;
                    at<float>(p,0x34)=e.row/1000.0f;at<int>(p,0x5c)=e.flags>>1;
                    at<int>(p,0x48)=e.phase;at<int>(p,0x4c)=e.col;
                }
            }
        }
    }
    // Host IDs must never be written directly into a local native DataArray.
    for(unsigned i=0;i<received.zombies;++i){const Match::Entity& e=received.zombie[i];Mirror* m=findMap(1,e.id);
        if(!m || !exists(m->object,1,m->localId))continue;
        for(unsigned j=0;j<5;++j){Mirror* target=findMap(1,e.links[j]);
            if(!e.links[j])at<unsigned>(m->object,0xf0+j*4)=0;
            else if(target && exists(target->object,1,target->localId))at<unsigned>(m->object,0xf0+j*4)=target->localId;
        }
    }
    state=received;winner=received.winner;at<int>(board,0x5560)=state.sun;return true;
}
static void reconcileZombies(){
    for(unsigned i=0;i<Match::MaxZombies;++i){Mirror& m=zombies[i];
        if(!m.hostId || !exists(m.object,1,m.localId))continue;
        Reconcile::Error delta=Reconcile::step(m.error);
        at<float>(m.object,0x2c)+=delta.x;at<float>(m.object,0x30)+=delta.y;
    }
}
bool engineCoinVisible(void* coin){
    if(active && roadView)return false;
    return !active || at<void*>(coin,4)!=board || !localRole || at<int>(coin,0x58)<4 || at<int>(coin,0x58)>6;
}
void engineRecordCoinDraw(void* coin,float tx,float ty){
    if(!active || at<void*>(coin,4)!=board || localRole || at<int>(coin,0x58)<4 || at<int>(coin,0x58)>6)return;
    unsigned id=at<unsigned>(coin,NativeLayout::CoinID),index=id&65535;
    if(index<1024)sunDraws[index]={id,GetTickCount(),tx,ty};
}
static bool sunHit(void* coin,int x,int y){
    unsigned id=at<unsigned>(coin,NativeLayout::CoinID),index=id&65535;
    float tx=(float)at<int>(board,0x30),ty=(float)at<int>(board,0x34);
    if(index<1024 && sunDraws[index].id==id && GetTickCount()-sunDraws[index].time<500){tx=sunDraws[index].tx;ty=sunDraws[index].ty;}
    return MatchUI::inside(x,y,(int)(tx+at<float>(coin,0x24))-8,(int)(ty+at<float>(coin,0x28))-8,76,76);
}
static void removePredictedCoins(){
    void* objects[1024];unsigned n=enumerate(3,objects,1024);
    for(unsigned i=0;i<n;++i){bool tracked=false;
        for(unsigned j=0;j<Match::MaxCoins;++j)if(coins[j].hostId && coins[j].object==objects[i] && coins[j].localId==at<unsigned>(objects[i],NativeLayout::CoinID))tracked=true;
        if(!tracked)removeObject(objects[i],3);
    }
}
static bool begin(unsigned id){
    if(active || !available || !app() || at<int>(app(),0x7fc)!=1)return false;
    LanSnapshot connection=lanSnapshot(service);if(connection.phase!=LAN_CONNECTED)return false;
    authority=connection.host;savedMode=at<int>(app(),0x7f8);savedResult=at<int>(app(),0x888);localRole=connection.role;matchId=id;
    state={};state.match=id;state.sun=300;state.brains=200;winner=0;for(RoadUnit& u:road)u={};roadSequence=0;roadView=false;cameraFrom=0;cameraAt=GetTickCount();resultAt=houseInfo=houseID=0;resultPressed=false;
    memset(plants,0,sizeof(plants));memset(zombies,0,sizeof(zombies));memset(coins,0,sizeof(coins));
    hoverCell={};pressedCell={};menuHover=menuPressed=false;memset(sunDraws,0,sizeof(sunDraws));
    lastCommand[0]=lastCommand[1]=0;requestSeq=remoteSeq=0;
    at<int>(app(),0x888)=0;
    at<int>(app(),0x7f8)=1; // Day survival supplies a flat five-lane lawn, independent of profile progress.
    localPrepared=peerPrepared=false;introClosed=introDone=peerIntroDone=false;introAt=goAt=0;memset(observedZombies,0,sizeof(observedZombies));memset(observedDying,0,sizeof(observedDying));decks[0]={};decks[1]={};memset(readyAt,0,sizeof(readyAt));selected=cardPressed=-1;
    // NewGame preserves the engine's chooser, lawn camera, preloading, music and Ready/Set/Plant.
    active=true;running=false;
    say("Native preparation: starting NewGame and stock seed chooser.");
    pvzKillSelector(app());pvzNewGame(app());board=at<void*>(app(),0x768);
    if(!board){active=false;at<int>(app(),0x7f8)=savedMode;at<int>(app(),0x888)=savedResult;return false;}
    void* bank=at<void*>(board,0x144);if(bank)at<int>(bank,0x24)=NativeCatalog::deckSize(localRole);
    void* chooser=at<void*>(app(),0x774);
    if(chooser){
        at<int>(chooser,0xd18)=NativeCatalog::deckSize(localRole);
        for(int i=0;i<49;++i)if(!NativeCatalog::card(localRole,NativeCatalog::chooserCard(localRole,i)))at<int>((BYTE*)chooser+0xa4+i*0x3c,0x24)=4;
    }
    // Preload both armies locally, even on a fresh Windows profile. The stock
    // survival preloader otherwise considers only that profile's unlocked seeds.
    for(int type=0;type<40;++type)if(NativeCatalog::plant(type))pvzPreloadPlant(type);
    for(int i=0;i<NativeCatalog::ZombieCount;++i)pvzPreloadZombie(NativeCatalog::ZombieTypes[i]);
    at<int>(board,0x5748)=0;at<bool>(board,0x164)=false;
    active=true;running=false;startedAt=lastPublish=GetTickCount();lastIncome=0;
    say("Native chooser opened; waiting for both loadouts.");return true;
}
static void end(){
    if(!active)return;
    running=false;pvzKillChooser(app());pvzShowMenu(app());
    active=false;board=0;at<int>(app(),0x7f8)=savedMode;at<int>(app(),0x888)=savedResult;
    say("Returned to the menu without saving the LAN board.");
}
static void markUsed(int role,int index,int type){
    unsigned refresh=role?0:at<unsigned>((void*)0x69f2b0,type*36+0x14);
    readyAt[role][index]=at<unsigned>(board,0x5568)+refresh;
}
static void command(unsigned action,int role,unsigned sequence){
    if(!active || !running || winner)return;
    if(role!=localRole){if(sequence<=remoteSeq)return;remoteSeq=sequence;}
    int row,col;if(!Match::parseAction(action,role,row,col))return;
    int unit=action>>24,index=decks[role].find(unit);if(index<0 || state.tick<readyAt[role][index])return;
    unsigned tick=GetTickCount();if(lastCommand[role] && tick-lastCommand[role]<200)return;
    void* object=0;
    int cost=engineTypeCost(unit);
    if(!role){
        void* objects[1024];if(enumerate(0,objects,1024)>=Match::MaxPlants || at<int>(board,0x5560)<cost || pvzCanPlant(board,col,row,unit)!=0)return;
        object=pvzAddPlant(board,col,row,unit);if(!object)return;
        at<int>(board,0x5560)-=cost;state.sun=at<int>(board,0x5560);
    }else{
        void* objects[1024];if(enumerate(1,objects,1024)>=Match::MaxZombies || state.brains<cost)return;
        unsigned id=++roadSequence;RoadUnit* u=roadAdd(id,action);if(!u)return;
        state.brains-=cost;markUsed(role,index,unit);lastCommand[role]=tick;
        if(!lanSendEvent(service,Match::RoadSpawn,action,id))leaveRequest=true;
        return;
    }
    markUsed(role,index,unit);lastCommand[role]=tick;
    lanSendEvent(service,Match::Spawn,action,at<unsigned>(object,NativeLayout::idOffset(role)));
    capture();lanPublishState(service,state);lastPublish=tick;
}
static void collect(unsigned id,int role){
    if(!active || !running || winner || role)return;
    void* objects[1024];unsigned n=enumerate(3,objects,1024);
    for(unsigned i=0;i<n;++i)if(at<unsigned>(objects[i],NativeLayout::CoinID)==id && !at<bool>(objects[i],0x50)){
        collectLocal(objects[i]);lanSendEvent(service,Match::Collected,id,matchId);return;
    }
}
static void __fastcall update(void* self,void*){
    CallbackGuard guard;
    if(!active || self!=board){originalUpdate(self);return;}
    LanSnapshot connection=lanSnapshot(service);
    if(connection.phase!=LAN_CONNECTED){leaveRequest=true;return;}
    at<int>(self,0x5748)=0;at<bool>(self,0x164)=false;
    // Keep only the autonomous wave spawner disabled. Native sun spawning, plant AI,
    // zombie AI, projectiles, effects, animation and sounds run on BOTH computers.
    at<int>(self,0x559c)=100000;at<int>(self,0x5584)=0;
    if(!running && at<int>(app(),0x7fc)==3){
        void* effects=at<void*>(app(),0x820);if(effects)pvzUpdateEffects(effects);
        pvzUpdateLayers(self);return;
    }
    if(winner){if(GetTickCount()-resultAt<3500)originalUpdate(self);else {pvzUpdateEffects(at<void*>(app(),0x820));pvzUpdateLayers(self);}return;}
    originalUpdate(self);
    if(!running)return;
    roadUpdate();
    if(!connection.host){reconcileZombies();removePredictedCoins();pvzDeleteQueue(self);return;}
    unsigned tick=GetTickCount();state.tick=at<unsigned>(self,0x5568);
    // Reliable per-frame death edges complement the slower repair snapshots.
    for(unsigned i=0;i<1024;++i)if(observedZombies[i]){
        BYTE* base=at<BYTE*>(board,0x90);void* p=base+i*0x15c;
        bool present=exists(p,1,observedZombies[i]);
        if(!present || (!observedDying[i] && dying(p))){
            if(!observedDying[i])lanSendEvent(service,Match::Death,present?1:4,observedZombies[i]);
            observedDying[i]=true;if(!present)observedZombies[i]=0;
        }
    }
    void* seen[1024];unsigned count=enumerate(1,seen,1024);
    for(unsigned i=0;i<count;++i){unsigned id=at<unsigned>(seen[i],0x158),index=id&65535;
        if(index>=1024)continue;
        if(observedZombies[index]!=id){observedZombies[index]=id;observedDying[index]=dying(seen[i]);
            if(observedDying[index])lanSendEvent(service,Match::Death,1,id);
        }
    }
    if(state.tick-lastIncome>=500){lastIncome=state.tick;if(state.brains<9750)state.brains+=25;}
    if(tick-lastPublish>=250 || winner){lastPublish=tick;roadPublish();publishMowers();capture();if(!lanPublishState(service,state)){say("Cannot publish native board state.");leaveRequest=true;}}
}
extern void engineDrawWorld(void*,Graphics*,void*);
static void __fastcall draw(void* self,void*,Graphics* g){CallbackGuard guard;if(active && self==board && at<int>(app(),0x7fc)==3 && !winner)engineDrawWorld(self,g,(void*)originalDraw);else originalDraw(self,g);if(active && self==board)engineDraw(g);}
static void __fastcall lostFocus(void* self,void*){
    if(!active)originalLostFocus(self); // LAN input has its own exit; avoid an inaccessible native pause modal.
}
static bool lostFocusSlot(){
    // Find the verified LawnApp virtual override rather than assuming a slot number.
    DWORD* table=*(DWORD**)app();
    for(unsigned i=0;i<160;++i)if(table[i]==0x44f460){
        originalLostFocus=(Update)0x44f460;
        return slot((DWORD)table,i,0x44f460,(void*)lostFocus);
    }
    return false;
}
static void __stdcall saveBoard(void* self){
    if(active && self==board){if(logLine)logLine("LAN board save suppressed.\r\n");return;}
    ((void (__stdcall*)(void*))0x408c30)(self);
}
static bool protectSaves(){
    const DWORD calls[]={0x40c03f,0x44ee55,0x44f25d,0x45333f};
    for(DWORD call:calls)if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x408c30)return false;
    for(DWORD call:calls){
        DWORD old;if(!VirtualProtect((void*)call,5,PAGE_EXECUTE_READWRITE,&old))return false;
        *(DWORD*)(call+1)=(DWORD)saveBoard-call-5;
        FlushInstructionCache(GetCurrentProcess(),(void*)call,5);
        DWORD unused;VirtualProtect((void*)call,5,old,&unused);
    }
    return true;
}
extern "C" int engineHideNativeUI(void* self){return active && self==board && at<int>(app(),0x7fc)==3;}
extern "C" int engineSeedAvailable(int type){
    if(!active)return -1;
    int card=NativeCatalog::chooserCard(localRole,type);return card>=0 && NativeCatalog::card(localRole,card)?1:0;
}
extern "C" int engineSeedCount(){return active?49:-1;}
extern "C" int engineMapChooserCard(int type){return active?NativeCatalog::chooserCard(localRole,type):type;}
extern "C" int engineAcceptChooser(void* self){
    if(!active)return 0;
    unsigned expected=NativeCatalog::deckSize(localRole);
    if(self!=at<void*>(app(),0x774) || at<int>(self,0xd20)!=0 || at<unsigned>(self,0xd24)!=expected)return 1;
    NativeCatalog::Deck deck;
    for(int i=0;i<49;++i){void* chosen=(BYTE*)self+0xa4+i*0x3c;
        if(at<int>(chosen,0x24)==1 && !deck.set(localRole,at<unsigned>(chosen,0x28),expected,NativeCatalog::chooserCard(localRole,i)))return 1;
    }
    if(deck.complete() && !localPrepared){
        decks[localRole]=deck;localPrepared=true;
        for(unsigned i=0;i<deck.count;++i)if(!lanSendEvent(service,Match::Pick,(unsigned)deck.types[i]|(i<<8)|(localRole<<16)|(deck.count<<24),matchId))leaveRequest=true;
        if(!lanSnapshot(service).host && !lanSendEvent(service,Match::Ready,matchId,0))leaveRequest=true;
        engineSetReadyLabel(at<void*>(self,0x88),"WAITING...");
        say("Loadout locked; waiting in the native chooser for the other player.");
    }
    return 1;
}
static bool replaceCall(DWORD call,DWORD target,void* replacement){
    if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=target)return false;
    DWORD old;if(!VirtualProtect((void*)call,5,PAGE_EXECUTE_READWRITE,&old))return false;
    *(DWORD*)(call+1)=(DWORD)replacement-call-5;
    FlushInstructionCache(GetCurrentProcess(),(void*)call,5);
    DWORD unused;VirtualProtect((void*)call,5,old,&unused);return true;
}
extern void engineChooserOverlay(Graphics*);
static void __fastcall chooserDraw(void* self,void*,Graphics* g){
    CallbackGuard guard;originalChooserDraw(self,g);
    if(active && localRole && self==at<void*>(app(),0x774))engineChooserOverlay(g);
}
static void __stdcall chooserTooltip(void* self){
    if(active && localRole){void* tooltip=at<void*>(self,0xd28);if(tooltip)at<bool>(tooltip,0x64)=false;return;}
    ((void (__stdcall*)(void*))0x486150)(self);
}
static bool installPreparation(){
    const DWORD availableCalls[]={0x483ffe,0x48486e,0x484986,0x484b21,0x48521d,0x485a5b,0x485dbd,0x485e41};
    const DWORD countCalls[]={0x4857eb,0x485d2b};
    const DWORD cardCalls[]={0x4848bb,0x484acc,0x484b72};
    // Verify all targets BEFORE any mutation.
    for(DWORD call:availableCalls)if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x453b20)return false;
    for(DWORD call:countCalls)if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x453ac0)return false;
    for(DWORD call:cardCalls)if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x4876f0)return false;
    for(DWORD call:availableCalls)if(!replaceCall(call,0x453b20,(void*)pvzSeedAvailableGate))return false;
    for(DWORD call:countCalls)if(!replaceCall(call,0x453ac0,(void*)pvzSeedCountGate))return false;
    for(DWORD call:cardCalls)if(!replaceCall(call,0x4876f0,(void*)pvzChooserCardGate))return false;
    originalChooserDraw=(Draw)0x484690;
    return replaceCall(0x413124,0x52ae60,(void*)pvzRoadUpdateGate) && replaceCall(0x4170bd,0x52e2e0,(void*)pvzRoadDrawGate) && replaceCall(0x52b316,0x413400,(void*)zombiesWon) && replaceCall(0x45889c,0x458540,(void*)pvzMowGate) && replaceCall(0x4661bd,0x42a040,(void*)pvzPortraitGate) && replaceCall(0x4852cd,0x486150,(void*)chooserTooltip) && replaceCall(0x485d48,0x485600,(void*)pvzChooserStartGate) &&
        slot(0x66d578,26,0x484690,(void*)chooserDraw) &&
        replaceCall(0x4172e7,0x41a0a0,(void*)pvzBottomGate);
}
bool engineInstall(LanService* s,void (*log)(const char*)){
    service=s;logLine=log;
    if(!s || *(DWORD*)(0x656ca8+22*4)!=0x415d40 || *(DWORD*)(0x656ca8+26*4)!=0x41acf0 ||
       memcmp((void*)0x40d120,"\x51\x53\x55",3) || memcmp((void*)0x40ddc0,"\x55\x8b\xec",3)){
        say("Native board signatures differ; match start disabled.");return false;
    }
    originalUpdate=(Update)0x415d40;originalDraw=(Draw)0x41acf0;
    // Relocate only whole, verified instructions from PlayDeathAnim's entry.
    if(memcmp((void*)0x533240,"\x83\xec\x08\x53\x55",5)){say("Native death signature differs.");return false;}
    DWORD protection;if(!VirtualProtect((void*)0x533240,5,PAGE_EXECUTE_READWRITE,&protection))return false;
    *(BYTE*)0x533240=0xe9;*(DWORD*)0x533241=(DWORD)pvzZombieDeathGate-0x533245;
    FlushInstructionCache(GetCurrentProcess(),(void*)0x533240,5);DWORD unused;VirtualProtect((void*)0x533240,5,protection,&unused);
    available=protectSaves() && installPreparation() && engineInstallCoinDraw() && slot(0x656ca8,22,0x415d40,(void*)update) && slot(0x656ca8,26,0x41acf0,(void*)draw) && lostFocusSlot();
    say(available?"Native board hooks installed (protocol 13; two maps, local camera and road deployment).":"Cannot install native board hooks; match start disabled.");return available;
}
static void closeChooser(){
    void* chooser=at<void*>(app(),0x774);if(!chooser){leaveRequest=true;return;}
    pvzCloseChooser(chooser);introClosed=true;introAt=0;
    // Keep native packets within the original engine's enum. Extended cards are
    // represented by the mod deck; no invalid virtual ID reaches native Update.
    if(localRole){void* bank=at<void*>(board,0x144);
        for(unsigned i=0;i<decks[1].count;++i){int card=decks[1].types[i];at<int>((BYTE*)bank+0x28+i*0x50,0x34)=card<100?card:60;}
    }
    say("Both READY; native Ready/Set/Plant intro released.");
}
static unsigned scheduledDelay(const LanSnapshot& connection,unsigned delay){unsigned transit=connection.rtt/2;return delay>(transit+100)?delay-transit:100;}
static void go(){
    running=true;roadView=localRole==1;cameraFrom=roadView?1067.0f:0.0f;cameraAt=GetTickCount();state.tick=at<unsigned>(board,0x5568);lastIncome=state.tick;
    at<int>(board,0x5560)=state.sun=300;at<int>(board,0x5538)=200;
    void* bank=at<void*>(board,0x144);
    if(bank){at<int>(bank,8)=8;at<int>(bank,12)=0;} // Native sun collection flies to the visible HUD bank.
    pvzDeleteQueue(board);say("Both loadouts ready; native local simulation started on this peer.");
}
void enginePoll(){
    if(!service || polling || callbackDepth)return;
    PollGuard guard;LanSnapshot connection=lanSnapshot(service);
    if(active && (connection.phase!=LAN_CONNECTED || at<void*>(app(),0x768)!=board))leaveRequest=true;
    if(leaveRequest){leaveRequest=false;if(active){lanSendEvent(service,Match::Stop,0,matchId);end();}return;}
    if(startRequest){startRequest=false;
        if(connection.phase==LAN_CONNECTED && connection.host && !active){
            unsigned id=GetTickCount()|1;
            if(begin(id)){if(!lanSendEvent(service,Match::Start,id,0))leaveRequest=true;}
            else say("Cannot start native preparation; return to the main menu.");
        }
    }
    LanEvent event;
    while(lanPollEvent(service,&event)){
        if(event.type==Match::Start && !connection.host){if(!begin(event.value)){lanSendEvent(service,Match::Stop,0,event.value);say("Client could not open native preparation.");}}
        else if(event.type==Match::Pick && active && !running && event.nonce==matchId){
            unsigned role=(event.value>>16)&255,index=(event.value>>8)&255,count=event.value>>24;int type=event.value&255;
            if(role!=(unsigned)(1-localRole) || !decks[role].set(role,index,count,type)){say("Invalid remote loadout.");leaveRequest=true;}
        }else if(event.type==Match::Ready && connection.host && active && event.value==matchId && !running){
            peerPrepared=decks[1-localRole].complete();
        }else if(event.type==Match::Intro && !connection.host && active && localPrepared && !introClosed && !introAt && event.value==matchId && decks[1-localRole].complete()){
            introAt=GetTickCount()+scheduledDelay(connection,800);
        }else if(event.type==Match::IntroDone && active && !running && event.value==matchId && introClosed){peerIntroDone=true;
        }else if(event.type==Match::Go && !connection.host && active && introDone && peerIntroDone && event.value==matchId && !running && !goAt){goAt=GetTickCount()+scheduledDelay(connection,500);
        }else if(event.type==Match::Death && !connection.host && active && running){
            Mirror* m=findMap(1,event.nonce);
            if(m && !m->terminal){m->terminal=true;if(exists(m->object,1,m->localId)){
                at<int>(m->object,0xc8)=0;if(event.value==4)pvzZombieDie(m->object);else pvzZombieDeathAnim(m->object);
            }}
        }
        else if(event.type==Match::Command && connection.host)command(event.value,1-localRole,event.nonce);
        else if(event.type==Match::Collect && connection.host && event.nonce==matchId)collect(event.value,1-localRole);
        else if(event.type==Match::Spawn && !connection.host && active && running){
            int role=(event.value&255)==Match::Zombie?1:0,row,col,unit=event.value>>24;
            if(!Match::parseAction(event.value,role,row,col) || decks[role].find(unit)<0 ||
                !addMirror(role,event.nonce,row,col,role?NativeCatalog::zombieType(unit):unit)){say("Cannot apply native spawn event.");leaveRequest=true;}
            else markUsed(role,decks[role].find(unit),unit);
        }else if(event.type==Match::Collected && !connection.host && active && event.nonce==matchId){
            Mirror* m=findMap(3,event.value);if(m && exists(m->object,3,m->localId) && !at<bool>(m->object,0x50))collectLocal(m->object);
        }else if(event.type==Match::RoadSpawn && !connection.host && active && running && !winner){
            int row,col;int unit=event.value>>24;
            if(!Match::parseAction(event.value,1,row,col) || decks[1].find(unit)<0 || !roadAdd(event.nonce,event.value)){leaveRequest=true;continue;}
            markUsed(1,decks[1].find(unit),unit);
        }else if(event.type==Match::RoadPosition && !connection.host && active && running && !winner){
            RoadUnit* u=roadFind(event.nonce);float x;if(u && RoadMotion::decode(event.value,x)){if(Reconcile::absolute(x-u->x)>80)u->x=x;}
        }else if(event.type==Match::RoadDone && !connection.host && active && event.value==matchId){if(RoadUnit* u=roadFind(event.nonce))roadRemove(*u);
        }else if(event.type==Match::Mower && !connection.host && active && running && !winner){
            unsigned row=event.value&255,phase=event.value>>8;if(row>=5 || phase>4)continue;
            void* m=mowerInRow(row);if(m){if(phase==4){at<bool>(m,0x30)=true;at<bool>(m,0x31)=false;}
                else {if(phase==2 && at<int>(m,0x2c)!=2)pvzStartMower(m);
                    if(phase==2){float target=(int)event.nonce/1000.0f;
                        if(Reconcile::absolute(target-at<float>(m,8))>160.0f)at<float>(m,8)=target;}}}
        }else if(event.type==Match::Home && !connection.host && active && !winner && NativeCatalog::zombie(event.value&255) && ((event.value>>8)&255)<5){houseInfo=event.value;houseID=event.nonce;
        }else if(event.type==Match::Finish && !connection.host && active && event.nonce==matchId && !winner){
            Mirror* m=findMap(1,event.value);playLoss(m && exists(m->object,1,m->localId)?m->object:0);
        }
        else if(event.type==Match::Stop && active && event.nonce==matchId){end();return;}
    }
    if(active && !running){
        void* chooser=at<void*>(app(),0x774);
        if(chooser && !introClosed && !localPrepared)engineSetReadyLabel(at<void*>(chooser,0x88),"READY");
        if(connection.host && localPrepared && peerPrepared && !introClosed && !introAt){
            if(lanSendEvent(service,Match::Intro,matchId,0))introAt=GetTickCount()+800;else leaveRequest=true;
        }
        if(introAt && (int)(GetTickCount()-introAt)>=0)closeChooser();
        if(introClosed && !introDone && at<int>(app(),0x7fc)==3){
            introDone=true;if(!lanSendEvent(service,Match::IntroDone,matchId,0))leaveRequest=true;
        }
        if(connection.host && introDone && peerIntroDone && !goAt){
            if(lanSendEvent(service,Match::Go,matchId,0))goAt=GetTickCount()+500;else leaveRequest=true;
        }
        if(goAt && (int)(GetTickCount()-goAt)>=0){goAt=0;go();}
    }
    if(active && !running && GetTickCount()-startedAt>600000){say("Preparation timed out after ten minutes.");leaveRequest=true;}
    if(active && running && !winner && !connection.host){Match::StateData incoming={};if(lanReadState(service,&incoming) && incoming.match==matchId && incoming.tick>=state.tick){
        if(!mirror(incoming)){say("Cannot repair native state.");leaveRequest=true;}
    }}
}
void engineRequestStart(){startRequest=true;}
bool engineResultReady(){return active && winner && GetTickCount()-resultAt>=3500;}
bool engineResultHover(){return menuHover;}
bool engineActive(){return active;}
bool engineStarting(){return active && !running && introClosed && at<int>(app(),0x7fc)==3;}
bool enginePreparing(){return active && at<int>(app(),0x7fc)!=3;}
void engineLeave(){leaveRequest=true;}
const char* engineStatus(){return status;}
const Match::StateData* engineState(){return &state;}
int engineRole(){return localRole;}
bool engineHover(MatchUI::Cell* cell){*cell=hoverCell;return running && !winner && roadView==(localRole==1) && selected>=0 && hoverCell.row>=0;}
bool engineMenuHover(){return menuHover;}
static MatchUI::Cell cellAt(int x,int y){
    MatchUI::Cell cell;
    if(roadView){for(int r=0;r<5;++r)for(int c=0;c<9;++c)if(MatchUI::inside(x,y,RoadLayout::Left+c*RoadLayout::CellWidth,RoadLayout::Top+r*RoadLayout::RowHeight,RoadLayout::CellWidth,RoadLayout::RowHeight))return {r,c,RoadLayout::Left+c*RoadLayout::CellWidth,RoadLayout::Top+r*RoadLayout::RowHeight};return cell;}
    int bx=at<int>(board,0x30),by=at<int>(board,0x34);
    for(int r=0;r<5;++r)for(int c=0;c<9;++c){
        int gx=bx+pvzGridX(board,c,r),gy=by+pvzGridY(board,c,r);
        if(MatchUI::inside(x,y,gx,gy,80,100))return {r,c,gx,gy};
    }
    return cell;
}
unsigned engineCardCount(){return decks[localRole].count;}
int engineCardType(unsigned index){return index<decks[localRole].count?decks[localRole].types[index]:-1;}
int engineSelected(){return selected;}
int engineCardCost(unsigned index){int type=engineCardType(index);return type>=0?engineTypeCost(type):0;}
unsigned engineCardWait(unsigned index){
    if(index>=NativeCatalog::MaxDeck)return 0;
    unsigned current=running?at<unsigned>(board,0x5568):state.tick;
    return readyAt[localRole][index]>current?readyAt[localRole][index]-current:0;
}
static int cardAt(int x,int y){
    if(y<4 || y>=74)return -1;
    for(unsigned i=0;i<decks[localRole].count;++i)if(x>=98+(int)i*54 && x<148+(int)i*54)return (int)i;
    return -1;
}
static unsigned sunAt(int x,int y){
    if(localRole || !running)return 0;
    if(lanSnapshot(service).host){void* objects[1024];unsigned n=enumerate(3,objects,1024);
        for(unsigned i=n;i>0;--i){void* p=objects[i-1];if(!at<bool>(p,0x50) && sunHit(p,x,y))return at<unsigned>(p,NativeLayout::CoinID);}
    }else for(unsigned i=0;i<Match::MaxCoins;++i){Mirror& m=coins[i];
        if(m.hostId && exists(m.object,3,m.localId) && !at<bool>(m.object,0x50) && sunHit(m.object,x,y))return m.hostId;
    }
    return 0;
}
bool engineInput(unsigned message,unsigned key,int x,int y){
    if(!active)return false;
    if(message==WM_KILLFOCUS){pressedCell={};hoverCell={};cardPressed=-1;menuPressed=menuHover=false;return false;}
    if(message==WM_KEYDOWN && key==VK_ESCAPE){engineLeave();return true;}
    if(winner){
        if(message==WM_MOUSEMOVE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP)menuHover=MatchUI::inside(x,y,414,384,240,42);
        if(message==WM_LBUTTONDOWN)resultPressed=menuHover && engineResultReady();
        if(message==WM_LBUTTONUP){bool leave=resultPressed && menuHover && engineResultReady();resultPressed=false;if(leave)engineLeave();}
        return message==WM_KEYDOWN || message==WM_KEYUP || message==WM_CHAR || (message>=WM_MOUSEFIRST && message<=WM_MOUSELAST);
    }
    // Native chooser owns all mouse/keyboard input during preparation.
    if(!localPrepared)return false;
    if(!running && at<int>(app(),0x7fc)!=3)return message==WM_KEYDOWN || message==WM_KEYUP || message==WM_CHAR || (message>=WM_MOUSEFIRST && message<=WM_MOUSELAST);
    bool arrow=MatchUI::inside(x,y,953,90,94,42);
    if(message==WM_LBUTTONDOWN && arrow){arrowPressed=true;return true;}
    if(message==WM_LBUTTONUP && arrowPressed){arrowPressed=false;if(arrow){cameraFrom=engineCameraOffset();roadView=!roadView;cameraAt=GetTickCount();hoverCell={};pressedCell={};}return true;}
    if(message==WM_RBUTTONDOWN){selected=-1;return true;}
    if(message==WM_MOUSEMOVE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP){menuHover=MatchUI::menuAt(x,y);hoverCell=cellAt(x,y);}
    if(message==WM_LBUTTONDOWN){pressedCell=hoverCell;menuPressed=menuHover;cardPressed=cardAt(x,y);}
    if(message==WM_LBUTTONUP){
        bool leave=menuPressed && menuHover;int card=cardAt(x,y);
        bool choose=cardPressed>=0 && cardPressed==card;
        bool place=pressedCell.row>=0 && hoverCell.row==pressedCell.row && hoverCell.col==pressedCell.col;
        pressedCell={};menuPressed=false;cardPressed=-1;
        if(leave){engineLeave();return true;}
        if(choose){selected=selected==card?-1:card;return true;}
        if(!running || winner)return true;
        if(GetTickCount()-cameraAt<500 || roadView!=(localRole==1))return true;
        unsigned sun=sunAt(x,y);
        if(sun){if(lanSnapshot(service).host)collect(sun,localRole);else lanSendEvent(service,Match::Collect,sun,matchId);return true;}
        if(!place || selected<0 || engineCardWait(selected))return true;
        int type=engineCardType(selected);
        unsigned action=Match::action(localRole?Match::Zombie:Match::Plant,hoverCell.row,hoverCell.col,type);
        if(lanSnapshot(service).host)command(action,localRole,0);else lanSendEvent(service,Match::Command,action,++requestSeq);
        selected=-1;
    }
    return message==WM_KEYDOWN || message==WM_KEYUP || message==WM_CHAR || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP ||
        message==WM_RBUTTONDOWN || message==WM_RBUTTONUP || message==WM_MOUSEMOVE;
}

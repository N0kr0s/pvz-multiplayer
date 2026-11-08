// Apply our entry-local XOR delta to a matching widescreen PAK. No game data is bundled.
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif
using Byte=unsigned char;
static unsigned hash(const Byte* p,unsigned n){unsigned v=2166136261u;for(unsigned i=0;i<n;++i)v=(v^p[i])*16777619u;return v;}
static unsigned u32(const Byte* p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
static void put32(Byte* p,unsigned n){for(int i=0;i<4;++i)p[i]=(Byte)(n>>(i*8));}
struct Entry {char name[256];unsigned length,offset;Byte meta[8];Byte* replacement;};
static bool fail(char* error,const char* message){strcpy(error,message);return false;}
static bool patch(const Byte* input,unsigned size,const Byte* delta,unsigned deltaSize,Byte** output,unsigned* outputSize,char* error){
    if(size<9 || size>128*1024*1024 || deltaSize<12 || memcmp(delta,"PVZMPK01",8))return fail(error,"Invalid PAK or mod patch.");
    Byte* raw=(Byte*)malloc(size);Entry* entries=(Entry*)calloc(8192,sizeof(Entry));
    if(!raw || !entries){free(raw);free(entries);return fail(error,"Not enough memory.");}
    for(unsigned i=0;i<size;++i)raw[i]=input[i]^247;
    bool ok=false;unsigned count=0,pos=8,newSize=size;Byte* result=0;
    do {
        const Byte magic[8]={0xc0,0x4a,0xc0,0xba,0,0,0,0};
        if(memcmp(raw,magic,8)){fail(error,"This main.pak is not a supported PvZ PAK.");break;}
        bool valid=true;
        while(pos<size && raw[pos]==0){
            if(count==8192 || size-pos<2){valid=false;break;}
            ++pos;unsigned length=raw[pos++];
            if(!length || size-pos<length+12){valid=false;break;}
            Entry& e=entries[count++];memcpy(e.name,raw+pos,length);pos+=length;
            e.length=u32(raw+pos);memcpy(e.meta,raw+pos+4,8);pos+=12;
        }
        if(!valid || pos>=size || raw[pos++]!=128){fail(error,"Corrupt PAK directory.");break;}
        unsigned cursor=pos;
        for(unsigned i=0;i<count;++i){Entry& e=entries[i];if(e.length>size-cursor){valid=false;break;}e.offset=cursor;cursor+=e.length;}
        if(!valid || cursor!=size){fail(error,"Corrupt PAK data.");break;}
        unsigned records=u32(delta+8),at=12;
        if(records<1 || records>2){fail(error,"Unsupported patch version.");break;}
        for(unsigned r=0;r<records && valid;++r){
            if(deltaSize-at<1){valid=false;break;}
            unsigned nameSize=delta[at++];char name[256]={};
            if(!nameSize || deltaSize-at<nameSize+16){valid=false;break;}
            memcpy(name,delta+at,nameSize);at+=nameSize;
            unsigned oldLength=u32(delta+at),oldHash=u32(delta+at+4),newLength=u32(delta+at+8),newHash=u32(delta+at+12);at+=16;
            unsigned xorLength=oldLength>newLength?oldLength:newLength;
            if(oldLength>16*1024*1024 || newLength>16*1024*1024 || xorLength>deltaSize-at){valid=false;break;}
            Entry* entry=0;
            for(unsigned i=0;i<count;++i)if(!strcmp(entries[i].name,name)){if(entry){valid=false;break;}entry=&entries[i];}
            if(!valid || !entry){fail(error,"Required PAK entry is missing or duplicated.");valid=false;break;}
            unsigned currentHash=hash(raw+entry->offset,entry->length);
            if(entry->length==newLength && currentHash==newHash){at+=xorLength;continue;}
            if(entry->length!=oldLength || currentHash!=oldHash){
                fail(error,"PAK differs from PvZWidescreenForSteam 1.0. No files changed. Install that exact base first.");valid=false;break;
            }
            entry->replacement=(Byte*)malloc(newLength?newLength:1);
            if(!entry->replacement){fail(error,"Not enough memory.");valid=false;break;}
            for(unsigned i=0;i<newLength;++i)entry->replacement[i]=delta[at+i]^(i<oldLength?raw[entry->offset+i]:0);
            if(hash(entry->replacement,newLength)!=newHash){fail(error,"Mod patch verification failed.");valid=false;break;}
            newSize=newSize-entry->length+newLength;entry->length=newLength;at+=xorLength;
        }
        if(!valid || at!=deltaSize){if(!error[0])fail(error,"Corrupt mod patch.");break;}
        result=(Byte*)malloc(newSize);
        if(!result){fail(error,"Not enough memory.");break;}
        memcpy(result,raw,8);unsigned header=8,data=pos;
        for(unsigned i=0;i<count;++i){Entry& e=entries[i];unsigned n=strlen(e.name);result[header++]=0;result[header++]=(Byte)n;
            memcpy(result+header,e.name,n);header+=n;put32(result+header,e.length);header+=4;memcpy(result+header,e.meta,8);header+=8;
            memcpy(result+data,e.replacement?e.replacement:raw+e.offset,e.length);data+=e.length;
        }
        result[header++]=128;
        if(header!=pos || data!=newSize){fail(error,"Internal PAK size mismatch.");break;}
        for(unsigned i=0;i<newSize;++i)result[i]^=247;
        *output=result;*outputSize=newSize;result=0;ok=true;
    }while(false);
    free(result);for(unsigned i=0;i<count;++i)free(entries[i].replacement);free(entries);free(raw);return ok;
}
static Byte* read(const char* path,unsigned* size){
    FILE* f=fopen(path,"rb");if(!f)return 0;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<0 || n>128*1024*1024){fclose(f);return 0;}
    Byte* p=(Byte*)malloc(n?n:1);if(!p){fclose(f);return 0;}
    bool ok=fread(p,1,n,f)==(unsigned)n;fclose(f);if(!ok){free(p);return 0;}*size=(unsigned)n;return p;
}
static bool write(const char* path,const Byte* p,unsigned size){
    FILE* f=fopen(path,"wb");if(!f)return false;
    bool ok=fwrite(p,1,size,f)==size;if(fclose(f))ok=false;return ok;
}
#ifdef _WIN32
static bool gameRunning(){
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;
    PROCESSENTRY32 e={};e.dwSize=sizeof(e);bool running=false;
    if(Process32First(snapshot,&e))do{if(!lstrcmpiA(e.szExeFile,"pvzgame.exe")){running=true;break;}}while(Process32Next(snapshot,&e));
    CloseHandle(snapshot);return running;
}
extern "C" void WINAPI Start(){
    char directory[MAX_PATH];GetModuleFileNameA(0,directory,MAX_PATH);char* last=directory;
    for(char* p=directory;*p;++p)if(*p=='\\' || *p=='/')last=p;*last=0;
    char error[256]={};Byte* result=0;unsigned inputSize=0,deltaSize=0,resultSize=0;
    Byte* input=0;Byte* delta=0;bool ok=false;
    do {
        if(!SetCurrentDirectoryA(directory)){fail(error,"Cannot open the mod directory.");break;}
        input=read("..\\main.pak",&inputSize);delta=read("assets\\menu.patch",&deltaSize);
        if(!input || !delta){fail(error,"main.pak or multiplayer/assets/menu.patch is missing. Extract the complete mod into the game directory.");break;}
        if(!patch(input,inputSize,delta,deltaSize,&result,&resultSize,error)){
            unsigned upgradeSize=0;Byte* upgrade=read("assets\\road-upgrade.patch",&upgradeSize);
            error[0]=0;bool upgraded=upgrade && patch(input,inputSize,upgrade,upgradeSize,&result,&resultSize,error);free(upgrade);
            if(!upgraded)break;
        }
        if(resultSize==inputSize && !memcmp(result,input,inputSize)){ok=true;break;}
        if(gameRunning()){fail(error,"Close all Plants vs. Zombies windows before installing the menu patch.");break;}
        CreateDirectoryA("backups",0);
        if(!CopyFileA("..\\main.pak","backups\\before-native-menu.pak",TRUE) && GetLastError()!=ERROR_FILE_EXISTS){fail(error,"Cannot back up main.pak. Check write permissions in the game directory.");break;}
        if(!write("..\\main.pak.pvzmp-tmp",result,resultSize)){fail(error,"Cannot write the patched PAK. Original main.pak is unchanged.");break;}
        if(!MoveFileExA("..\\main.pak.pvzmp-tmp","..\\main.pak",MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){fail(error,"Cannot replace main.pak. Close the game and check write permissions.");break;}
        ok=true;
    }while(false);
    free(result);free(input);free(delta);
    if(!ok)MessageBoxA(0,error,"PvZ Multiplayer resource installer",MB_OK|MB_ICONERROR);
    ExitProcess(ok?0:1);
}
#else
int main(int argc,char** argv){
    if(argc!=4){fprintf(stderr,"Usage: %s main.pak menu.patch output.pak\n",argv[0]);return 2;}
    unsigned a=0,b=0,c=0;Byte* input=read(argv[1],&a);Byte* delta=read(argv[2],&b);Byte* output=0;char error[256]={};
    bool ok=input && delta && patch(input,a,delta,b,&output,&c,error);
    if(ok)ok=write(argv[3],output,c);
    if(!ok)fprintf(stderr,"%s\n",error[0]?error:"File read/write failed");
    free(input);free(delta);free(output);return ok?0:1;
}
#endif

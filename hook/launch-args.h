#pragma once
namespace LaunchArgs {
inline const char* spaces(const char* p){while(*p==' ' || *p=='\t')++p;return p;}
inline const char* tokenEnd(const char* p){
    if(*p=='"'){++p;while(*p && *p!='"')++p;if(*p)++p;}
    else while(*p && *p!=' ' && *p!='\t')++p;
    return p;
}
inline bool originalGame(const char* start,const char* end){
    if(start<end && *start=='"'){++start;if(end>start && end[-1]=='"')--end;}
    const char* name=start;for(const char* p=start;p<end;++p)if(*p=='/' || *p=='\\')name=p+1;
    const char* expected="plantsvszombies.exe";
    while(name<end && *expected){char c=*name++;if(c>='A' && c<='Z')c+=32;if(c!=*expected++)return false;}
    return name==end && !*expected;
}
inline const char* gameArguments(const char* command){
    const char* args=tokenEnd(command);const char* next=spaces(args);const char* end=tokenEnd(next);
    // Windows Steam %command% includes the original executable. Linux's
    // wrapper replaces that executable, leaving just the actual game options.
    return originalGame(next,end)?end:args;
}
}

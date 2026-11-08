#include "../hook/launch-args.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
    struct Case {const char* command;const char* expected;};
    const Case cases[]={
        {R"("D:\Program Files\Steam\game\multiplayer\start-windows.exe" "D:\Program Files\Steam\game\PlantsVsZombies.exe" -steam -flag "a b")",R"( -steam -flag "a b")"},
        {R"("D:\game\start-windows.exe" D:\game\PlantsVsZombies.exe -steam)"," -steam"},
        {R"("Z:\game\hook\pvzmp-loader.exe" -steam -foo)"," -steam -foo"},
        {R"("D:\game\start-windows.exe")",""},
        {R"(loader.exe "D:\game\PLANTSVSZOMBIES.EXE")",""},
        {R"(loader.exe "PlantsVsZombies.exe.backup" -foo)",R"( "PlantsVsZombies.exe.backup" -foo)"},
        {R"(loader.exe "argument with spaces")",R"( "argument with spaces")"},
    };
    for(const auto& c:cases)assert(!std::strcmp(LaunchArgs::gameArguments(c.command),c.expected));
    puts("Steam launch arguments OK: quoted Windows paths, original EXE removal, native args and Proton replacement.");
}

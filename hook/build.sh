#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
flags=(--target=i686-windows-gnu -std=c++20 -fno-exceptions -fno-rtti -fno-stack-protector -I/usr/include/wine/windows -I/usr/include/wine/msvcrt)
clang++ "${flags[@]}" -c menu.cpp -o menu.obj
clang++ "${flags[@]}" -c lan.cpp -o lan.obj
clang++ "${flags[@]}" -c diagnostics.cpp -o diagnostics.obj
lld-link /safeseh:no /dll /machine:x86 /entry:DllMain@12 /out:pvzmp-hook.dll diagnostics.obj menu.obj lan.obj /usr/lib/wine/i386-windows/libiphlpapi.a /usr/lib/wine/i386-windows/libws2_32.a /usr/lib/wine/i386-windows/libmsvcrt.a /usr/lib/wine/i386-windows/libgdi32.a /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a
clang++ "${flags[@]}" -c bootstrap.cpp -o bootstrap.obj
lld-link /safeseh:no /machine:x86 /subsystem:windows /entry:Start@0 /out:pvzmp-loader.exe bootstrap.obj /usr/lib/wine/i386-windows/libmsvcrt.a /usr/lib/wine/i386-windows/libkernel32.a /usr/lib/wine/i386-windows/libuser32.a

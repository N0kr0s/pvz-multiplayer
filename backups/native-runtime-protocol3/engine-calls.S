.intel_syntax noprefix
.text
.globl _pvzMakeBoard
_pvzMakeBoard:
    mov ecx, [esp+4]
    mov eax, 0x44f5f0
    jmp eax
.globl _pvzInitBoard
_pvzInitBoard:
    push [esp+4]
    mov eax, 0x40af90
    call eax
    ret
.globl _pvzStartPlaying
_pvzStartPlaying:
    push esi
    mov esi, [esp+8]
    mov eax, 0x44f6b0
    call eax
    pop esi
    ret
.globl _pvzKillSelector
_pvzKillSelector:
    push esi
    mov esi, [esp+8]
    mov eax, 0x44f9e0
    call eax
    pop esi
    ret
.globl _pvzShowMenu
_pvzShowMenu:
    mov ecx, [esp+4]
    mov eax, 0x44f8e0
    jmp eax
.globl _pvzAddPlant
_pvzAddPlant:
    mov eax, [esp+12] // native EAX is row; column is the second stack argument
    mov ecx, [esp+8]
    mov edx, [esp+4]
    push -1
    push 0
    push ecx
    push edx
    mov edx, 0x40d120
    call edx
    ret
.globl _pvzAddZombie
_pvzAddZombie:
    push ebx
    mov eax, [esp+8]
    mov ecx, [esp+12]
    mov ebx, 0 // fromWave, not row
    push ecx // row
    push 0 // zombie type
    mov edx, 0x40ddc0
    call edx
    pop ebx
    ret
.globl _pvzAddShot
_pvzAddShot:
    mov eax, [esp+4]
    mov ecx, [esp+16]
    imul edx, ecx, 10000
    add edx, 400000
    push 0
    push ecx
    push edx
    push [esp+24]
    push [esp+24]
    mov ecx, 0x40d620
    call ecx
    ret
.globl _pvzDeleteQueue
_pvzDeleteQueue:
    push esi
    mov esi, [esp+8]
    mov eax, 0x41bad0
    call eax
    pop esi
    ret
.globl _pvzPlantDie
_pvzPlantDie:
    push [esp+4]
    mov eax, 0x4679b0
    call eax
    ret
.globl _pvzZombieDie
_pvzZombieDie:
    mov ecx, [esp+4]
    mov eax, 0x530510
    jmp eax
.globl _pvzGridX
_pvzGridX:
    mov ecx, [esp+4]
    mov eax, [esp+8]
    mov edx, 0x41c680
    call edx
    ret
.globl _pvzGridY
_pvzGridY:
    push ebx
    mov ebx, [esp+8]
    mov ecx, [esp+12]
    mov eax, [esp+16]
    mov edx, 0x41c740
    call edx
    pop ebx
    ret
// Cosmetic-only client update, without the game's time-stop screen effect.
.globl _pvzUpdateEffects
_pvzUpdateEffects:
    push edi
    mov edi, [esp+8]
    mov eax, 0x445890
    call eax
    pop edi
    ret
.globl _pvzUpdateLayers
_pvzUpdateLayers:
    push [esp+4]
    mov eax, 0x416080
    call eax
    ret
// DrawSeedPacket: ECX=grayness; eight caller-cleaned stack arguments.
.globl _pvzDrawSeedPacket
_pvzDrawSeedPacket:
    mov ecx, [esp+20]
    mov eax, [esp+4]
    mov edx, [esp+16]
    push 0
    push 0
    push 0
    push -1
    push edx
    push [esp+32] // y (original esp+12)
    push [esp+32] // x (original esp+8)
    push eax
    mov eax, 0x4876f0
    call eax
    add esp, 32
    ret
// Render-dispatch call gate: EDI=Graphics, callee-cleaned Board argument.
.globl _pvzBottomGate
_pvzBottomGate:
    push edi
    push [esp+8]
    call _engineHideNativeUI
    add esp, 4
    pop edi
    test eax, eax
    jnz 3f
    mov eax, 0x41a0a0
    jmp eax
3:
    ret 4
// Clang's i686 Windows stack allocator: probe every guard page and preserve ECX.
// Large wire buffers must not jump across the Windows thread stack guard page.
.globl __alloca
__alloca:
    push ecx
    lea ecx, [esp+8]
    cmp eax, 4096
    jb 2f
1:
    sub ecx, 4096
    test DWORD PTR [ecx], eax
    sub eax, 4096
    cmp eax, 4096
    ja 1b
2:
    sub ecx, eax
    test DWORD PTR [ecx], eax
    mov eax, esp
    mov esp, ecx
    mov ecx, [eax]
    mov eax, [eax+4]
    push eax
    ret

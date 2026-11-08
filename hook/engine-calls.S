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
    push [esp+20] // seed type (original esp+16)
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
    mov edx, [esp+16] // type
    mov ebx, 0 // fromWave, not row
    push ecx // row
    push edx // zombie type
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
// Cosmetic update while waiting for readiness or after the match ends.
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
    push 0 // useCurrentCost
    push 1 // drawCost: native digits and font
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
.globl _pvzPreloadPlant
_pvzPreloadPlant:
    push esi
    mov esi, [esp+8]
    mov eax, 0x4681e0
    call eax
    pop esi
    ret
.globl _pvzPreloadZombie
_pvzPreloadZombie:
    mov eax, [esp+4]
    mov edx, 0x5369e0
    jmp edx
.globl _pvzNewGame
_pvzNewGame:
    mov eax, [esp+4]
    mov edx, 0x44f890
    jmp edx
.globl _pvzKillChooser
_pvzKillChooser:
    push esi
    mov esi, [esp+8]
    mov eax, 0x44fe70
    call eax
    pop esi
    ret
.globl _pvzCloseChooser
_pvzCloseChooser:
    push ebx
    mov ebx, [esp+8]
    mov eax, 0x486d20
    call eax
    pop ebx
    ret
.globl _pvzCanPlant
_pvzCanPlant:
    mov eax, [esp+12]
    mov ecx, [esp+4]
    push [esp+16]
    push [esp+12]
    push ecx
    mov edx, 0x40e020
    call edx
    ret
.globl _pvzSeedCost
_pvzSeedCost:
    mov eax, [esp+4]
    mov edx, -1
    mov ecx, 0x467b00
    jmp ecx
.globl _pvzAddCoin
_pvzAddCoin:
    mov ecx, [esp+4]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    push [esp+20]
    mov eax, 0x40cb10
    call eax
    ret
.globl _pvzCollectCoin
_pvzCollectCoin:
    mov ecx, [esp+4]
    mov eax, 0x432060
    jmp eax
// Scoped calls from the stock chooser: leave all ordinary modes unchanged.
.globl _pvzSeedAvailableGate
_pvzSeedAvailableGate:
    push ecx
    push edx
    push ebx
    call _engineSeedAvailable
    add esp, 4
    pop edx
    pop ecx
    cmp eax, -1
    jne 4f
    mov eax, 0x453b20
    jmp eax
4:
    ret
.globl _pvzSeedCountGate
_pvzSeedCountGate:
    push ecx
    push edx
    push eax
    call _engineSeedCount
    pop edx // saved app
    cmp eax, -1
    jne 5f
    mov eax, edx
    pop edx
    pop ecx
    push 0x453ac0
    ret
5:
    pop edx
    pop ecx
    ret
.globl _pvzChooserCardGate
_pvzChooserCardGate:
    push ecx
    push [esp+20]
    call _engineMapChooserCard
    add esp, 4
    pop ecx
    cmp eax, -1
    je 6f
    mov [esp+16], eax
    cmp eax, 100
    jb 10f
    jmp _pvzCustomCardGate
10:
    mov eax, 0x4876f0
    jmp eax
6:
    ret
.globl _pvzChooserStartGate
_pvzChooserStartGate:
    push eax
    push ecx
    push edx
    push eax
    call _engineAcceptChooser
    add esp, 4
    pop edx
    pop ecx
    test eax, eax
    pop eax
    jnz 7f
    push 0x485600
    ret
7:
    ret
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

// Zombie::PlayDeathAnim: EAX=this, one callee-cleaned damage-flags argument.
.globl _pvzZombieDeathAnim
_pvzZombieDeathAnim:
    mov eax, [esp+4]
    push 0
    mov edx, 0x533240
    call edx
    ret
.globl _pvzSetButtonLabel
_pvzSetButtonLabel:
    mov edx, [esp+8]
    push [esp+4]
    mov eax, 0x448470
    call eax
    ret
// This override is scoped to one custom card draw, on the game thread.
.globl _pvzPortraitGate
_pvzPortraitGate:
    cmp dword ptr [_pvzPortraitOverride], -1
    je 8f
    mov eax, [_pvzPortraitOverride]
    ret
8:
    push 0x42a040
    ret
.globl _pvzCustomCardGate
_pvzCustomCardGate:
    push ecx
    .rept 8
    push [esp+36]
    .endr
    call _engineCustomCard
    add esp, 36
    ret
.globl _pvzDrawSeedPacketNoCost
_pvzDrawSeedPacketNoCost:
    mov ecx, [esp+20]
    mov eax, [esp+4]
    mov edx, [esp+16]
    push 0
    push 0
    push 0
    push -1
    push edx
    push [esp+32]
    push [esp+32]
    push eax
    mov eax, 0x4876f0
    call eax
    add esp, 32
    ret

.globl _pvzZombieDamageStates
_pvzZombieDamageStates:
    push edi
    mov eax, [esp+8]
    mov edi, 0
    mov edx, 0x52a470
    call edx
    pop edi
    ret
.globl _pvzZombieDeathGate
_pvzZombieDeathGate:
    push eax
    push ecx
    push edx
    push eax
    call _engineAllowZombieDeath
    add esp, 4
    test eax, eax
    jz 9f
    pop edx
    pop ecx
    pop eax
    // Original five bytes, then resume at the next instruction.
    sub esp, 8
    push ebx
    push ebp
    push 0x533245
    ret
9:
    pop edx
    pop ecx
    pop eax
    ret 4

.globl _pvzSunSound
_pvzSunSound:
    mov edx, [esp+4]
    mov eax, 0x432b00
    jmp eax
.globl _pvzStartMower
_pvzStartMower:
    push esi
    mov esi, [esp+8]
    mov eax, 0x458da0
    call eax
    pop esi
    ret
.globl _pvzMowGate
_pvzMowGate:
    push eax
    push ecx
    push edx
    push eax
    call _engineAllowMower
    add esp, 4
    test eax, eax
    pop edx
    pop ecx
    pop eax
    jz 11f
    push 0x458540
    ret
11:
    ret 4

.globl _pvzWalkIntoHouse
_pvzWalkIntoHouse:
    mov eax, [esp+4]
    mov edx, 0x534780
    jmp edx

.globl _pvzAddRoadVisual
_pvzAddRoadVisual:
    push ebx
    mov eax, [esp+8]
    mov ecx, [esp+12]
    mov edx, [esp+16]
    mov ebx, -3
    push ecx
    push edx
    mov edx, 0x40ddc0
    call edx
    pop ebx
    ret
.globl _pvzRoadReanim
_pvzRoadReanim:
    mov eax, [esp+4]
    mov edx, 0x52c310
    jmp edx
.globl _pvzRoadUpdateGate
_pvzRoadUpdateGate:
    push eax
    push ecx
    push edx
    push eax
    call _engineUpdateRoadObject
    add esp, 4
    test eax, eax
    pop edx
    pop ecx
    pop eax
    jnz 12f
    push 0x52ae60
    ret
12:
    ret
.globl _pvzRoadDrawGate
_pvzRoadDrawGate:
    push ecx
    push edx
    push ebx
    call _engineHideRoadObject
    add esp, 4
    test eax, eax
    pop edx
    pop ecx
    jnz 13f
    push 0x52e2e0
    ret
13:
    ret 4
.globl _pvzDrawRoadZombie
_pvzDrawRoadZombie:
    push ebx
    mov ebx, [esp+8]
    push [esp+12]
    mov eax, 0x52e2e0
    call eax
    pop ebx
    ret

.globl _pvzRoadWalk
_pvzRoadWalk:
    push esi
    mov eax, [esp+8]
    mov esi, 0
    mov edx, 0x52f2e0
    call edx
    pop esi
    ret
.globl _pvzRoadAnimRate
_pvzRoadAnimRate:
    mov eax, [esp+4]
    push [esp+8]
    mov edx, 0x52eff0
    call edx
    ret

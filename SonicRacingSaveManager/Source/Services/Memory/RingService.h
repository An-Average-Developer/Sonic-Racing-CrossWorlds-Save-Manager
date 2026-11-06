#pragma once
#include "Services/Memory/ProcessMemory.h"

// Max rings in races. The game keeps one 0x3C-byte ring block per racer (12 slots) in its race
// object, whose pointer is a global of the exe (module+8B5B9E0 in 1.4.x). Block of racer n at
// [global] + 0x34 + n*0x3C:
//   +0x00 rings now          +0x08 rings picked up in total     +0x20 rings pending
//   +0x28 max rings (100)    +0x2C checksum seed                +0x30 checksum = key ^ rings
// The global is found through the code of the game's AddRings(racer, amount):
//   48 8B 3D <global>   mov rdi,[global]
//   8B F2 8B D9         mov esi,edx; mov ebx,ecx
//   48 85 FF 74 1D      test rdi,rdi; je
//   8B CB E8 ????????   mov ecx,ebx; call
//   84 C0 75 12         test al,al; jne
//   83 FB 0C 7D 0D      cmp ebx,12; jge
//   48 6B CB 3C         imul rcx,rbx,3C
//   01 74 39 54         add [rcx+rdi+54],esi   (racer n's pending rings)
// so it survives game updates that move the code. Only the local player (racer 0) is touched: the
// CPU racers share the same code, so a code patch would give them 100 rings too. While on, Tick()
// (every 100 ms) puts the player's rings back to the max with the matching checksum.
namespace SonicRacingSaveManager::Memory {

// The address of the race object's global (see above), 0 when the game's code wasn't found
int64_t FindRaceGlobal(const ProcessMemory& memory);

class RingService {
public:
    explicit RingService(ProcessMemory& memory) : memory(memory) {}

    bool IsOn() const { return on; }
    // Finds the race object's global (once per attach); false when the game's code wasn't found
    bool Enable();
    void Disable() { on = false; }
    // Detach / the game closed: forget the global too (the next attach scans again)
    void Reset() { on = false; global = 0; }
    // Keeps racer 0 at the max; does nothing outside races (no race object or no sane ring block)
    void Tick();

private:
    ProcessMemory& memory;
    int64_t global = 0;
    bool on = false;
};

} // namespace SonicRacingSaveManager::Memory

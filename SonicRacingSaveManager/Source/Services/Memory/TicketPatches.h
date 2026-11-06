#pragma once
#include "Services/Memory/ProcessMemory.h"

// Ticket patches. Addresses are relative to the game's main module (WriteBytes adds the module base).
//
// Target: the ticket spend routine at module+4FC52D0 (ecx = amount -> r14d, rbx = currency struct):
//   4FC534D  8B 43 58        mov eax,[rbx+58]       ; current tickets
//   4FC5350  41 0F44 CC      cmove ecx,r12d         ; ecx = cap (999 / 9999999)
//   4FC5354  41 2B C6        sub eax,r14d           ; tickets -= amount
//   4FC5357  3B C1           cmp eax,ecx
//   4FC5359  0F 4C C8        cmovl ecx,eax          ; ecx = min(tickets, cap)
//   4FC535C  8B C5           mov eax,ebp            ; ebp = 0
//   4FC535E  85 C9           test ecx,ecx
//   4FC5360  0F 4F C1        cmovg eax,ecx          ; eax = max(ecx, 0)
//   4FC5363  89 43 58        mov [rbx+58],eax       ; store tickets
//   4FC5366  41 80 FF 13     cmp r15b,13
namespace SonicRacingSaveManager::Memory::TicketPatches {

// Freeze configuration for tickets (skip the store)
inline constexpr int64_t TICKET_FREEZE_ADDRESS = 0x4FC5363;
inline const Bytes FREEZE_BYTES = { 0x90, 0x90, 0x90 };     // NOP instructions
inline const Bytes ORIGINAL_BYTES = { 0x89, 0x43, 0x58 };   // mov [rbx+58],eax

// Multiply configuration for tickets (flips sub→add so spending gains tickets)
inline constexpr int64_t TICKET_MULTIPLY_ADDRESS = 0x4FC5354;
inline const Bytes MULTIPLY_BYTES = { 0x41, 0x03, 0xC6 };           // add eax,r14d
inline const Bytes MULTIPLY_ORIGINAL_BYTES = { 0x41, 0x2B, 0xC6 };  // sub eax,r14d

// Multiply x500 configuration - code cave in the INT3 padding after the function at 4FB3960.
// The 5 bytes at 4FC5354 (sub eax,r14d; cmp eax,ecx) become a JMP to cave A (4FB39B1, 15 x CC),
// which computes eax += r14d*500 and redoes the cmp. A jmp rel32 back doesn't fit in the 15 bytes,
// so cave A hops (jmp short) to cave B (4FB39EA, 6 x CC), which jumps back to 4FC5359 (cmovl), so
// the game's own cap / >= 0 clamps still run. edx is free there (the next call clobbers it); r14d
// is left intact because the code after the store uses it for another counter.
inline constexpr int64_t TICKET_MULTIPLY500_PATCH_ADDRESS = 0x4FC5354;
inline const Bytes MULTIPLY500_PATCH_BYTES = { 0xE9, 0x58, 0xE6, 0xFE, 0xFF };      // jmp 4FB39B1
inline const Bytes MULTIPLY500_PATCH_ORIGINAL = { 0x41, 0x2B, 0xC6, 0x3B, 0xC1 };   // sub eax,r14d; cmp eax,ecx

inline constexpr int64_t TICKET_MULTIPLY500_CAVE_ADDRESS = 0x4FB39B1;
inline const Bytes MULTIPLY500_CAVE_BYTES = {
    0x41, 0x69, 0xD6, 0xF4, 0x01, 0x00, 0x00,   // imul edx,r14d,500
    0x01, 0xD0,                                 // add eax,edx
    0x3B, 0xC1,                                 // cmp eax,ecx  (restored)
    0xEB, 0x2C,                                 // jmp short 4FB39EA (cave B)
    0xCC, 0xCC                                  // int3 (unused)
};
inline const Bytes MULTIPLY500_CAVE_ORIGINAL = Bytes(15, 0xCC);

inline constexpr int64_t TICKET_MULTIPLY500_CAVE2_ADDRESS = 0x4FB39EA;
inline const Bytes MULTIPLY500_CAVE2_BYTES = { 0xE9, 0x6A, 0x19, 0x01, 0x00 };      // jmp 4FC5359 (cmovl ecx,eax)
inline const Bytes MULTIPLY500_CAVE2_ORIGINAL = Bytes(5, 0xCC);

} // namespace SonicRacingSaveManager::Memory::TicketPatches

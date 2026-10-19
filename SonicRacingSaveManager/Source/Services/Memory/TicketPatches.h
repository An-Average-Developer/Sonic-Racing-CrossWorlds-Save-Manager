#pragma once
#include "Services/Memory/ProcessMemory.h"

// The ticket patches of MemoryEditorViewModel.cs, copied byte for byte. Addresses are relative to
// the game's main module (WriteBytes adds the module base).
namespace SonicRacingSaveManager::Memory::TicketPatches {

// Freeze configuration for tickets
inline constexpr int64_t TICKET_FREEZE_ADDRESS = 0x4D830ED;
inline const Bytes FREEZE_BYTES = { 0x90, 0x90, 0x90 };     // NOP instructions
inline const Bytes ORIGINAL_BYTES = { 0x89, 0x5E, 0x58 };   // mov [rsi+58],ebx

// Multiply configuration for tickets (flips sub→add so spending gains tickets)
inline constexpr int64_t TICKET_MULTIPLY_ADDRESS = 0x4D830DB;      // 6 time up of the freeze bytes
inline const Bytes MULTIPLY_BYTES = { 0x41, 0x03, 0xC6 };           // add eax,r14d
inline const Bytes MULTIPLY_ORIGINAL_BYTES = { 0x41, 0x2B, 0xC6 };  // sub eax,r14d

// Multiply x500 configuration - code cave in INT3 padding at 4D83101
// Patches 5 bytes at 4D830DB (sub+cmp) with JMP to cave, cave computes eax += r14d*500
inline constexpr int64_t TICKET_MULTIPLY500_PATCH_ADDRESS = 0x4D830DB;
inline const Bytes MULTIPLY500_PATCH_BYTES = { 0xE9, 0x21, 0x00, 0x00, 0x00 };      // JMP to cave at 4D83101
inline const Bytes MULTIPLY500_PATCH_ORIGINAL = { 0x41, 0x2B, 0xC6, 0x3B, 0xC7 };   // sub eax,r14d; cmp eax,edi
inline constexpr int64_t TICKET_MULTIPLY500_CAVE_ADDRESS = 0x4D83101;               // 6 lines below the freeze bytes
inline const Bytes MULTIPLY500_CAVE_BYTES = {
    0x45, 0x69, 0xF6, 0xF4, 0x01, 0x00, 0x00,   // imul r14d,r14d,500
    0x41, 0x03, 0xC6,                           // add eax,r14d
    0x3B, 0xC7,                                 // cmp eax,edi  (restored)
    0x90,                                       // nop
    0xEB, 0xD0                                  // jmp short -0x30 → back to 4D830E0
};
inline const Bytes MULTIPLY500_CAVE_ORIGINAL = {
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
    0xCC, 0xCC, 0xCC, 0xCC, 0xCC
};

} // namespace SonicRacingSaveManager::Memory::TicketPatches

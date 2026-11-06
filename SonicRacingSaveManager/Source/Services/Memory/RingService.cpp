#include "Services/Memory/RingService.h"

namespace SonicRacingSaveManager::Memory {

namespace {
constexpr const char* kAddRingsPattern =
    "48 8B 3D ?? ?? ?? ?? 8B F2 8B D9 48 85 FF 74 1D 8B CB E8 ?? ?? ?? ?? 84 C0 75 12 83 FB 0C 7D 0D 48 6B CB 3C 01 74 39 54";
constexpr int64_t kFirstBlock = 0x34, kRings = 0x00, kMax = 0x28, kChecksum = 0x30;
constexpr int kPlayer = 0, kBlockSize = 0x3C;
} // namespace

int64_t FindRaceGlobal(const ProcessMemory& memory) {
    const int64_t at = memory.FindPattern(kAddRingsPattern);
    if (at == 0) return 0;
    int32_t disp = 0;
    if (!memory.Read(at + 3, &disp, sizeof(disp))) return 0;
    return at + 7 + disp;   // rip-relative: the end of the mov + disp32
}

bool RingService::Enable() {
    if (global == 0) global = FindRaceGlobal(memory);
    if (global == 0) return false;
    on = true;
    Tick();
    return true;
}

void RingService::Tick() {
    if (!on || global == 0) return;
    int64_t race = 0;
    if (!memory.Read(global, &race, sizeof(race)) || race == 0) return;
    const int64_t block = race + kFirstBlock + static_cast<int64_t>(kPlayer) * kBlockSize;
    int32_t rings = 0, max = 0, checksum = 0;
    if (!memory.Read(block + kRings, &rings, 4) || !memory.Read(block + kMax, &max, 4) || !memory.Read(block + kChecksum, &checksum, 4)) return;
    if (max <= 0 || max > 1000 || rings < 0 || rings >= max) return;
    // checksum = key ^ rings: swap the rings inside it first, then the rings
    const int32_t fixed = checksum ^ rings ^ max;
    if (memory.Write(block + kChecksum, &fixed, 4)) memory.Write(block + kRings, &max, 4);
}

} // namespace SonicRacingSaveManager::Memory

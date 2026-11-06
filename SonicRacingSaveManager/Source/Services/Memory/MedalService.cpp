#include "Services/Memory/MedalService.h"

namespace SonicRacingSaveManager::Memory {

namespace {
constexpr const char* kSpendPattern = "80 FA 02 75 2B ?? ?? ?? ?? ?? ?? 48 85 C0 74 50 8B 40 08 C1 E8 1E F6 D0 A8 01 74 44 8B 93 4C 06 00 00";
constexpr int64_t kSiteOffset = 5;
const Bytes kOriginal = { 0x29, 0x8B, 0x4C, 0x06, 0x00, 0x00 };   // sub [rbx+64Ch],ecx
const Bytes kFreeze = Bytes(6, 0x90);
const Bytes kMultiply = { 0x01, 0x8B, 0x4C, 0x06, 0x00, 0x00 };   // add [rbx+64Ch],ecx

void Append(Bytes& b, int32_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    b.insert(b.end(), p, p + 4);
}
} // namespace

bool MedalService::FindSite() {
    if (site) return true;
    const int64_t at = memory.FindPattern(kSpendPattern);
    if (at == 0) return false;
    site = at + kSiteOffset;
    return true;
}

bool MedalService::Write(Mode m) {
    switch (m) {
        case Mode::Off: return memory.WriteCode(site, kOriginal);
        case Mode::Freeze: return memory.WriteCode(site, kFreeze);
        case Mode::Multiply: return memory.WriteCode(site, kMultiply);
        case Mode::Multiply500: {
            if (!cave) {
                cave = memory.AllocateNearModule(0x100);
                if (!cave) return false;
                const int64_t back = site + static_cast<int64_t>(kOriginal.size());
                // imul ecx,ecx,500 / add [rbx+64Ch],ecx / cmp dword [rbx+64Ch],9999999 / jle back /
                // mov dword [rbx+64Ch],9999999 / back: jmp to the game (ecx is free after the sub;
                // the test that follows sets the flags again)
                Bytes code = { 0x69, 0xC9, 0xF4, 0x01, 0x00, 0x00, 0x01, 0x8B, 0x4C, 0x06, 0x00, 0x00,
                               0x81, 0xBB, 0x4C, 0x06, 0x00, 0x00, 0x7F, 0x96, 0x98, 0x00, 0x7E, 0x0A,
                               0xC7, 0x83, 0x4C, 0x06, 0x00, 0x00, 0x7F, 0x96, 0x98, 0x00, 0xE9 };
                Append(code, static_cast<int32_t>(back - (cave + static_cast<int64_t>(code.size()) + 4)));
                if (!memory.WriteCode(cave, code)) {
                    cave = 0;
                    return false;
                }
            }
            Bytes jump = { 0xE9 };
            Append(jump, static_cast<int32_t>(cave - (site + 5)));
            jump.push_back(0x90);
            return memory.WriteCode(site, jump);
        }
    }
    return false;
}

MedalService::Result MedalService::Toggle(Mode m) {
    if (m == Mode::Off || !CanToggle(m)) return Result::EnableFailed;
    if (mode == m) {
        if (!Write(Mode::Off)) return Result::DisableFailed;
        mode = Mode::Off;
        return Result::Disabled;
    }
    if (!FindSite() || !Write(m)) return Result::EnableFailed;
    mode = m;
    return Result::Enabled;
}

void MedalService::RestoreAll() {
    if (site && mode != Mode::Off) Write(Mode::Off);
    mode = Mode::Off;
}

} // namespace SonicRacingSaveManager::Memory

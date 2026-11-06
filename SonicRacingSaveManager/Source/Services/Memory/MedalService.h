#pragma once
#include "Services/Memory/ProcessMemory.h"

// Duel Medal patches, like the ticket ones. The shop's spend routine (module+4CF7AF3 in 1.4.x)
// checks the price against the medals and takes it off:
//   4CF7B1C  80 FA 02           cmp dl,2                 ; currency 2 = Duel Medals
//   4CF7B1F  75 2B              jne (the other currency, +0x644)
//   4CF7B21  29 8B 4C 06 00 00  sub [rbx+64Ch],ecx       ; medals -= price   <- the patched spot
//   4CF7B27  48 85 C0 74 50 ... (pushes [rbx+64Ch] to the UI)
// No checksum or cap there. Found by an AOB (the 6 patched bytes are wildcards), so it survives
// updates that move the code. The three modes patch the same instruction, so only one is on at once:
//   Freeze    6 x NOP                        (buying doesn't take medals)
//   Multiply  add [rbx+64Ch],ecx             (buying gives the price instead)
//   x500      jmp to a cave: medals += price * 500, capped at 9,999,999 (the tickets' cap)
namespace SonicRacingSaveManager::Memory {

class MedalService {
public:
    enum class Mode { Off, Freeze, Multiply, Multiply500 };
    enum class Result { Enabled, Disabled, EnableFailed, DisableFailed };

    explicit MedalService(ProcessMemory& memory) : memory(memory) {}

    Mode Current() const { return mode; }
    bool CanToggle(Mode m) const { return mode == Mode::Off || mode == m; }
    // Turns `m` on (when everything is off) or off (when it's the one on); throws Error when not attached
    Result Toggle(Mode m);
    // Detach: the original instruction back
    void RestoreAll();
    // The game closed: forget everything
    void Reset() {
        mode = Mode::Off;
        site = cave = 0;
    }

private:
    bool FindSite();
    bool Write(Mode m);

    ProcessMemory& memory;
    Mode mode = Mode::Off;
    int64_t site = 0, cave = 0;
};

} // namespace SonicRacingSaveManager::Memory

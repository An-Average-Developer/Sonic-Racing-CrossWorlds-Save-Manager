#pragma once
#include "Services/Memory/ProcessMemory.h"

// The Freeze / Multiply / x500 toggles of MemoryEditorViewModel.cs (ToggleTicketsFreeze,
// ToggleTicketsMultiply, ToggleTicketsMultiply500 and the restore on detach), on top of
// ProcessMemory. Same bytes, same write order. Multiply and x500 patch the same instruction
// (0x4D830DB), so each one is blocked while the other is on (new in the C++ port: in the C# turning
// both on mixed their bytes).
namespace SonicRacingSaveManager::Memory {

class TicketService {
public:
    enum class Result { Enabled, Disabled, EnableFailed, DisableFailed };

    explicit TicketService(ProcessMemory& memory) : memory(memory) {}

    bool IsFrozen() const { return frozen; }
    bool IsMultiplied() const { return multiplied; }
    bool IsMultiply500() const { return multiply500; }
    bool CanToggleMultiply() const { return !multiply500; }
    bool CanToggleMultiply500() const { return !multiplied; }

    // Each one flips its patch; WriteBytes throws Error when not attached
    Result ToggleFreeze();
    Result ToggleMultiply();
    Result ToggleMultiply500();

    // DetachFromProcess: writes the original bytes back for whatever is on, then forgets the states
    void RestoreAll();
    // The game closed: nothing to restore, just forget the states
    void Reset() { frozen = multiplied = multiply500 = false; }

private:
    ProcessMemory& memory;
    bool frozen = false, multiplied = false, multiply500 = false;
};

} // namespace SonicRacingSaveManager::Memory

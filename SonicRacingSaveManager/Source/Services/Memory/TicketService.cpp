#include "Services/Memory/TicketService.h"
#include "Services/Memory/TicketPatches.h"

namespace SonicRacingSaveManager::Memory {

using namespace TicketPatches;

TicketService::Result TicketService::ToggleFreeze() {
    if (!frozen) {
        // Write NOP bytes to freeze
        if (!memory.WriteBytes(TICKET_FREEZE_ADDRESS, FREEZE_BYTES)) return Result::EnableFailed;
        frozen = true;
        return Result::Enabled;
    }
    // Restore original bytes to unfreeze
    if (!memory.WriteBytes(TICKET_FREEZE_ADDRESS, ORIGINAL_BYTES)) return Result::DisableFailed;
    frozen = false;
    return Result::Disabled;
}

TicketService::Result TicketService::ToggleMultiply() {
    if (!multiplied) {
        if (!memory.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_BYTES)) return Result::EnableFailed;
        multiplied = true;
        return Result::Enabled;
    }
    if (!memory.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_ORIGINAL_BYTES)) return Result::DisableFailed;
    multiplied = false;
    return Result::Disabled;
}

TicketService::Result TicketService::ToggleMultiply500() {
    if (!multiply500) {
        // Write cave first, then JMP patch (safest order)
        bool success = memory.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_BYTES);
        if (success) success = memory.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_BYTES);
        if (!success) {
            memory.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);
            return Result::EnableFailed;
        }
        multiply500 = true;
        return Result::Enabled;
    }
    // Restore patch first so game stops jumping to cave, then wipe cave
    const bool success = memory.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_ORIGINAL);
    memory.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);
    if (!success) return Result::DisableFailed;
    multiply500 = false;
    return Result::Disabled;
}

void TicketService::RestoreAll() {
    // Restore original bytes if frozen
    if (frozen) memory.WriteBytes(TICKET_FREEZE_ADDRESS, ORIGINAL_BYTES);
    if (multiplied) memory.WriteBytes(TICKET_MULTIPLY_ADDRESS, MULTIPLY_ORIGINAL_BYTES);
    if (multiply500) {
        memory.WriteBytes(TICKET_MULTIPLY500_PATCH_ADDRESS, MULTIPLY500_PATCH_ORIGINAL);
        memory.WriteBytes(TICKET_MULTIPLY500_CAVE_ADDRESS, MULTIPLY500_CAVE_ORIGINAL);
    }
    Reset();
}

} // namespace SonicRacingSaveManager::Memory

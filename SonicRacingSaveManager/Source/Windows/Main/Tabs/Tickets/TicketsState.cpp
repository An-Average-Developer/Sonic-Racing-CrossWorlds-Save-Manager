#include "Windows/Main/Tabs/Tickets/TicketsState.h"
#include "Core/Time.h"
#include "Localization/Localization.h"
#include "Windows/Dialogs/MessageDialog.h"

namespace SonicRacingSaveManager::Main {

namespace {
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Result = Memory::TicketService::Result;
} // namespace

TicketsState::TicketsState(HWND ownerWindow) : owner(ownerWindow) {
    statusMessage = Loc::Get("tickets_status_searching");
    tickets.name = L"Tickets";
    tickets.description = L"In-game currency for purchases";
    if (isAutoAttachEnabled) {
        StartAutoAttachTimer();
        TryAutoAttach();
    }
}

TicketsState::~TicketsState() {
    StopUpdateTimer();
    StopAutoAttachTimer();
}

void TicketsState::ShowError(const std::wstring& text, const std::wstring& title) {
    Dialogs::Message(owner, text, title, MessageButtons::Ok, MessageIcon::Error);
}

// ── Timers ──────────────────────────────────────────────────────────────────

void TicketsState::StartUpdateTimer() {
    if (!updateTimer) updateTimer = Async::Every(1000, [this] { UpdateTimerTick(); });
}

void TicketsState::StopUpdateTimer() {
    if (updateTimer) Async::CancelTimer(updateTimer);
    updateTimer = 0;
}

void TicketsState::StartAutoAttachTimer() {
    if (!autoAttachTimer) autoAttachTimer = Async::Every(2000, [this] { TryAutoAttach(); });
}

void TicketsState::StopAutoAttachTimer() {
    if (autoAttachTimer) Async::CancelTimer(autoAttachTimer);
    autoAttachTimer = 0;
}

// While attached the timer always watches the game (as in the Inazuma editor's port, closing the
// game is noticed even with Auto-refresh off); the values are re-read only with Auto-refresh on
void TicketsState::UpdateTimerTick() {
    if (!isAttached) return;
    if (!memoryService.IsProcessRunning()) {
        StopUpdateTimer();
        ticketService.Reset();
        memoryService.DetachFromProcess();
        isAttached = false;
        ++searchGeneration;
        ResetTicketSearch();
        if (isAutoAttachEnabled) {
            statusMessage = Loc::Get("tickets_status_game_closed_waiting");
            StartAutoAttachTimer();
        } else {
            statusMessage = Loc::Get("tickets_status_game_closed");
        }
        return;
    }
    if (autoRefresh) RefreshValues();
}

void TicketsState::SetAutoRefresh(bool value) { autoRefresh = value; }

// ── Attach / detach ─────────────────────────────────────────────────────────

void TicketsState::Attached(const std::wstring& status) {
    isAttached = true;
    statusMessage = status;
    StopAutoAttachTimer();
    RefreshValues();
    StartUpdateTimer();
}

void TicketsState::TryAutoAttach() {
    if (!isAutoAttachEnabled || isAttached) return;
    if (memoryService.AttachToProcess()) Attached(Loc::Get("tickets_status_attached"));
    else statusMessage = Loc::Get("tickets_status_waiting");
}

void TicketsState::AttachToProcess() {
    if (memoryService.AttachToProcess()) Attached(Loc::Get("tickets_status_attached"));
    else statusMessage = Loc::Get("tickets_status_attach_failed");
}

void TicketsState::ToggleAutoAttach() {
    isAutoAttachEnabled = !isAutoAttachEnabled;
    if (isAttached) return;
    if (isAutoAttachEnabled) {
        StartAutoAttachTimer();
        TryAutoAttach();
    } else {
        StopAutoAttachTimer();
        RefreshDetectedProcesses();
        statusMessage = Loc::Get("tickets_status_auto_off");
    }
}

void TicketsState::RefreshDetectedProcesses() {
    detectedProcesses.clear();
    for (const auto& p : Memory::ProcessMemory::GetCandidateProcesses())
        detectedProcesses.push_back({ p.pid, p.processName + L".exe  (PID: " + std::to_wstring(p.pid) + L")" });
    // The ListBox keeps its selection only while that process is still listed
    if (selectedGameProcess) {
        bool found = false;
        for (const auto& p : detectedProcesses) found = found || p.id == *selectedGameProcess;
        if (!found) selectedGameProcess.reset();
    }
}

void TicketsState::AttachToSelectedProcess() {
    if (!selectedGameProcess) return;
    if (memoryService.AttachToProcess(*selectedGameProcess)) Attached(Loc::Fmt("tickets_status_attached_to", memoryService.ProcessName()));
    else statusMessage = Loc::Get("tickets_status_attach_selected_failed");
}

void TicketsState::DetachFromProcess() {
    try {
        StopUpdateTimer();
        // Restore the original bytes of whatever patch is on
        ticketService.RestoreAll();
        memoryService.DetachFromProcess();
        isAttached = false;
        ++searchGeneration;
        ResetTicketSearch();
        if (isAutoAttachEnabled) {
            statusMessage = Loc::Get("tickets_status_detached_searching");
            StartAutoAttachTimer();
        } else {
            statusMessage = Loc::Get("tickets_status_detached");
        }
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_detach_error", e.Message());
    }
}

// ── Values ──────────────────────────────────────────────────────────────────

void TicketsState::RefreshValues() {
    if (!isAttached) return;
    try {
        if (tickets.address != 0) {
            const int32_t value = memoryService.ReadInt32(tickets.address);
            tickets.currentValue = value;
            if (tickets.newValue == 0 || tickets.newValue == tickets.currentValue) tickets.newValue = value;
        }
        if (IsTicketLocated()) statusMessage = Loc::Fmt("tickets_status_refreshed", Time::Clock(Time::Now()));
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_read_error", e.Message());
    }
}

void TicketsState::ApplyValue() {
    if (!valueSelected || !isAttached || tickets.address == 0) return;
    try {
        if (memoryService.WriteInt32(tickets.address, tickets.newValue)) {
            tickets.currentValue = tickets.newValue;
            statusMessage = Loc::Fmt("tickets_status_updated", tickets.name, tickets.newValue);
            if (onValueApplied) onValueApplied();
        } else {
            statusMessage = Loc::Fmt("tickets_status_update_failed", tickets.name);
            ShowError(Loc::Fmt("tickets_update_failed_message", tickets.name), Loc::Get("tickets_update_failed_title"));
        }
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_write_error", e.Message());
        ShowError(Loc::Fmt("tickets_write_error_message", e.Message()), Loc::Get("dialog_error"));
    }
}

// ── Patches ─────────────────────────────────────────────────────────────────

void TicketsState::ToggleTicketsFreeze() {
    if (!isAttached) return;
    try {
        switch (ticketService.ToggleFreeze()) {
            case Result::Enabled: statusMessage = Loc::Get("tickets_status_frozen"); break;
            case Result::Disabled: statusMessage = Loc::Get("tickets_status_unfrozen"); break;
            case Result::EnableFailed:
                statusMessage = Loc::Get("tickets_status_freeze_failed");
                ShowError(Loc::Get("tickets_freeze_failed_message"), Loc::Get("tickets_freeze_failed_title"));
                break;
            case Result::DisableFailed:
                statusMessage = Loc::Get("tickets_status_unfreeze_failed");
                ShowError(Loc::Get("tickets_unfreeze_failed_message"), Loc::Get("tickets_unfreeze_failed_title"));
                break;
        }
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_freeze_error", e.Message());
        ShowError(Loc::Fmt("tickets_freeze_error_message", e.Message()), Loc::Get("dialog_error"));
    }
}

void TicketsState::ToggleTicketsMultiply() {
    if (!CanToggleMultiply()) return;
    try {
        switch (ticketService.ToggleMultiply()) {
            case Result::Enabled: statusMessage = Loc::Get("tickets_status_multiply_on"); break;
            case Result::Disabled: statusMessage = Loc::Get("tickets_status_multiply_off"); break;
            case Result::EnableFailed:
                statusMessage = Loc::Get("tickets_status_multiply_failed");
                ShowError(Loc::Get("tickets_multiply_failed_message"), Loc::Get("tickets_multiply_failed_title"));
                break;
            case Result::DisableFailed:
                statusMessage = Loc::Get("tickets_status_multiply_disable_failed");
                ShowError(Loc::Get("tickets_multiply_disable_failed_message"), Loc::Get("tickets_disable_failed_title"));
                break;
        }
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_multiply_error", e.Message());
        ShowError(Loc::Fmt("tickets_multiply_error_message", e.Message()), Loc::Get("dialog_error"));
    }
}

void TicketsState::ToggleTicketsMultiply500() {
    if (!CanToggleMultiply500()) return;
    try {
        switch (ticketService.ToggleMultiply500()) {
            case Result::Enabled: statusMessage = Loc::Get("tickets_status_x500_on"); break;
            case Result::Disabled: statusMessage = Loc::Get("tickets_status_x500_off"); break;
            case Result::EnableFailed:
                statusMessage = Loc::Get("tickets_status_x500_failed");
                ShowError(Loc::Get("tickets_x500_failed_message"), Loc::Get("tickets_x500_failed_title"));
                break;
            case Result::DisableFailed:
                statusMessage = Loc::Get("tickets_status_x500_disable_failed");
                ShowError(Loc::Get("tickets_x500_disable_failed_message"), Loc::Get("tickets_disable_failed_title"));
                break;
        }
    } catch (const Memory::Error& e) {
        statusMessage = Loc::Fmt("tickets_status_x500_error", e.Message());
        ShowError(Loc::Fmt("tickets_x500_error_message", e.Message()), Loc::Get("dialog_error"));
    }
}

} // namespace SonicRacingSaveManager::Main

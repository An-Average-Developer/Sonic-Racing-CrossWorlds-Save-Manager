#pragma once
#include <windows.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Core/Async.h"
#include "Models/MemoryValue.h"
#include "Services/Memory/ProcessMemory.h"
#include "Services/Memory/MedalService.h"
#include "Services/Memory/RingService.h"
#include "Services/Memory/SpeedService.h"
#include "Services/Memory/TicketService.h"

namespace SonicRacingSaveManager::Main {

// Features\MemoryEditor\ViewModels\MemoryEditorViewModel.cs: attaching to the game (Auto-Attach
// every 2 s, or a process picked by hand), the 1 s refresh, the Freeze / Multiply / x500 toggles and
// the guided ticket search. Split:
//   TicketsState.cpp    attach / detach, timers, refresh, apply, the patch toggles
//   TicketsSearch.cpp   Find Tickets (the 6 steps, search / next search)
class TicketsState {
public:
    explicit TicketsState(HWND owner);
    ~TicketsState();
    TicketsState(const TicketsState&) = delete;
    TicketsState& operator=(const TicketsState&) = delete;

    // ── State (the view model's properties) ──
    bool isAttached = false;
    std::wstring statusMessage;
    bool autoRefresh = true;
    bool isAutoAttachEnabled = true;
    std::wstring selectedTool = L"menu";   // "menu" | "tickets"
    Models::MemoryValue tickets;           // MemoryValues[0]
    bool valueSelected = false;            // SelectedValue == Tickets
    std::vector<Models::GameProcessInfo> detectedProcesses;
    std::optional<uint32_t> selectedGameProcess;   // pid

    bool IsTicketsFrozen() const { return ticketService.IsFrozen(); }
    bool IsTicketsMultiplied() const { return ticketService.IsMultiplied(); }
    bool IsTicketsMultiply500() const { return ticketService.IsMultiply500(); }
    bool HasDetectedProcesses() const { return !detectedProcesses.empty(); }

    // ── Commands ──
    void AttachToProcess();
    bool CanAttachToProcess() const { return !isAttached; }
    void DetachFromProcess();
    void ToggleAutoAttach();
    void RefreshDetectedProcesses();
    bool CanAttachToSelectedProcess() const { return !isAttached && selectedGameProcess.has_value(); }
    void AttachToSelectedProcess();
    void RefreshValues();
    bool CanApplyValue() const { return isAttached && valueSelected && tickets.address != 0; }
    void ApplyValue();
    void SetAutoRefresh(bool value);
    // Toggles (CanExecute = attached; Multiply and x500 also wait for the other one to be off)
    bool CanToggleFreeze() const { return isAttached; }
    bool CanToggleMultiply() const { return isAttached && ticketService.CanToggleMultiply(); }
    bool CanToggleMultiply500() const { return isAttached && ticketService.CanToggleMultiply500(); }
    void ToggleTicketsFreeze();
    void ToggleTicketsMultiply();
    void ToggleTicketsMultiply500();
    // Duel Medal patches (MedalService): Freeze / Multiply / x500, one at a time (same instruction)
    using MedalMode = Memory::MedalService::Mode;
    bool IsMedals(MedalMode m) const { return medalService.Current() == m; }
    bool CanToggleMedals(MedalMode m) const { return isAttached && medalService.CanToggle(m); }
    void ToggleMedals(MedalMode m);
    // Max rings in races (RingService): racer 0 kept at 100 rings every 100 ms while on
    bool IsMaxRings() const { return ringService.IsOn(); }
    bool CanToggleMaxRings() const { return isAttached; }
    void ToggleMaxRings();
    // Top speed in races (SpeedService): the player's top speed kept at SpeedLimit() while on
    bool IsSpeedLimit() const { return speedService.IsOn(); }
    bool CanToggleSpeedLimit() const { return isAttached; }
    void ToggleSpeedLimit();
    float SpeedLimit() const { return speedService.Limit(); }
    void SetSpeedLimit(float kmh);
    float PlayerSpeed() const { return speedService.SpeedKmh(); }
    float PlayerTopSpeed() const { return speedService.TopKmh(); }
    bool SearchingSpeed() const { return speedService.Searching(); }

    // ── Find Tickets (TicketsSearch.cpp) ──
    // 1 = go to Friendship, 2 = type tickets + Search, 3 = go to Customize Machine, 4 = back to
    // Friendship, 5 = spend 1 ticket + Search, 6 = found
    int ticketSearchStep = 1;
    int32_t ticketSearchValue = 0;   // the amount typed for the current step
    std::wstring ticketSearchHint;   // extra result info (match count, errors), "" when nothing to add
    bool isTicketSearching = false;
    bool IsTicketLocated() const { return tickets.address != 0; }
    bool IsTicketNextStep() const { return ticketSearchStep == 1 || ticketSearchStep == 3 || ticketSearchStep == 4; }
    bool IsTicketSearchInputStep() const { return ticketSearchStep == 2 || ticketSearchStep == 5; }
    std::wstring TicketStepTitle() const;
    std::wstring TicketStepText() const;
    bool CanSearchTickets() const { return isAttached && !isTicketSearching; }
    void SearchTickets();
    bool CanNewTicketSearch() const { return isAttached && !isTicketSearching; }
    void ResetTicketSearch();
    void NextTicketStep();

    // ValueAppliedDialog: shown by the tab when a value was written
    std::function<void()> onValueApplied;

private:
    void Attached(const std::wstring& status);
    void StartUpdateTimer();
    void StopUpdateTimer();
    void StartAutoAttachTimer();
    void StopAutoAttachTimer();
    void UpdateTimerTick();
    void TryAutoAttach();
    void StopRings();
    void StopSpeed();
    void ShowError(const std::wstring& text, const std::wstring& title);
    void SetTicketAddress(int64_t address) { tickets.address = address; }

    HWND owner;
    Memory::ProcessMemory memoryService;
    Memory::TicketService ticketService{ memoryService };
    Memory::MedalService medalService{ memoryService };
    Memory::RingService ringService{ memoryService };
    Async::TimerId ringTimer = 0;
    Memory::SpeedService speedService{ memoryService };
    Async::TimerId speedTimer = 0;
    std::optional<std::vector<int64_t>> ticketCandidates;
    Async::TimerId updateTimer = 0, autoAttachTimer = 0;
    // Bumped on detach: a search finishing after it is ignored
    unsigned searchGeneration = 0;
};

} // namespace SonicRacingSaveManager::Main

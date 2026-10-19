#include <cwchar>
#include "Core/Async.h"
#include "Localization/Localization.h"
#include "Windows/Main/Tabs/Tickets/TicketsState.h"

// Find Tickets: Cheat Engine style — search the amount you have, change it in the game (spend a
// ticket after a menu round-trip), search the new amount among the previous matches, until one
// address is left.
namespace SonicRacingSaveManager::Main {

namespace {

// {count:N0}: thousands separators of the user's locale
std::wstring Grouped(size_t count) {
    const std::wstring digits = std::to_wstring(count);
    wchar_t out[64]{};
    NUMBERFMTW format{};
    wchar_t decimal[8] = L".", thousand[8] = L",";
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal, 8);
    GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousand, 8);
    format.Grouping = 3;
    format.lpDecimalSep = decimal;
    format.lpThousandSep = thousand;
    if (GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, digits.c_str(), &format, out, 64) > 0) return out;
    return digits;
}

} // namespace

std::wstring TicketsState::TicketStepTitle() const {
    return ticketSearchStep == 6 ? Loc::Get("tickets_step_done") : Loc::Fmt("tickets_step_of", ticketSearchStep, 5);
}

std::wstring TicketsState::TicketStepText() const {
    switch (ticketSearchStep) {
        case 1: return Loc::Get("tickets_step1");
        case 2: return Loc::Get("tickets_step2");
        case 3: return Loc::Get("tickets_step3");
        case 4: return Loc::Get("tickets_step4");
        case 5: return Loc::Get("tickets_step5");
        default: return Loc::Get("tickets_step6");
    }
}

void TicketsState::SearchTickets() {
    if (!isAttached || isTicketSearching) return;
    const int32_t value = ticketSearchValue;
    const std::optional<std::vector<int64_t>> previous = ticketSearchStep == 5 ? ticketCandidates : std::nullopt;
    isTicketSearching = true;
    ticketSearchHint = Loc::Get("tickets_searching");
    struct Outcome {
        std::vector<int64_t> matches;
        bool ok = false;
        std::wstring error;
    };
    const unsigned generation = searchGeneration;
    Async::Run(
        [this, value, previous]() {
            Outcome o;
            try {
                o.matches = previous ? memoryService.FilterInt32(*previous, value) : memoryService.ScanForInt32(value);
                o.ok = true;
            } catch (const Memory::Error& e) {
                o.error = e.Message();
            }
            return o;
        },
        [this, previous, generation](Outcome o) {
            isTicketSearching = false;
            if (generation != searchGeneration) return;   // detached meanwhile
            if (!o.ok) {
                ticketSearchHint = Loc::Get("tickets_search_failed");
                statusMessage = Loc::Fmt("tickets_status_search_error", o.error);
                return;
            }
            if (o.matches.size() == 1) {
                ticketCandidates.reset();
                SetTicketAddress(o.matches[0]);
                ticketSearchStep = 6;
                ticketSearchHint.clear();
                statusMessage = Loc::Get("tickets_status_located");
                RefreshValues();
            } else if (o.matches.empty()) {
                ticketCandidates.reset();
                ticketSearchStep = previous ? 1 : 2;
                ticketSearchHint = Loc::Get(previous ? "tickets_search_restarted" : "tickets_search_nothing");
                statusMessage = Loc::Get("tickets_status_search_nothing");
            } else {
                // Still several candidates: repeat the menu round-trip and spend another ticket
                const std::wstring count = Grouped(o.matches.size());
                ticketCandidates = std::move(o.matches);
                ticketSearchStep = 3;
                ticketSearchHint = Loc::Fmt("tickets_search_left", count);
                statusMessage = Loc::Fmt("tickets_status_search_matches", count);
            }
        });
}

void TicketsState::ResetTicketSearch() {
    ticketCandidates.reset();
    SetTicketAddress(0);
    tickets.currentValue = 0;
    tickets.newValue = 0;
    ticketSearchStep = 1;
    ticketSearchHint.clear();
}

void TicketsState::NextTicketStep() {
    if (!IsTicketNextStep()) return;
    ticketSearchHint.clear();
    ++ticketSearchStep;
}

} // namespace SonicRacingSaveManager::Main

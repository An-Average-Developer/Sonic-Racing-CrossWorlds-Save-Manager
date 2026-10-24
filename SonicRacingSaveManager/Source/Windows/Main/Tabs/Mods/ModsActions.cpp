#include <algorithm>
#include "Localization/Localization.h"
#include "Windows/Dialogs/MessageDialog.h"
#include "Windows/Main/Tabs/Mods/ModsState.h"

// Priority mode (the "01 - " folder prefixes), Move Up / Move Down and drag-and-drop (HandleDrop: in
// the C# it was written but never wired to the list; the C++ list drags rows in priority mode).
namespace SonicRacingSaveManager::Main {

namespace {
using Dialogs::MessageButtons;
using Dialogs::MessageIcon;
using Dialogs::MessageResult;
namespace Mods = Services::Mods;
} // namespace

void ModsState::TogglePriorityMode() {
    if (!service || ShowGameRunningWarning()) return;
    const bool disable = isPriorityMode;
    Dialogs::Message(owner, Loc::Get(disable ? "mods_priority_disable_confirm" : "mods_priority_enable_confirm"),
                     Loc::Get(disable ? "mods_priority_disable_title" : "mods_priority_enable_title"), MessageButtons::YesNo, MessageIcon::Question,
                     [this, disable](MessageResult result) {
                         if (result != MessageResult::Yes || isReordering) return;
                         isReordering = true;
                         ++reorders;
                         const Mods::ModManager copy = *service;
                         RunOnCopies(
                             [copy, disable](std::vector<Models::ModInfo>& list, std::wstring&) {
                                 if (disable) copy.DisablePriorityMode(list);
                                 else copy.EnablePriorityMode(list);
                                 return 1;
                             },
                             [this, disable](int) {
                                 isPriorityMode = !disable;
                                 statusMessage = Loc::Get(disable ? "mods_status_priority_off" : "mods_status_priority_on");
                                 RefreshMods({}, false);
                             },
                             "mods_priority_failed");
                     });
}

// Mods.Move(from, to) + ReorderMods: the rows move at once (they slide to their places), the
// folders are renamed on a worker thread under the list's "Reordering" veil
void ModsState::Reorder(int from, int to, const std::wstring& status) {
    if (isReordering) return;
    Models::ModInfo moved = std::move(mods[static_cast<size_t>(from)]);
    mods.erase(mods.begin() + from);
    mods.insert(mods.begin() + to, std::move(moved));
    isReordering = true;
    ++reorders;
    const Mods::ModManager copy = *service;
    RunOnCopies(
        [copy](std::vector<Models::ModInfo>& list, std::wstring&) {
            copy.ReorderMods(list);
            return 1;
        },
        [this, status](int) { statusMessage = status; }, "mods_move_failed");
}

void ModsState::MoveModUp() {
    const int index = SelectedIndex();
    if (index < 0 || !isPriorityMode || !service) return;
    if (ShowGameRunningWarning()) return;
    if (index > 0) Reorder(index, index - 1, Loc::Fmt("mods_status_moved_up", mods[static_cast<size_t>(index)].name));
}

void ModsState::MoveModDown() {
    const int index = SelectedIndex();
    if (index < 0 || !isPriorityMode || !service) return;
    if (ShowGameRunningWarning()) return;
    if (index < static_cast<int>(mods.size()) - 1) Reorder(index, index + 1, Loc::Fmt("mods_status_moved_down", mods[static_cast<size_t>(index)].name));
}

void ModsState::HandleDrop(int droppedIndex, int targetIndex) {
    if (!isPriorityMode || !service) return;
    if (ShowGameRunningWarning()) return;
    const int count = static_cast<int>(mods.size());
    if (droppedIndex < 0 || targetIndex < 0 || droppedIndex >= count || targetIndex >= count || droppedIndex == targetIndex) return;
    const std::wstring name = mods[static_cast<size_t>(droppedIndex)].name;
    Reorder(droppedIndex, targetIndex, Loc::Fmt("mods_status_moved_to", name, targetIndex + 1));
}

} // namespace SonicRacingSaveManager::Main

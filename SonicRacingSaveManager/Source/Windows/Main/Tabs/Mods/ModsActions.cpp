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
                         if (result != MessageResult::Yes) return;
                         try {
                             if (disable) service->DisablePriorityMode(mods);
                             else service->EnablePriorityMode(mods);
                             isPriorityMode = !disable;
                             statusMessage = Loc::Get(disable ? "mods_status_priority_off" : "mods_status_priority_on");
                             RefreshMods();
                         } catch (const Mods::Error& e) {
                             Dialogs::Message(owner, Loc::Fmt("mods_priority_failed", e.Message()), Loc::Get("dialog_error"), MessageButtons::Ok,
                                              MessageIcon::Error);
                         }
                     });
}

// Mods.Move(from, to) + ReorderMods: the folders are renamed to their new positions
void ModsState::Reorder(int from, int to, const std::wstring& status) {
    try {
        Models::ModInfo moved = std::move(mods[static_cast<size_t>(from)]);
        mods.erase(mods.begin() + from);
        mods.insert(mods.begin() + to, std::move(moved));
        service->ReorderMods(mods);
        ++revision;
        statusMessage = status;
    } catch (const Mods::Error& e) {
        Dialogs::Message(owner, Loc::Fmt("mods_move_failed", e.Message()), Loc::Get("dialog_error"), MessageButtons::Ok, MessageIcon::Error);
        RefreshMods();
    }
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

#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Replacement for System.Windows.MessageBox: a MaterialDesign dialog (themed card, title, text,
// flat buttons) centered on its owner. Asynchronous like the other dialogs: the owner is disabled
// until it closes and `done` runs afterwards on the UI thread.
namespace SonicRacingSaveManager::Dialogs {

enum class MessageButtons { Ok, OkCancel, YesNo, YesNoCancel };
enum class MessageIcon { None, Information, Warning, Error, Question };
enum class MessageResult { Ok, Cancel, Yes, No };

void Message(HWND owner, const std::wstring& text, const std::wstring& title, MessageButtons buttons = MessageButtons::Ok,
             MessageIcon icon = MessageIcon::None, std::function<void(MessageResult)> done = {});
// MessageBox.Show(text, title) with OK + Information is by far the most common call
inline void Info(HWND owner, const std::wstring& text, const std::wstring& title) {
    Message(owner, text, title, MessageButtons::Ok, MessageIcon::Information);
}
// From a worker thread: shows it on the UI thread and waits for the answer.
MessageResult MessageBlocking(HWND owner, const std::wstring& text, const std::wstring& title,
                              MessageButtons buttons = MessageButtons::Ok, MessageIcon icon = MessageIcon::None);

bool AnyMessageOpen();

} // namespace SonicRacingSaveManager::Dialogs

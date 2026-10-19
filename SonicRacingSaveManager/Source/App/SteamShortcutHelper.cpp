#include "App/SteamShortcutHelper.h"
#include <windows.h>
#include <string>
#include "App/MessageLoop.h"
#include "Core/Async.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SonicRacingSaveManager::App {

namespace {

void ShowResult(const std::wstring& message, bool success) {
    Dialogs::Options o;
    o.title = Loc::Get("steam_helper_title");
    o.close = Loc::Get("ok");
    Dialogs::ShowText(nullptr, o, message, success ? Rgba(0, 204, 170, 200) : Rgba(160, 180, 200, 200),
                      [](Dialogs::Result) { PostQuitMessage(0); });
}

} // namespace

int RunSteamShortcutHelper(int argc, wchar_t** argv) {
    if (argc < 3) {
        // argc counts the exe too; the C# args array didn't
        MessageBoxW(nullptr, Loc::Fmt("steam_helper_bad_args", argc - 1).c_str(), L"Add to Steam", MB_OK | MB_ICONERROR);
        return 1;
    }
    const std::wstring exePath = argv[2], iconPath = argc > 3 ? argv[3] : L"";
    namespace S = Services::SteamShortcuts;

    const bool wasRunning = S::IsSteamRunning();
    Dialogs::Options o;
    o.title = Loc::Get("steam_helper_title");
    o.primary = Loc::Get("steam_helper_title");
    o.close = Loc::Get("cancel");
    Dialogs::ShowText(nullptr, o, Loc::Get(wasRunning ? "steam_helper_warning_running" : "steam_helper_warning_not_running"),
                      Rgba(232, 160, 0, 200), [=](Dialogs::Result result) {
                          if (result != Dialogs::Result::Primary) {
                              PostQuitMessage(0);
                              return;
                          }
                          // Closing / restarting Steam takes seconds: off the UI thread
                          Async::Run([=]() -> std::pair<std::wstring, bool> {
                              const auto steamPath = S::FindSteamPath();
                              if (!steamPath) return { Loc::Fmt("steam_helper_error", L"Steam installation not found."), false };
                              if (wasRunning) S::CloseSteam(*steamPath);
                              const std::wstring error = S::AddOrUpdateShortcut(exePath, iconPath);
                              // (the C# restarted Steam only when the shortcut was written)
                              if (!error.empty()) return { Loc::Fmt("steam_helper_error", error), false };
                              if (wasRunning) S::StartSteam(*steamPath);
                              return { Loc::Get("steam_helper_success"), true };
                          }, [](std::pair<std::wstring, bool> r) { ShowResult(r.first, r.second); });
                      });
    return App::RunMessageLoop();
}

} // namespace SonicRacingSaveManager::App

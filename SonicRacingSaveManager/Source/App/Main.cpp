#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include "App/MessageLoop.h"
#include "App/SteamShortcutHelper.h"
#include "Core/Async.h"
#include "Graphics/Device.h"
#include "Graphics/Fonts.h"
#include "Localization/Localization.h"
#include "Services/App/Log.h"
#include "Services/App/ResourceExtractor.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/MainWindow.h"

using namespace SonicRacingSaveManager;

namespace {

struct Args {
    int count = 0;
    wchar_t** values = nullptr;
    Args() { values = CommandLineToArgvW(GetCommandLineW(), &count); }
    ~Args() { LocalFree(values); }
    bool Is(int i, const wchar_t* text) const { return i < count && std::wstring(values[i]) == text; }
};

} // namespace

// App.xaml.cs
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    Args args;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Async::Initialize();
    Device::Get().Init();

    int code = 0;
    Services::ResourceExtractor::Extract();   // SteamAssetExtractor.Extract()
    Loc::LoadSaved();                          // the Add-to-Steam helper's texts too
    Fonts::Initialize();
    if (args.Is(1, App::SteamShortcutHelperArg)) {
        code = App::RunSteamShortcutHelper(args.count, args.values);
    } else {
        Log::Start();
        UI::Material::SetDarkTheme(true);   // themeService.IsDarkTheme = true
        {
            // The loading screen is handled inside MainWindow
            Main::MainWindow window;
            if (window.Open()) code = App::RunMessageLoop();
            else Log::Error(L"The main window could not be opened");
        }
        Log::Info(L"Manager closed");
    }
    Async::Shutdown();
    Device::Get().Shutdown();
    CoUninitialize();
    return code;
}

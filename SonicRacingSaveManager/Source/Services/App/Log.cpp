#include "Services/App/Log.h"
#include <windows.h>
#include <cstdio>
#include <mutex>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/AppVersion.h"

namespace SonicRacingSaveManager::Log {

namespace {

constexpr uint64_t kMaxSize = 2 * 1024 * 1024;   // per session; past it the log stops growing

std::mutex mutex;
bool started = false, full = false;

std::wstring Folder() { return Files::Combine(Paths::DataDir(), L"logs"); }
std::wstring Current() { return Files::Combine(Folder(), L"manager.log"); }
std::wstring Previous() { return Files::Combine(Folder(), L"manager-previous.log"); }
// The updater's log, kept in its temp folder when an update failed (UpdateService)
std::wstring UpdateLog() { return Files::Combine(Files::TempDirectory(), std::wstring(Config::AppFolder) + L"_Update", L"update.log"); }

std::wstring Timestamp() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t text[40];
    swprintf_s(text, L"%04u-%02u-%02u %02u:%02u:%02u.%03u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return text;
}

std::wstring SystemInfo() {
    std::wstring info;
    info += L"Manager: " + AppVersion::DisplayVersion();
#ifdef _DEBUG
    info += L" (Debug)";
#endif
    info += L"\r\nWindows build: " + std::to_wstring(System::WindowsBuild());
    info += L"\r\nRunning as administrator: " + std::wstring(System::IsElevated() ? L"yes" : L"no");
    info += L"\r\nExecutable: " + Files::FileName(Files::ExecutablePath());
    info += L"\r\nTime: " + Timestamp() + L"\r\n";
    return info;
}

} // namespace

void Start() {
    std::lock_guard lock(mutex);
    Files::CreateDirectories(Folder());
    if (Files::Exists(Current())) Files::Move(Current(), Previous(), true);
    started = Files::WriteText(Current(), L"=== Sonic Racing CrossWorlds Save Manager log ===\r\n" + SystemInfo() + L"\r\n");
    full = false;
}

void Write(Level level, const std::wstring& message) {
    std::lock_guard lock(mutex);
    if (!started || full) return;
    if (Files::Size(Current()).value_or(0) > kMaxSize) {
        Files::AppendText(Current(), Timestamp() + L"  [INFO ]  (log size limit reached, nothing more is written this session)\r\n");
        full = true;
        return;
    }
    const wchar_t* tag = level == Level::Error ? L"[ERROR]" : level == Level::Warning ? L"[WARN ]" : L"[INFO ]";
    std::wstring line = message;
    for (auto& c : line)
        if (c == L'\r' || c == L'\n') c = L' ';
    Files::AppendText(Current(), Timestamp() + L"  " + tag + L"  " + line + L"\r\n");
}

std::wstring Text() {
    std::lock_guard lock(mutex);
    std::wstring text = Files::ReadText(Current()).value_or(L"(no log for this session)");
    if (auto previous = Files::ReadText(Previous())) text += L"\r\n\r\n=== Previous session ===\r\n" + *previous;
    return text;
}

std::string Zip() {
    std::lock_guard lock(mutex);
    Zip::Writer zip;
    zip.Add(L"system.txt", Text::Narrow(SystemInfo()));
    if (auto bytes = Files::ReadBytes(Current())) zip.Add(L"manager.log", *bytes);
    if (auto bytes = Files::ReadBytes(Previous())) zip.Add(L"manager-previous.log", *bytes);
    if (auto bytes = Files::ReadBytes(UpdateLog())) zip.Add(L"update.log", *bytes);
    return zip.Finish();
}

} // namespace SonicRacingSaveManager::Log

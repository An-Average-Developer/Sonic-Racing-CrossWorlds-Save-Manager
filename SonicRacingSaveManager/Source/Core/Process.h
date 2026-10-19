#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Process helpers (System.Diagnostics.Process replacement).
namespace SonicRacingSaveManager::Process {

struct Info {
    uint32_t pid = 0;
    std::wstring name;        // "Poppy_Playtime-Win64-Shipping.exe"
};

std::vector<Info> List();
// Like Process.GetProcessesByName: `name` without ".exe", case-insensitive.
std::vector<uint32_t> FindByName(const std::wstring& nameWithoutExe);
// Same, in a list taken earlier (several lookups share one snapshot of the process table)
std::vector<uint32_t> FindByName(const std::vector<Info>& processes, const std::wstring& nameWithoutExe);
std::wstring ImagePath(uint32_t pid);                     // full exe path ("" when not accessible)
bool IsAlive(uint32_t pid);
bool Kill(uint32_t pid);
bool SetPriority(uint32_t pid, unsigned long priorityClass);
uint32_t ForegroundPid();
uint32_t CurrentPid();

// ShellExecute "open" (URLs, folders, documents) — Process.Start(new ProcessStartInfo(x) { UseShellExecute = true })
bool Open(const std::wstring& target, const std::wstring& arguments = L"", const std::wstring& workingDir = L"");
bool OpenUrl(const std::wstring& url);
bool ShowInExplorer(const std::wstring& path);   // explorer /select,
// Starts a program; returns its pid (0 on failure).
uint32_t Start(const std::wstring& exe, const std::wstring& arguments = L"", const std::wstring& workingDir = L"");
// Starts elevated ("runas"); returns the process handle as pid (0 when refused).
uint32_t StartElevated(const std::wstring& exe, const std::wstring& arguments, bool wait = false, unsigned long* exitCode = nullptr);
// Runs to completion, capturing stdout+stderr.
bool RunAndCapture(const std::wstring& exe, const std::wstring& arguments, std::string& output, unsigned long* exitCode = nullptr,
                   unsigned timeoutMs = 0xFFFFFFFF);

// A child process with redirected stdin/stdout (SteamCMD). Output lines arrive on a worker thread.
class Child {
public:
    ~Child();
    static std::unique_ptr<Child> Launch(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingDir,
                                         std::function<void(const std::string& chunk)> onOutput);
    bool Write(const std::string& text);   // to stdin
    void CloseInput();
    bool Wait(unsigned timeoutMs = 0xFFFFFFFF);
    bool Running() const;
    unsigned long ExitCode() const;
    void Kill();
    uint32_t Pid() const { return pid; }

private:
    void* process = nullptr;
    void* stdinWrite = nullptr;
    void* stdoutRead = nullptr;
    void* reader = nullptr;   // thread handle
    uint32_t pid = 0;
    std::function<void(const std::string&)> callback;
    static unsigned long __stdcall ReaderThread(void* self);
};

std::wstring QuoteArgument(const std::wstring& arg);

} // namespace SonicRacingSaveManager::Process

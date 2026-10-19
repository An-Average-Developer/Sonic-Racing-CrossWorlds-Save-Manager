#include "Core/Process.h"
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include "Core/Text.h"

namespace SonicRacingSaveManager::Process {

std::vector<Info> List() {
    std::vector<Info> out;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32W entry{ sizeof(entry) };
    if (Process32FirstW(snapshot, &entry)) {
        do {
            out.push_back({ entry.th32ProcessID, entry.szExeFile });
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return out;
}

std::vector<uint32_t> FindByName(const std::wstring& nameWithoutExe) { return FindByName(List(), nameWithoutExe); }

std::vector<uint32_t> FindByName(const std::vector<Info>& processes, const std::wstring& nameWithoutExe) {
    std::vector<uint32_t> out;
    std::wstring wanted = nameWithoutExe;
    if (Text::EndsWith(wanted, L".exe", true)) wanted.resize(wanted.size() - 4);
    for (const auto& p : processes) {
        std::wstring name = p.name;
        if (Text::EndsWith(name, L".exe", true)) name.resize(name.size() - 4);
        if (Text::Equals(name, wanted, true)) out.push_back(p.pid);
    }
    return out;
}

std::wstring ImagePath(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t buffer[MAX_PATH * 4];
    DWORD size = static_cast<DWORD>(std::size(buffer));
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buffer, &size)) out.assign(buffer, size);
    CloseHandle(h);
    return out;
}

bool IsAlive(uint32_t pid) {
    if (!pid) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
}

bool Kill(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    const bool ok = TerminateProcess(h, 1) != 0;
    CloseHandle(h);
    return ok;
}

bool SetPriority(uint32_t pid, unsigned long priorityClass) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return false;
    const bool ok = SetPriorityClass(h, priorityClass) != 0;
    CloseHandle(h);
    return ok;
}

uint32_t ForegroundPid() {
    HWND fg = GetForegroundWindow();
    if (!fg) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid;
}

uint32_t CurrentPid() { return GetCurrentProcessId(); }

bool Open(const std::wstring& target, const std::wstring& arguments, const std::wstring& workingDir) {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", target.c_str(),
                                                                arguments.empty() ? nullptr : arguments.c_str(),
                                                                workingDir.empty() ? nullptr : workingDir.c_str(), SW_SHOWNORMAL));
    return result > 32;
}

bool OpenUrl(const std::wstring& url) { return Open(url); }

bool ShowInExplorer(const std::wstring& path) { return Open(L"explorer.exe", L"/select,\"" + path + L"\""); }

std::wstring QuoteArgument(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++backslashes; continue; }
        if (c == L'"') out.append(backslashes * 2 + 1, L'\\');
        else out.append(backslashes, L'\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    out += L'"';
    return out;
}

uint32_t Start(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingDir) {
    std::wstring command = QuoteArgument(exe);
    if (!arguments.empty()) command += L" " + arguments;
    STARTUPINFOW si{ sizeof(si) };
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        workingDir.empty() ? nullptr : workingDir.c_str(), &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return pi.dwProcessId;
}

uint32_t StartElevated(const std::wstring& exe, const std::wstring& arguments, bool wait, unsigned long* exitCode) {
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = arguments.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) return 0;
    const uint32_t pid = GetProcessId(info.hProcess);
    if (wait) {
        WaitForSingleObject(info.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(info.hProcess, &code);
        if (exitCode) *exitCode = code;
    }
    CloseHandle(info.hProcess);
    return pid;
}

bool RunAndCapture(const std::wstring& exe, const std::wstring& arguments, std::string& output, unsigned long* exitCode,
                   unsigned timeoutMs) {
    auto child = Child::Launch(exe, arguments, L"", [&output](const std::string& chunk) { output += chunk; });
    if (!child) return false;
    child->CloseInput();
    const bool finished = child->Wait(timeoutMs);
    if (!finished) child->Kill();
    if (exitCode) *exitCode = child->ExitCode();
    return finished;
}

// ── Child ───────────────────────────────────────────────────────────────────

Child::~Child() {
    if (stdinWrite) CloseHandle(stdinWrite);
    if (reader) {
        WaitForSingleObject(reader, 2000);
        CloseHandle(reader);
    }
    if (stdoutRead) CloseHandle(stdoutRead);
    if (process) CloseHandle(process);
}

std::unique_ptr<Child> Child::Launch(const std::wstring& exe, const std::wstring& arguments, const std::wstring& workingDir,
                                     std::function<void(const std::string&)> onOutput) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE outRead = nullptr, outWrite = nullptr, inRead = nullptr, inWrite = nullptr;
    if (!CreatePipe(&outRead, &outWrite, &sa, 0)) return nullptr;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0)) {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return nullptr;
    }
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(inWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;
    si.hStdInput = inRead;
    PROCESS_INFORMATION pi{};
    std::wstring command = QuoteArgument(exe);
    if (!arguments.empty()) command += L" " + arguments;
    const BOOL ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                   workingDir.empty() ? nullptr : workingDir.c_str(), &si, &pi);
    CloseHandle(outWrite);
    CloseHandle(inRead);
    if (!ok) {
        CloseHandle(outRead);
        CloseHandle(inWrite);
        return nullptr;
    }
    CloseHandle(pi.hThread);

    auto child = std::unique_ptr<Child>(new Child());
    child->process = pi.hProcess;
    child->pid = pi.dwProcessId;
    child->stdoutRead = outRead;
    child->stdinWrite = inWrite;
    child->callback = std::move(onOutput);
    child->reader = CreateThread(nullptr, 0, &Child::ReaderThread, child.get(), 0, nullptr);
    return child;
}

unsigned long __stdcall Child::ReaderThread(void* self) {
    auto* child = static_cast<Child*>(self);
    char buffer[4096];
    while (true) {
        DWORD read = 0;
        if (!ReadFile(child->stdoutRead, buffer, sizeof(buffer), &read, nullptr) || read == 0) break;
        if (child->callback) child->callback(std::string(buffer, read));
    }
    return 0;
}

bool Child::Write(const std::string& text) {
    if (!stdinWrite) return false;
    DWORD written = 0;
    return WriteFile(stdinWrite, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
}

void Child::CloseInput() {
    if (stdinWrite) CloseHandle(stdinWrite);
    stdinWrite = nullptr;
}

bool Child::Wait(unsigned timeoutMs) {
    const bool done = WaitForSingleObject(process, timeoutMs) == WAIT_OBJECT_0;
    if (done && reader) WaitForSingleObject(reader, 5000);
    return done;
}

bool Child::Running() const { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; }

unsigned long Child::ExitCode() const {
    DWORD code = 0;
    if (process) GetExitCodeProcess(process, &code);
    return code;
}

void Child::Kill() {
    if (process) TerminateProcess(process, 1);
}

} // namespace SonicRacingSaveManager::Process

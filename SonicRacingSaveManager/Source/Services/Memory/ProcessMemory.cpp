#include "Services/Memory/ProcessMemory.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstring>
#include "Core/Files.h"
#include "Core/Process.h"

namespace SonicRacingSaveManager::Memory {

namespace {

// Process.MainModule.BaseAddress: the first module of the snapshot is the exe itself
uintptr_t MainModuleBase(uint32_t pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry{ sizeof(entry) };
    uintptr_t base = 0;
    if (Module32FirstW(snap, &entry)) base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
    CloseHandle(snap);
    return base;
}

// TryGetWorkingSet: Process.WorkingSet64, 0 when it can't be read
long long WorkingSet(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    PROCESS_MEMORY_COUNTERS counters{ sizeof(counters) };
    const long long size = K32GetProcessMemoryInfo(h, &counters, sizeof(counters)) ? static_cast<long long>(counters.WorkingSetSize) : 0;
    CloseHandle(h);
    return size;
}

} // namespace

bool ProcessMemory::Attach(uint32_t pid, const std::wstring& name) {
    if (processHandle) CloseHandle(processHandle);
    processHandle = OpenProcess(PROCESS_ALL_ACCESS_, FALSE, pid);
    if (!processHandle) return false;
    moduleBase = MainModuleBase(pid);
    if (moduleBase == 0) {
        CloseHandle(processHandle);
        processHandle = nullptr;
        return false;
    }
    processName = name;
    attached = true;
    return true;
}

bool ProcessMemory::AttachToProcess() {
    const auto processes = Process::FindByName(Config::Game::ProcessName);
    if (processes.empty()) return false;
    // OrderByDescending(TryGetWorkingSet).First(): the first of the heaviest ones
    uint32_t best = processes.front();
    long long bestSize = WorkingSet(best);
    for (size_t i = 1; i < processes.size(); ++i) {
        const long long size = WorkingSet(processes[i]);
        if (size > bestSize) {
            best = processes[i];
            bestSize = size;
        }
    }
    return Attach(best, Config::Game::ProcessName);
}

bool ProcessMemory::AttachToProcess(uint32_t processId) {
    // Process.GetProcessById throws for a pid that's gone (caught: false)
    if (!Process::IsAlive(processId)) {
        attached = false;
        return false;
    }
    const std::wstring name = Files::FileNameWithoutExtension(Process::ImagePath(processId));
    return Attach(processId, name.empty() ? std::wstring(Config::Game::ProcessName) : name);
}

std::vector<CandidateProcess> ProcessMemory::GetCandidateProcesses() {
    std::vector<CandidateProcess> out;
    for (const auto& p : Process::List()) {
        std::wstring name = p.name;
        if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0) name.resize(name.size() - 4);
        if (_wcsicmp(name.c_str(), Config::Game::ProcessName) == 0) out.push_back({ p.pid, name });
    }
    return out;
}

void ProcessMemory::DetachFromProcess() {
    if (processHandle) {
        CloseHandle(processHandle);
        processHandle = nullptr;
    }
    attached = false;
}

void ProcessMemory::ThrowIfNotAttached() const {
    if (!attached || !processHandle) throw Error(L"Not attached to process");
}

int32_t ProcessMemory::ReadInt32(int64_t address) const {
    ThrowIfNotAttached();
    int32_t value = 0;
    SIZE_T read = 0;
    return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), &value, 4, &read) && read == 4 ? value : 0;
}

bool ProcessMemory::WriteInt32(int64_t address, int32_t value) const {
    ThrowIfNotAttached();
    return WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(address), &value, 4, nullptr) != FALSE;
}

std::vector<int64_t> ProcessMemory::ScanForInt32(int32_t value) const {
    ThrowIfNotAttached();
    std::vector<int64_t> matches;
    std::vector<uint8_t> buffer(ScanChunkSize);
    int64_t address = 0;
    int64_t lastAddress = 0;
    MEMORY_BASIC_INFORMATION mbi{};
    while (VirtualQueryEx(processHandle, reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) != 0) {
        const int64_t regionBase = reinterpret_cast<int64_t>(mbi.BaseAddress);
        const int64_t regionSize = static_cast<int64_t>(mbi.RegionSize);
        if (regionSize <= 0) break;

        const bool writable = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && (mbi.Protect & PAGE_GUARD) == 0 &&
                              (mbi.Protect & PAGE_NOACCESS) == 0 &&
                              (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
        if (writable) {
            for (int64_t offset = 0; offset < regionSize; offset += ScanChunkSize) {
                const SIZE_T toRead = static_cast<SIZE_T>(std::min<int64_t>(ScanChunkSize, regionSize - offset));
                SIZE_T bytesRead = 0;
                if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(regionBase + offset), buffer.data(), toRead, &bytesRead)) continue;
                for (SIZE_T i = 0; i + 4 <= bytesRead; i += 4) {
                    int32_t v;
                    std::memcpy(&v, buffer.data() + i, 4);
                    if (v == value) matches.push_back(regionBase + offset + static_cast<int64_t>(i));
                }
            }
        }

        const int64_t next = regionBase + regionSize;
        if (next <= lastAddress) break;
        lastAddress = next;
        address = next;
    }
    return matches;
}

std::vector<int64_t> ProcessMemory::FilterInt32(const std::vector<int64_t>& candidates, int32_t value) const {
    ThrowIfNotAttached();
    std::vector<int64_t> matches;
    for (int64_t candidate : candidates) {
        int32_t v = 0;
        SIZE_T read = 0;
        if (ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(candidate), &v, 4, &read) && read == 4 && v == value) matches.push_back(candidate);
    }
    return matches;
}

bool ProcessMemory::IsProcessRunning() const { return !Process::FindByName(processName).empty(); }

bool ProcessMemory::WriteBytes(int64_t address, const Bytes& bytes) const {
    ThrowIfNotAttached();
    LPVOID target = reinterpret_cast<LPVOID>(static_cast<int64_t>(moduleBase) + address);
    // Change memory protection to writable
    DWORD oldProtect = 0;
    if (!VirtualProtectEx(processHandle, target, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    // Write the bytes
    const bool success = WriteProcessMemory(processHandle, target, bytes.data(), bytes.size(), nullptr) != FALSE;
    // Restore original protection
    DWORD ignored = 0;
    VirtualProtectEx(processHandle, target, bytes.size(), oldProtect, &ignored);
    return success;
}

} // namespace SonicRacingSaveManager::Memory

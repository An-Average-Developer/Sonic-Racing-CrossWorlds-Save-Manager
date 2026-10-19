#pragma once
#include <windows.h>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>
#include "App/Config.h"

// Features\MemoryEditor\Services\MemoryEditorService.cs: attaching to the game, int32 reads / writes,
// the Cheat Engine style ticket search and the module-relative byte patches. Same Win32 calls,
// constants, chunk size and order as the C# (kept byte for byte: see TicketPatches.h).
namespace SonicRacingSaveManager::Memory {

using Bytes = std::vector<uint8_t>;

inline constexpr DWORD PROCESS_ALL_ACCESS_ = 0x1F0FFF;   // the C# value (not the SDK's newer 0x1FFFFF)
inline constexpr size_t ScanChunkSize = 16 * 1024 * 1024;

// throw new InvalidOperationException("Not attached to process"): the view model showed ex.Message
class Error : public std::exception {
public:
    explicit Error(std::wstring message) : text(std::move(message)) {}
    const std::wstring& Message() const { return text; }
    const char* what() const noexcept override { return "SonicRacingSaveManager::Memory::Error"; }

private:
    std::wstring text;
};

// GetCandidateProcesses(): one running SonicRacingCrossWorldsSteam process
struct CandidateProcess {
    uint32_t pid = 0;
    std::wstring processName;   // Process.ProcessName (without ".exe")
};

class ProcessMemory {
public:
    ~ProcessMemory() { DetachFromProcess(); }

    bool IsAttached() const { return attached; }
    const std::wstring& ProcessName() const { return processName; }

    // Automatic attach by the default process name. With several processes of that name (a launcher
    // stub next to the real game) the one using the most memory (working set) is picked.
    bool AttachToProcess();
    // Manual attach to a process picked from GetCandidateProcesses
    bool AttachToProcess(uint32_t processId);
    static std::vector<CandidateProcess> GetCandidateProcesses();
    void DetachFromProcess();

    // Throw Error when not attached; a failed read gives 0, as in the C#
    int32_t ReadInt32(int64_t address) const;
    bool WriteInt32(int64_t address, int32_t value) const;
    // Cheat Engine style "first scan": every 4-byte aligned int32 equal to `value` in the game's
    // private read/write memory (the heap, where the tickets live), read in 16 MB chunks
    std::vector<int64_t> ScanForInt32(int32_t value) const;
    // "Next scan": keeps only the candidates that now hold `value`
    std::vector<int64_t> FilterInt32(const std::vector<int64_t>& candidates, int32_t value) const;

    bool IsProcessRunning() const;
    // Patch at module base + `address`: VirtualProtectEx(RWX) → WriteProcessMemory → restore the old
    // protection. False when the protection can't be changed or the write fails.
    bool WriteBytes(int64_t address, const Bytes& bytes) const;

private:
    bool Attach(uint32_t pid, const std::wstring& name);
    void ThrowIfNotAttached() const;

    HANDLE processHandle = nullptr;
    bool attached = false;
    std::wstring processName = Config::Game::ProcessName;
    uintptr_t moduleBase = 0;
};

} // namespace SonicRacingSaveManager::Memory

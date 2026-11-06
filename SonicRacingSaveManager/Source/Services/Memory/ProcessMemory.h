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
    bool AttachToProcess(uint32_t pid);
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

    // Writes code at an absolute address (VirtualProtectEx RWX around the write, like WriteBytes)
    bool WriteCode(int64_t address, const Bytes& bytes) const;
    // Allocates `size` bytes of RWX memory within 2 GB below the main module (for code caves that a
    // rel32 jmp can reach), 0 when nothing could be allocated
    int64_t AllocateNearModule(size_t size) const;
    // Raw reads / writes at absolute addresses (false when they fail); throw Error when not attached
    bool Read(int64_t address, void* out, size_t size) const;
    bool Write(int64_t address, const void* data, size_t size) const;
    // Cheat Engine style AOB scan of the main module's code: "48 8B 3D ?? ?? ?? ?? 8B F2", "??" = any
    // byte. The absolute address of the first match, 0 when there is none.
    int64_t FindPattern(const char* pattern) const;
    // The address of the first 8-byte aligned qword equal to `value` in the main module's data
    // (a vtable slot pointing to a function), 0 when there is none
    int64_t FindQwordInModule(uint64_t value) const;
    int64_t ModuleBase() const { return static_cast<int64_t>(moduleBase); }
    uint32_t ProcessId() const { return processId; }
    // Every 8-byte aligned qword equal to `value` in the private read/write memory of process `pid`,
    // through a handle of its own (safe on a worker thread while the UI detaches)
    static std::vector<int64_t> ScanForPointer(uint32_t pid, uint64_t value);

private:
    bool Attach(uint32_t pid, const std::wstring& name);
    void ThrowIfNotAttached() const;

    HANDLE processHandle = nullptr;
    bool attached = false;
    std::wstring processName = Config::Game::ProcessName;
    uintptr_t moduleBase = 0;
    uint32_t processId = 0;
};

} // namespace SonicRacingSaveManager::Memory

#pragma once
#include <optional>
#include <string>
#include <vector>

// Registry, clipboard, file pickers, DPAPI and other small Windows services.
namespace SonicRacingSaveManager::System {

enum class Hive { CurrentUser, LocalMachine };
std::optional<std::wstring> RegistryString(Hive hive, const std::wstring& key, const std::wstring& value, bool wow64_32 = false);
std::optional<unsigned long> RegistryDword(Hive hive, const std::wstring& key, const std::wstring& value);
std::vector<std::wstring> RegistrySubKeys(Hive hive, const std::wstring& key);

bool SetClipboardText(const std::wstring& text);
std::wstring GetClipboardText();

struct FileFilter {
    std::wstring name;     // "Images"
    std::wstring pattern;  // "*.png;*.jpg"
};
// Modern IFileOpenDialog pickers; empty result when cancelled.
std::wstring PickFile(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters,
                      const std::wstring& initialDir = L"");
std::vector<std::wstring> PickFiles(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters);
std::wstring PickFolder(void* owner, const std::wstring& title, const std::wstring& initialDir = L"");
// SaveFileDialog: `fileName` pre-filled, `defaultExtension` ("zip", no dot) added when the user types none
std::wstring PickSaveFile(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters, const std::wstring& fileName,
                          const std::wstring& defaultExtension);

// DPAPI (ProtectedData, CurrentUser scope)
std::optional<std::string> Protect(const std::string& data);
std::optional<std::string> Unprotect(const std::string& data);

std::string Base64Encode(const std::string& data);
std::optional<std::string> Base64Decode(const std::string& text);
std::string Sha256Hex(const std::string& data);
std::string RandomHex(size_t bytes);
// Guid.NewGuid().ToString(): "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
std::wstring NewGuid();

bool IsElevated();
std::wstring MachineName();
std::wstring UserName();
// Stable anonymous id for this PC (hash of MachineGuid), used by the heartbeat / online counter.
std::string MachineId();

// Short beep-free system sound helpers live in Services/Audio; this only checks the OS build.
unsigned long WindowsBuild();

} // namespace SonicRacingSaveManager::System

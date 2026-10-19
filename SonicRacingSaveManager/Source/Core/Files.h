#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// File system helpers. None of them throw: failures come back as false / empty values,
// so a failed file access never stops the launcher.
namespace SonicRacingSaveManager::Files {

bool Exists(const std::wstring& path);           // file
bool DirectoryExists(const std::wstring& path);
bool CreateDirectories(const std::wstring& path);
bool Delete(const std::wstring& path);           // file
bool DeleteDirectory(const std::wstring& path);  // recursive
bool Copy(const std::wstring& from, const std::wstring& to, bool overwrite = true);
bool Move(const std::wstring& from, const std::wstring& to, bool overwrite = true);
// Moves a whole directory; falls back to copy + delete across volumes.
bool MoveDirectory(const std::wstring& from, const std::wstring& to);
bool CopyDirectory(const std::wstring& from, const std::wstring& to, bool overwrite = true);
std::optional<uint64_t> Size(const std::wstring& path);
std::optional<int64_t> LastWriteTime(const std::wstring& path);  // FILETIME ticks

std::optional<std::string> ReadBytes(const std::wstring& path);
std::optional<std::wstring> ReadText(const std::wstring& path);  // UTF-8 (BOM tolerated)
std::vector<std::wstring> ReadLines(const std::wstring& path);
bool WriteBytes(const std::wstring& path, const void* data, size_t size);
bool WriteBytes(const std::wstring& path, const std::string& data);
bool WriteText(const std::wstring& path, const std::wstring& text);  // UTF-8 without BOM, creates the folder
bool AppendText(const std::wstring& path, const std::wstring& text);

// Lists full paths. `pattern` is a wildcard like L"*.pak".
std::vector<std::wstring> ListFiles(const std::wstring& dir, const std::wstring& pattern = L"*", bool recursive = false);
std::vector<std::wstring> ListDirectories(const std::wstring& dir);
uint64_t DirectorySize(const std::wstring& dir);

std::wstring Combine(const std::wstring& a, const std::wstring& b);
template <typename... T>
std::wstring Combine(const std::wstring& a, const std::wstring& b, const T&... rest) {
    return Combine(Combine(a, b), rest...);
}
std::wstring DirectoryName(const std::wstring& path);
std::wstring FileName(const std::wstring& path);
std::wstring FileNameWithoutExtension(const std::wstring& path);
std::wstring Extension(const std::wstring& path);  // ".png" (with dot, original case)
std::wstring FullPath(const std::wstring& path);
std::wstring RelativePath(const std::wstring& base, const std::wstring& path);

std::wstring TempDirectory();        // %TEMP%
std::wstring LocalAppData();         // %LOCALAPPDATA%
std::wstring RoamingAppData();       // %APPDATA%
std::wstring ProgramFilesX86();
std::wstring Desktop();
std::wstring ExecutablePath();
std::wstring UniqueTempFile(const std::wstring& extension);

} // namespace SonicRacingSaveManager::Files

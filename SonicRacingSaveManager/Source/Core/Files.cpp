#include "Core/Files.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <filesystem>
#include <fstream>
#include "Core/Text.h"

namespace fs = std::filesystem;

namespace SonicRacingSaveManager::Files {

bool Exists(const std::wstring& path) {
    if (path.empty()) return false;
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryExists(const std::wstring& path) {
    if (path.empty()) return false;
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool CreateDirectories(const std::wstring& path) {
    if (path.empty()) return false;
    std::error_code ec;
    fs::create_directories(path, ec);
    return DirectoryExists(path);
}

bool Delete(const std::wstring& path) {
    if (!Exists(path)) return true;
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    return DeleteFileW(path.c_str()) != 0;
}

bool DeleteDirectory(const std::wstring& path) {
    if (!DirectoryExists(path)) return true;
    std::error_code ec;
    // Read-only files would make remove_all fail half way
    for (auto it = fs::recursive_directory_iterator(path, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        SetFileAttributesW(it->path().c_str(), FILE_ATTRIBUTE_NORMAL);
    ec.clear();
    fs::remove_all(path, ec);
    return !ec && !DirectoryExists(path);
}

bool Copy(const std::wstring& from, const std::wstring& to, bool overwrite) {
    CreateDirectories(DirectoryName(to));
    if (overwrite && Exists(to)) SetFileAttributesW(to.c_str(), FILE_ATTRIBUTE_NORMAL);
    return CopyFileW(from.c_str(), to.c_str(), overwrite ? FALSE : TRUE) != 0;
}

bool Move(const std::wstring& from, const std::wstring& to, bool overwrite) {
    CreateDirectories(DirectoryName(to));
    DWORD flags = MOVEFILE_COPY_ALLOWED;
    if (overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
    return MoveFileExW(from.c_str(), to.c_str(), flags) != 0;
}

bool CopyDirectory(const std::wstring& from, const std::wstring& to, bool overwrite) {
    std::error_code ec;
    fs::create_directories(to, ec);
    auto options = fs::copy_options::recursive;
    options |= overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::skip_existing;
    fs::copy(from, to, options, ec);
    return !ec;
}

bool MoveDirectory(const std::wstring& from, const std::wstring& to) {
    CreateDirectories(DirectoryName(to));
    if (MoveFileExW(from.c_str(), to.c_str(), 0)) return true;
    // Different volume (or target exists): merge by copying, then remove the source
    if (!CopyDirectory(from, to, true)) return false;
    DeleteDirectory(from);
    return true;
}

std::optional<uint64_t> Size(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return std::nullopt;
    return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

std::optional<int64_t> LastWriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return std::nullopt;
    return (static_cast<int64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

std::optional<std::string> ReadBytes(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    LARGE_INTEGER size{};
    GetFileSizeEx(file, &size);
    std::string data(static_cast<size_t>(size.QuadPart), '\0');
    size_t done = 0;
    while (done < data.size()) {
        DWORD read = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - done, 1 << 24));
        if (!ReadFile(file, data.data() + done, chunk, &read, nullptr) || read == 0) break;
        done += read;
    }
    CloseHandle(file);
    data.resize(done);
    return data;
}

std::optional<std::wstring> ReadText(const std::wstring& path) {
    auto bytes = ReadBytes(path);
    if (!bytes) return std::nullopt;
    std::string_view view(*bytes);
    if (view.size() >= 3 && static_cast<unsigned char>(view[0]) == 0xEF && static_cast<unsigned char>(view[1]) == 0xBB &&
        static_cast<unsigned char>(view[2]) == 0xBF)
        view.remove_prefix(3);
    if (view.size() >= 2 && static_cast<unsigned char>(view[0]) == 0xFF && static_cast<unsigned char>(view[1]) == 0xFE) {
        // UTF-16 LE (some game config files)
        return std::wstring(reinterpret_cast<const wchar_t*>(view.data() + 2), (view.size() - 2) / 2);
    }
    return Text::Widen(view);
}

std::vector<std::wstring> ReadLines(const std::wstring& path) {
    auto text = ReadText(path);
    if (!text) return {};
    return Text::SplitLines(*text);
}

bool WriteBytes(const std::wstring& path, const void* data, size_t size) {
    CreateDirectories(DirectoryName(path));
    if (Exists(path)) SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const auto* bytes = static_cast<const char*>(data);
    size_t done = 0;
    bool ok = true;
    while (done < size) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(size - done, 1 << 24));
        if (!WriteFile(file, bytes + done, chunk, &written, nullptr) || written == 0) { ok = false; break; }
        done += written;
    }
    CloseHandle(file);
    return ok;
}

bool WriteBytes(const std::wstring& path, const std::string& data) { return WriteBytes(path, data.data(), data.size()); }

bool WriteText(const std::wstring& path, const std::wstring& text) {
    const std::string utf8 = Text::Narrow(text);
    return WriteBytes(path, utf8.data(), utf8.size());
}

bool AppendText(const std::wstring& path, const std::wstring& text) {
    CreateDirectories(DirectoryName(path));
    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    const std::string utf8 = Text::Narrow(text);
    DWORD written = 0;
    const bool ok = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) != 0;
    CloseHandle(file);
    return ok;
}

std::vector<std::wstring> ListFiles(const std::wstring& dir, const std::wstring& pattern, bool recursive) {
    std::vector<std::wstring> out;
    if (!DirectoryExists(dir)) return out;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(Combine(dir, pattern).c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(Combine(dir, data.cFileName));
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    if (recursive) {
        for (const auto& sub : ListDirectories(dir)) {
            auto nested = ListFiles(sub, pattern, true);
            out.insert(out.end(), nested.begin(), nested.end());
        }
    }
    return out;
}

std::vector<std::wstring> ListDirectories(const std::wstring& dir) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW(Combine(dir, L"*").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return out;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(data.cFileName, L".") != 0 &&
            wcscmp(data.cFileName, L"..") != 0)
            out.push_back(Combine(dir, data.cFileName));
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return out;
}

uint64_t DirectorySize(const std::wstring& dir) {
    uint64_t total = 0;
    for (const auto& f : ListFiles(dir, L"*", true)) total += Size(f).value_or(0);
    return total;
}

std::wstring Combine(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    // An absolute second part wins, like Path.Combine
    if (b.size() >= 2 && (b[1] == L':' || (b[0] == L'\\' && b[1] == L'\\'))) return b;
    std::wstring out = a;
    if (out.back() != L'\\' && out.back() != L'/') out += L'\\';
    size_t start = 0;
    while (start < b.size() && (b[start] == L'\\' || b[start] == L'/')) ++start;
    out += b.substr(start);
    for (auto& c : out) if (c == L'/') c = L'\\';
    return out;
}

std::wstring DirectoryName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    if (slash == 2 && path.size() > 1 && path[1] == L':') return path.substr(0, 3);
    return path.substr(0, slash);
}

std::wstring FileName(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring FileNameWithoutExtension(const std::wstring& path) {
    std::wstring name = FileName(path);
    const size_t dot = name.find_last_of(L'.');
    return dot == std::wstring::npos ? name : name.substr(0, dot);
}

std::wstring Extension(const std::wstring& path) {
    const std::wstring name = FileName(path);
    const size_t dot = name.find_last_of(L'.');
    return dot == std::wstring::npos ? std::wstring() : name.substr(dot);
}

std::wstring FullPath(const std::wstring& path) {
    wchar_t buffer[MAX_PATH * 4];
    const DWORD n = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(buffer)), buffer, nullptr);
    return n ? std::wstring(buffer, n) : path;
}

std::wstring RelativePath(const std::wstring& base, const std::wstring& path) {
    std::error_code ec;
    auto rel = fs::relative(path, base, ec);
    return ec ? path : rel.wstring();
}

namespace {

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR raw = nullptr;
    std::wstring out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

} // namespace

std::wstring TempDirectory() {
    wchar_t buffer[MAX_PATH + 1];
    const DWORD n = GetTempPathW(MAX_PATH + 1, buffer);
    std::wstring out(buffer, n);
    while (!out.empty() && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
    return out;
}

std::wstring LocalAppData() { return KnownFolder(FOLDERID_LocalAppData); }
std::wstring RoamingAppData() { return KnownFolder(FOLDERID_RoamingAppData); }
std::wstring ProgramFilesX86() { return KnownFolder(FOLDERID_ProgramFilesX86); }
std::wstring Desktop() { return KnownFolder(FOLDERID_Desktop); }

std::wstring ExecutablePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n < buffer.size()) {
            buffer.resize(n);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring UniqueTempFile(const std::wstring& extension) {
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t name[64];
    swprintf_s(name, L"%08lX%04X%04X%s", guid.Data1, guid.Data2, guid.Data3, extension.c_str());
    return Combine(TempDirectory(), name);
}

} // namespace SonicRacingSaveManager::Files

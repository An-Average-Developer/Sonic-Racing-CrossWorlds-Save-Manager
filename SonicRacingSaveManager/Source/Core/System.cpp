#include "Core/System.h"
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <shobjidl.h>
#include <wincrypt.h>
#include <wrl/client.h>
#include "Core/Text.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

using Microsoft::WRL::ComPtr;

namespace SonicRacingSaveManager::System {

namespace {

HKEY Root(Hive hive) { return hive == Hive::CurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE; }

} // namespace

std::optional<std::wstring> RegistryString(Hive hive, const std::wstring& key, const std::wstring& value, bool wow64_32) {
    DWORD size = 0;
    DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ;
    if (wow64_32) flags |= RRF_SUBKEY_WOW6432KEY;
    if (RegGetValueW(Root(hive), key.c_str(), value.empty() ? nullptr : value.c_str(), flags, nullptr, nullptr, &size) !=
            ERROR_SUCCESS || size == 0)
        return std::nullopt;
    std::wstring out(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(Root(hive), key.c_str(), value.empty() ? nullptr : value.c_str(), flags, nullptr, out.data(), &size) !=
        ERROR_SUCCESS)
        return std::nullopt;
    out.resize(wcslen(out.c_str()));
    return out;
}

std::optional<unsigned long> RegistryDword(Hive hive, const std::wstring& key, const std::wstring& value) {
    DWORD data = 0, size = sizeof(data);
    if (RegGetValueW(Root(hive), key.c_str(), value.c_str(), RRF_RT_REG_DWORD, nullptr, &data, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return data;
}

std::vector<std::wstring> RegistrySubKeys(Hive hive, const std::wstring& key) {
    std::vector<std::wstring> out;
    HKEY h = nullptr;
    if (RegOpenKeyExW(Root(hive), key.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) return out;
    wchar_t name[512];
    for (DWORD i = 0;; ++i) {
        DWORD len = static_cast<DWORD>(std::size(name));
        if (RegEnumKeyExW(h, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        out.emplace_back(name, len);
    }
    RegCloseKey(h);
    return out;
}

bool SetClipboardText(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    bool ok = false;
    if (mem) {
        memcpy(GlobalLock(mem), text.c_str(), bytes);
        GlobalUnlock(mem);
        ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
        if (!ok) GlobalFree(mem);
    }
    CloseClipboard();
    return ok;
}

std::wstring GetClipboardText() {
    std::wstring out;
    if (!OpenClipboard(nullptr)) return out;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* text = static_cast<const wchar_t*>(GlobalLock(data))) {
            out = text;
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return out;
}

namespace {

std::vector<COMDLG_FILTERSPEC> Specs(const std::vector<FileFilter>& filters) {
    std::vector<COMDLG_FILTERSPEC> specs;
    for (const auto& f : filters) specs.push_back({ f.name.c_str(), f.pattern.c_str() });
    return specs;
}

std::wstring ItemPath(IShellItem* item) {
    PWSTR raw = nullptr;
    std::wstring out;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw))) out = raw;
    CoTaskMemFree(raw);
    return out;
}

void SetFolder(IFileDialog* dialog, const std::wstring& dir) {
    if (dir.empty()) return;
    ComPtr<IShellItem> folder;
    if (SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) dialog->SetFolder(folder.Get());
}

} // namespace

std::wstring PickFile(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters, const std::wstring& initialDir) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    const auto specs = Specs(filters);
    if (!specs.empty()) dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    if (!title.empty()) dialog->SetTitle(title.c_str());
    SetFolder(dialog.Get(), initialDir);
    if (FAILED(dialog->Show(static_cast<HWND>(owner)))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return {};
    return ItemPath(item.Get());
}

std::vector<std::wstring> PickFiles(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters) {
    std::vector<std::wstring> out;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return out;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_ALLOWMULTISELECT);
    const auto specs = Specs(filters);
    if (!specs.empty()) dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    if (!title.empty()) dialog->SetTitle(title.c_str());
    if (FAILED(dialog->Show(static_cast<HWND>(owner)))) return out;
    ComPtr<IShellItemArray> items;
    if (FAILED(dialog->GetResults(&items))) return out;
    DWORD count = 0;
    items->GetCount(&count);
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        if (SUCCEEDED(items->GetItemAt(i, &item))) out.push_back(ItemPath(item.Get()));
    }
    return out;
}

std::wstring PickFolder(void* owner, const std::wstring& title, const std::wstring& initialDir) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS);
    if (!title.empty()) dialog->SetTitle(title.c_str());
    SetFolder(dialog.Get(), initialDir);
    if (FAILED(dialog->Show(static_cast<HWND>(owner)))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return {};
    return ItemPath(item.Get());
}

std::wstring PickSaveFile(void* owner, const std::wstring& title, const std::vector<FileFilter>& filters, const std::wstring& fileName,
                          const std::wstring& defaultExtension) {
    ComPtr<IFileSaveDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    const auto specs = Specs(filters);
    if (!specs.empty()) dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    if (!title.empty()) dialog->SetTitle(title.c_str());
    if (!fileName.empty()) dialog->SetFileName(fileName.c_str());
    if (!defaultExtension.empty()) dialog->SetDefaultExtension(defaultExtension.c_str());
    if (FAILED(dialog->Show(static_cast<HWND>(owner)))) return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return {};
    return ItemPath(item.Get());
}

std::optional<std::string> Protect(const std::string& data) {
    DATA_BLOB in{ static_cast<DWORD>(data.size()), reinterpret_cast<BYTE*>(const_cast<char*>(data.data())) };
    DATA_BLOB out{};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return std::nullopt;
    std::string result(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::optional<std::string> Unprotect(const std::string& data) {
    DATA_BLOB in{ static_cast<DWORD>(data.size()), reinterpret_cast<BYTE*>(const_cast<char*>(data.data())) };
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return std::nullopt;
    std::string result(reinterpret_cast<char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::string Base64Encode(const std::string& data) {
    if (data.empty()) return {};
    DWORD size = 0;
    const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    CryptBinaryToStringA(reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()), flags, nullptr, &size);
    std::string out(size, '\0');
    CryptBinaryToStringA(reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()), flags, out.data(), &size);
    out.resize(size);
    return out;
}

std::optional<std::string> Base64Decode(const std::string& text) {
    if (text.empty()) return std::string();
    DWORD size = 0;
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr,
                              nullptr))
        return std::nullopt;
    std::string out(size, '\0');
    if (!CryptStringToBinaryA(text.c_str(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64,
                              reinterpret_cast<BYTE*>(out.data()), &size, nullptr, nullptr))
        return std::nullopt;
    out.resize(size);
    return out;
}

std::string Sha256Hex(const std::string& data) {
    BYTE hash[32]{};
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
               static_cast<ULONG>(data.size()), hash, sizeof(hash));
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (BYTE b : hash) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}

std::wstring NewGuid() {
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t text[40];
    swprintf_s(text, L"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
               guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return text;
}

std::string RandomHex(size_t bytes) {
    std::string raw(bytes, '\0');
    BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(raw.data()), static_cast<ULONG>(raw.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (unsigned char b : raw) {
        out += digits[b >> 4];
        out += digits[b & 15];
    }
    return out;
}

bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}

std::wstring MachineName() {
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = static_cast<DWORD>(std::size(buffer));
    return GetComputerNameW(buffer, &size) ? std::wstring(buffer, size) : std::wstring();
}

std::wstring UserName() {
    wchar_t buffer[256];
    DWORD size = static_cast<DWORD>(std::size(buffer));
    return GetUserNameW(buffer, &size) ? std::wstring(buffer) : std::wstring();
}

std::string MachineId() {
    auto guid = RegistryString(Hive::LocalMachine, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid");
    const std::string seed = Text::Narrow(guid.value_or(MachineName() + L"|" + UserName()));
    return Sha256Hex(seed);
}

unsigned long WindowsBuild() {
    auto build = RegistryString(Hive::LocalMachine, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber");
    return build ? static_cast<unsigned long>(Text::ToInt(*build).value_or(0)) : 0;
}

} // namespace SonicRacingSaveManager::System

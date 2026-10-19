#include "Services/App/ResourceExtractor.h"
#include <windows.h>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Generated/AssetTable.h"

namespace SonicRacingSaveManager::Services::ResourceExtractor {

namespace {

bool ResourceBytes(int id, const void*& data, DWORD& size) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return false;
    HGLOBAL handle = LoadResource(nullptr, res);
    if (!handle) return false;
    data = LockResource(handle);
    size = SizeofResource(nullptr, res);
    return data != nullptr;
}

// A stamp of the exe build + embedded content: when it matches the previous run, files whose size
// still matches are left alone instead of being rewritten on every start.
std::wstring CurrentStamp() {
    const std::wstring exe = Files::ExecutablePath();
    return std::wstring(kEmbeddedAssetsStamp) + L"|" + std::to_wstring(Files::Size(exe).value_or(0)) + L"|" +
           std::to_wstring(Files::LastWriteTime(exe).value_or(0));
}

} // namespace

void Extract(const std::function<void(size_t done, size_t total)>& progress) {
    const std::wstring temp = Paths::TempDir();
    const std::wstring stampFile = Files::Combine(temp, L".extracted");
    const std::wstring stamp = CurrentStamp();
    const bool sameBuild = Files::ReadText(stampFile).value_or(L"") == stamp;
    if (!sameBuild) Files::DeleteDirectory(temp);
    Files::CreateDirectories(temp);

    const size_t total = std::size(kEmbeddedAssets);
    size_t done = 0;
    for (const auto& asset : kEmbeddedAssets) {
        if (progress) progress(done, total);
        ++done;
        const void* data = nullptr;
        DWORD size = 0;
        if (!ResourceBytes(asset.id, data, size)) continue;
        const std::wstring dest = Files::Combine(temp, asset.root, asset.path);
        if (sameBuild && Files::Size(dest).value_or(~0ull) == size) continue;
        Files::WriteBytes(dest, data, size);
    }
    Files::WriteText(stampFile, stamp);
    if (progress) progress(total, total);
}

std::optional<std::string> ReadEmbedded(const std::wstring& root, const std::wstring& path) {
    for (const auto& asset : kEmbeddedAssets) {
        if (!Text::Equals(asset.root, root, true) || !Text::Equals(asset.path, path, true)) continue;
        const void* data = nullptr;
        DWORD size = 0;
        if (!ResourceBytes(asset.id, data, size)) return std::nullopt;
        return std::string(static_cast<const char*>(data), size);
    }
    return std::nullopt;
}

} // namespace SonicRacingSaveManager::Services::ResourceExtractor

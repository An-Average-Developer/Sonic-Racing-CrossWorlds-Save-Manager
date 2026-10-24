#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Graphics/Color.h"

namespace SonicRacingSaveManager::Models {

// Features\ModManager\Models\ModInfo.cs — one folder of the mods directory
struct ModInfo {
    std::wstring name;         // without the "01 - " priority prefix
    std::wstring folderPath;
    std::vector<std::wstring> files;   // everything but .json files
    bool isEnabled = false;            // no ".off" file and at least one active one
    long long totalSize = 0;
    int fileCount = 0;
    std::wstring version;              // info.json "version"
    std::wstring modPageUrl;           // info.json "mod_page"
    std::wstring infoName;             // info.json "name" (the Mod Store matches mods by it)
    bool hasUpdate = false;
    std::wstring latestVersion;
    int priority = -1;                 // -1 means no priority set
    // What the files change (Services::Mods::ReadModContents): the characters it replaces and the
    // other kinds of content (translation keys)
    std::vector<std::wstring> characters;
    std::vector<const char*> contentKinds;

    // "0 KB", "12.5 MB"... ({bytes:0.##} {unit})
    std::wstring FileSizeFormatted() const;
    std::wstring StatusText() const;   // "Enabled" / "Disabled" (translated)
    Color StatusColor() const { return isEnabled ? Argb(0xFF4CAF50) : Argb(0xFFFF6B00); }
    // "-", "1.0", "1.0 → 1.2"
    std::wstring VersionDisplay() const;
    std::wstring PriorityDisplay() const;   // "01" or "-"
    bool HasPriority() const { return priority >= 0; }
    // "-", "Update Available", "Up to Date" (translated)
    std::wstring UpdateStatusText() const;
    Color UpdateStatusColor() const;
    // "Blaze, Storm · Voice", "UI", "" when nothing is known (translated)
    std::wstring ContentsDisplay() const;
};

// GameBananaFile — one entry of the API's _aFiles
struct GameBananaFile {
    std::optional<std::wstring> fileName;      // _sFile
    std::optional<std::wstring> downloadUrl;   // _sDownloadUrl
    std::optional<long long> fileSize;         // _nFilesize
};

} // namespace SonicRacingSaveManager::Models

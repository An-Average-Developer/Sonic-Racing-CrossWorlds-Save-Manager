#pragma once
#include <optional>
#include <string>
#include "Core/Json.h"

// Small settings files. Every read/write swallows errors, like the try { } catch { } wrappers in
// the old code. The C# manager keeps some files in %LOCALAPPDATA% and others in %APPDATA%.
namespace SonicRacingSaveManager::Config {

enum class Folder {
    Local,     // %LOCALAPPDATA%\SonicRacingSaveManager (Discord, season theme, language)
    Roaming,   // %APPDATA%\SonicRacingSaveManager (settings.json)
};

std::wstring FilePath(const std::wstring& fileName, Folder folder = Folder::Local);

// Plain text .cfg files (trimmed contents)
std::optional<std::wstring> ReadText(const std::wstring& fileName, Folder folder = Folder::Local);
void WriteText(const std::wstring& fileName, const std::wstring& value, Folder folder = Folder::Local);

// JSON files (System.Text.Json / Newtonsoft compatible: PascalCase property names)
Json::Value ReadJson(const std::wstring& fileName, bool* exists = nullptr, Folder folder = Folder::Local);
void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented = false, Folder folder = Folder::Local);

} // namespace SonicRacingSaveManager::Config

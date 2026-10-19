#include "Settings/ConfigFile.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace SonicRacingSaveManager::Config {

std::wstring FilePath(const std::wstring& fileName, Folder folder) {
    return folder == Folder::Roaming ? Paths::RoamingFile(fileName) : Paths::DataFile(fileName);
}

std::optional<std::wstring> ReadText(const std::wstring& fileName, Folder folder) {
    auto text = Files::ReadText(FilePath(fileName, folder));
    if (!text) return std::nullopt;
    return Text::Trim(*text);
}

void WriteText(const std::wstring& fileName, const std::wstring& value, Folder folder) {
    Files::WriteText(FilePath(fileName, folder), value);
}

Json::Value ReadJson(const std::wstring& fileName, bool* exists, Folder folder) {
    auto text = Files::ReadBytes(FilePath(fileName, folder));
    if (exists) *exists = text.has_value();
    if (!text) return {};
    return Json::Parse(std::string_view(*text));
}

void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented, Folder folder) {
    const std::wstring path = FilePath(fileName, folder);
    Files::CreateDirectories(Files::DirectoryName(path));
    Files::WriteBytes(path, value.Serialize(indented));
}

} // namespace SonicRacingSaveManager::Config

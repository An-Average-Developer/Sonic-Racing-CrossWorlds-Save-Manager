#include "Core/Files.h"
#include "Core/Json.h"
#include "Services/Backup/BackupService.h"

namespace SonicRacingSaveManager::Services::Backup {

// JsonConvert.DeserializeObject<BackupMetadata>: missing properties keep their defaults ("" / []),
// invalid JSON fails
std::optional<Models::BackupMetadata> BackupService::ReadMetadata(const std::wstring& path) {
    const auto text = Files::ReadBytes(path);
    if (!text) return std::nullopt;
    bool ok = false;
    const Json::Value json = Json::Parse(std::string_view(*text), &ok);
    if (!ok || !json.IsObject()) return std::nullopt;
    Models::BackupMetadata m;
    m.accountId = json["AccountId"].WStr();
    m.backupDate = json["BackupDate"].WStr();
    m.backupName = json["BackupName"].WStr();
    for (const auto& file : json["Files"].Elements()) m.files.push_back(file.WStr());
    return m;
}

// JsonConvert.SerializeObject(metadata, Formatting.Indented), same property order
bool BackupService::WriteMetadata(const std::wstring& path, const Models::BackupMetadata& metadata) {
    Json::Value json = Json::Value::MakeObject();
    json.Set("AccountId", metadata.accountId);
    json.Set("BackupDate", metadata.backupDate);
    Json::Value files = Json::Value::MakeArray();
    for (const auto& f : metadata.files) files.Push(f);
    json.Set("Files", files);
    json.Set("BackupName", metadata.backupName);
    return Files::WriteBytes(path, json.Serialize(true));
}

} // namespace SonicRacingSaveManager::Services::Backup

#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Services/Backup/BackupService.h"

namespace SonicRacingSaveManager::Services::Backup {

namespace {

// The quoted value after `"PersonaName"` at `personaIndex` (content.IndexOf("\"", personaIndex + 13)...)
std::wstring QuotedAfter(const std::wstring& content, size_t personaIndex) {
    const size_t startQuote = content.find(L'"', personaIndex + 13);
    if (startQuote == std::wstring::npos) return {};
    const size_t endQuote = content.find(L'"', startQuote + 1);
    if (endQuote == std::wstring::npos) return {};
    const std::wstring name = content.substr(startQuote + 1, endQuote - startQuote - 1);
    return Text::Trim(name).empty() ? std::wstring() : name;
}

} // namespace

// The account's Steam name: the first PersonaName of userdata\<id>\config\localconfig.vdf, else the
// one within 500 characters after the id in config\loginusers.vdf; "" when not found
std::wstring BackupService::GetSteamAccountName(const std::wstring& steamId) {
    const auto steamDir = System::RegistryString(System::Hive::CurrentUser, L"Software\\Valve\\Steam", L"SteamPath");
    if (!steamDir || steamDir->empty()) return {};

    const std::wstring configPath = Files::Combine(*steamDir, L"userdata", steamId, L"config", L"localconfig.vdf");
    if (Files::Exists(configPath)) {
        const std::wstring content = Files::ReadText(configPath).value_or(L"");
        const size_t personaIndex = content.find(L"\"PersonaName\"");
        if (personaIndex != std::wstring::npos) {
            const std::wstring name = QuotedAfter(content, personaIndex);
            if (!name.empty()) return name;
        }
    }

    const std::wstring loginUsersPath = Files::Combine(*steamDir, L"config", L"loginusers.vdf");
    if (Files::Exists(loginUsersPath)) {
        const std::wstring content = Files::ReadText(loginUsersPath).value_or(L"");
        const size_t idIndex = content.find(steamId);
        if (idIndex != std::wstring::npos) {
            const size_t personaIndex = content.find(L"\"PersonaName\"", idIndex);
            if (personaIndex != std::wstring::npos && personaIndex - idIndex < 500) {
                const std::wstring name = QuotedAfter(content, personaIndex);
                if (!name.empty()) return name;
            }
        }
    }
    return {};
}

} // namespace SonicRacingSaveManager::Services::Backup

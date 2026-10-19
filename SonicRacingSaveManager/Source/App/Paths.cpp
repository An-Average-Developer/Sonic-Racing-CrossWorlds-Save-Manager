#include "App/Paths.h"
#include "App/Config.h"
#include "Core/Files.h"

namespace SonicRacingSaveManager::Paths {

namespace {
constexpr const wchar_t* kFolder = Config::AppFolder;
}

const std::wstring& RoamingDir() {
    static const std::wstring dir = Files::Combine(Files::RoamingAppData(), kFolder);
    return dir;
}

std::wstring RoamingFile(const std::wstring& name) { return Files::Combine(RoamingDir(), name); }

const std::wstring& DataDir() {
    static const std::wstring dir = Files::Combine(Files::LocalAppData(), kFolder);
    return dir;
}

std::wstring DataFile(const std::wstring& name) { return Files::Combine(DataDir(), name); }

const std::wstring& TempDir() {
    static const std::wstring dir = Files::Combine(Files::TempDirectory(), kFolder);
    return dir;
}

std::wstring Asset(const std::wstring& relative) { return Files::Combine(TempDir(), L"Assets", relative); }

std::wstring Translation(const std::wstring& fileName) { return Files::Combine(TempDir(), L"Translations", fileName); }

} // namespace SonicRacingSaveManager::Paths

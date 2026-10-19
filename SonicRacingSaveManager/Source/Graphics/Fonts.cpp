#include "Graphics/Fonts.h"
#include <array>
#include <initializer_list>
#include "Graphics/Device.h"

namespace SonicRacingSaveManager::Fonts {

namespace {

std::array<Family, 4> families;

bool SystemHas(const wchar_t* name) {
    ComPtr<IDWriteFontCollection> system;
    if (FAILED(Device::Get().DWrite()->GetSystemFontCollection(&system))) return false;
    UINT32 index = 0;
    BOOL exists = FALSE;
    system->FindFamilyName(name, &index, &exists);
    return exists != FALSE;
}

std::wstring FirstInstalled(std::initializer_list<const wchar_t*> names) {
    for (const wchar_t* n : names)
        if (SystemHas(n)) return n;
    return *names.begin();
}

} // namespace

void Initialize() {
    families[static_cast<int>(Font::Ui)] = { FirstInstalled({ L"Segoe UI", L"Tahoma" }), nullptr };
    families[static_cast<int>(Font::Icons)] = { FirstInstalled({ L"Segoe MDL2 Assets", L"Segoe Fluent Icons" }), nullptr };
    families[static_cast<int>(Font::Mono)] = { FirstInstalled({ L"Cascadia Code", L"Consolas", L"Courier New" }), nullptr };
    families[static_cast<int>(Font::Consolas)] = { FirstInstalled({ L"Consolas", L"Courier New" }), nullptr };
}

const Family& Get(Font font) { return families[static_cast<int>(font)]; }

} // namespace SonicRacingSaveManager::Fonts

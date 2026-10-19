#pragma once
#include <dwrite_3.h>
#include <string>
#include <wrl/client.h>

namespace SonicRacingSaveManager {

// The font families the old XAML used.
enum class Font {
    Ui,        // "Segoe UI": MaterialDesign3's "Noto" font isn't shipped in the C# exe, so WPF fell back to it
    Icons,     // "Segoe MDL2 Assets" glyphs (&#xE713; etc.)
    Mono,      // "Cascadia Code, Consolas, Courier New" (the dark WpfDialog, monospace ids)
    Consolas,  // explicit FontFamily="Consolas" (changelog lines, hex ids)
};

namespace Fonts {

struct Family {
    std::wstring name;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> collection;  // null = system collection
};

// Picks the first installed family of each fallback list.
void Initialize();
const Family& Get(Font font);

} // namespace Fonts
} // namespace SonicRacingSaveManager

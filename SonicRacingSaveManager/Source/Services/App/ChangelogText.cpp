#include "Core/Text.h"
#include "Services/App/ChangelogData.h"

// The text forms of the changelog (ChangelogData.cpp only holds the entries).
namespace SonicRacingSaveManager::Services::Changelog {

std::wstring EmbeddedText() {
    std::wstring text;
    for (const auto& entry : Entries()) {
        if (!text.empty()) text += L"\n\n";
        text += entry.label.empty() ? entry.version : entry.version + L" " + entry.label;
        for (size_t i = 0; i < entry.lines.size(); ++i) {
            // A blank line between sections, like the published changelog.txt
            if (i > 0 && entry.lines[i].rfind(L"## ", 0) == 0) text += L"\n";
            text += L"\n" + entry.lines[i];
        }
    }
    return Text::Trim(text);
}

} // namespace SonicRacingSaveManager::Services::Changelog

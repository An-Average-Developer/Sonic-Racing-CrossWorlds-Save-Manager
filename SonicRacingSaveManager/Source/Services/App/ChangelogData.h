#pragma once
#include <string>
#include <vector>

// The in-app changelog (newest first), formerly the embedded Resources\changelog.txt.
namespace SonicRacingSaveManager::Services::Changelog {

struct Entry {
    std::wstring version;
    std::wstring label;                // shown after the version ("[7.0.2 UPDATE!]"), usually empty
    std::vector<std::wstring> lines;   // "## Additions" section headers, "- change" lines, "=====Shot=====" move lists
};

const std::vector<Entry>& Entries();

// The whole changelog as changelog.txt text ("3.0.1\n## Additions\n- ...\n\n3.0.0\n...", trimmed):
// what ReadEmbeddedChangelog returned, and what the published changelog.txt holds.
std::wstring EmbeddedText();

} // namespace SonicRacingSaveManager::Services::Changelog

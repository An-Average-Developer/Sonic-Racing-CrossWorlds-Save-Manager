#pragma once
#include <string>
#include <vector>

// What a mod changes, read from the file lists of its containers (the .utoc directory index and the
// .pak index; nothing is extracted): the characters it replaces ("Asset/Character/Blaze/...",
// "CriWareData/VOICE_BLA.acb", the character select icons) and the other kinds of content it has.
namespace SonicRacingSaveManager::Services::Mods {

struct ModContents {
    std::vector<std::wstring> characters;   // "Blaze", "Charmy (Style)", "Alex"... in the order found
    std::vector<const char*> kinds;         // translation keys ("mods_content_voice"...)
};

// `files`: the mod's files (".off" ones too). Blocking: call from worker threads.
ModContents ReadModContents(const std::vector<std::wstring>& files);

} // namespace SonicRacingSaveManager::Services::Mods

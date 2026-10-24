#include "Services/Mods/ModContents.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string_view>
#include "Core/Files.h"
#include "Core/Text.h"

namespace SonicRacingSaveManager::Services::Mods {

namespace {

constexpr uint64_t kMaxIndexSize = 64ull << 20;   // bigger "indexes" are corrupt files

// ── Reading the containers ──────────────────────────────────────────────────

// Bounds-checked little-endian reader; a read past the end clears `ok` and returns zeros
struct Cursor {
    std::string_view data;
    size_t pos = 0;
    bool ok = true;

    template <typename T>
    T Get() {
        T value{};
        if (!ok || data.size() - pos < sizeof(T)) {
            ok = false;
            return value;
        }
        std::memcpy(&value, data.data() + pos, sizeof(T));
        pos += sizeof(T);
        return value;
    }
    void Skip(uint64_t n) {
        if (!ok || data.size() - pos < n) ok = false;
        else pos += static_cast<size_t>(n);
    }
    // FString: length with the terminator, ANSI when positive, UTF-16 when negative (paths are
    // ASCII: anything else becomes '?')
    std::string String() {
        const int32_t n = Get<int32_t>();
        std::string out;
        if (!ok || n == 0) return out;
        if (n > 0) {
            if (static_cast<uint64_t>(n) > data.size() - pos) {
                ok = false;
                return out;
            }
            out.assign(data.data() + pos, static_cast<size_t>(n - 1));
            pos += static_cast<size_t>(n);
            return out;
        }
        const uint64_t chars = static_cast<uint64_t>(-static_cast<int64_t>(n));
        if (chars * 2 > data.size() - pos) {
            ok = false;
            return out;
        }
        for (uint64_t i = 0; i + 1 < chars; ++i) {
            uint16_t c;
            std::memcpy(&c, data.data() + pos + i * 2, 2);
            out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
        }
        pos += static_cast<size_t>(chars * 2);
        return out;
    }
};

template <typename T>
T At(std::string_view data, size_t offset) {
    T value{};
    if (offset + sizeof(T) <= data.size()) std::memcpy(&value, data.data() + offset, sizeof(T));
    return value;
}

class File {
public:
    explicit File(const std::wstring& path)
        : handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr)) {
        LARGE_INTEGER s{};
        if (handle != INVALID_HANDLE_VALUE && GetFileSizeEx(handle, &s)) size = static_cast<uint64_t>(s.QuadPart);
    }
    ~File() {
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    uint64_t Size() const { return size; }
    std::optional<std::string> Read(uint64_t offset, uint64_t length) const {
        if (handle == INVALID_HANDLE_VALUE || length > kMaxIndexSize || offset > size || length > size - offset) return std::nullopt;
        std::string out(static_cast<size_t>(length), '\0');
        size_t done = 0;
        while (done < out.size()) {
            OVERLAPPED at{};
            const uint64_t where = offset + done;
            at.Offset = static_cast<DWORD>(where);
            at.OffsetHigh = static_cast<DWORD>(where >> 32);
            DWORD got = 0;
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(out.size() - done, 1u << 30));
            if (!ReadFile(handle, out.data() + done, chunk, &got, &at) || got == 0) return std::nullopt;
            done += got;
        }
        return out;
    }

private:
    HANDLE handle;
    uint64_t size = 0;
};

// IoStore container (.utoc): the paths in its directory index (FIoDirectoryIndexResource), when it
// has one and it isn't encrypted
void UtocPaths(const std::wstring& path, std::vector<std::string>& out) {
    const File file(path);
    const auto bytes = file.Read(0, file.Size());
    if (!bytes || bytes->size() < 144 || bytes->compare(0, 16, "-==--==--==--==-") != 0) return;
    const std::string_view d = *bytes;
    const uint8_t version = At<uint8_t>(d, 16);
    const uint64_t headerSize = At<uint32_t>(d, 20), entries = At<uint32_t>(d, 24), blocks = At<uint32_t>(d, 28);
    const uint64_t methodCount = At<uint32_t>(d, 36), methodLength = At<uint32_t>(d, 40), indexSize = At<uint32_t>(d, 48);
    const uint8_t flags = At<uint8_t>(d, 80);
    constexpr uint8_t kEncrypted = 2, kSigned = 4, kIndexed = 8;
    if (!(flags & kIndexed) || (flags & kEncrypted) || indexSize == 0) return;
    // Chunk ids, offsets and lengths, perfect hash seeds (v4) and the chunks without one (v5),
    // compression blocks and method names, then the signatures
    uint64_t offset = headerSize + entries * 12 + entries * 10;
    if (version >= 4) offset += static_cast<uint64_t>(At<uint32_t>(d, 84)) * 4;
    if (version >= 5) offset += static_cast<uint64_t>(At<uint32_t>(d, 96)) * 4;
    offset += blocks * 12 + methodCount * methodLength;
    if (flags & kSigned) offset += 4 + static_cast<uint64_t>(At<int32_t>(d, static_cast<size_t>(offset))) * 2 + blocks * 20;
    if (offset > d.size() || indexSize > d.size() - offset) return;

    Cursor c{ d.substr(static_cast<size_t>(offset), static_cast<size_t>(indexSize)) };
    const std::string mount = c.String();
    struct Dir {
        uint32_t name, firstChild, nextSibling, firstFile;
    };
    struct FileEntry {
        uint32_t name, nextFile, userData;
    };
    const uint32_t dirCount = c.Get<uint32_t>();
    if (!c.ok || dirCount > c.data.size() / sizeof(Dir)) return;
    std::vector<Dir> dirs(dirCount);
    for (auto& dir : dirs) dir = c.Get<Dir>();
    const uint32_t fileCount = c.Get<uint32_t>();
    if (!c.ok || fileCount > c.data.size() / sizeof(FileEntry)) return;
    std::vector<FileEntry> files(fileCount);
    for (auto& f : files) f = c.Get<FileEntry>();
    const uint32_t nameCount = c.Get<uint32_t>();
    if (!c.ok || nameCount > c.data.size() / 4) return;
    std::vector<std::string> names(nameCount);
    for (auto& name : names) name = c.String();
    if (!c.ok) return;

    // Walks the tree from the root; `visited` guards against loops in a damaged index
    constexpr uint32_t kNone = 0xFFFFFFFF;
    std::vector<bool> visited(dirs.size());
    const auto name = [&](uint32_t i) { return i < names.size() ? names[i] : std::string(); };
    std::vector<std::pair<uint32_t, std::string>> stack{ { 0u, mount } };
    while (!stack.empty() && out.size() < 200000) {
        auto [i, prefix] = std::move(stack.back());
        stack.pop_back();
        for (; i != kNone && i < dirs.size() && !visited[i]; i = dirs[i].nextSibling) {
            visited[i] = true;
            const std::string here = dirs[i].name == kNone ? prefix : prefix + name(dirs[i].name) + "/";
            for (uint32_t f = dirs[i].firstFile, guard = 0; f != kNone && f < files.size() && guard < files.size(); f = files[f].nextFile, ++guard)
                out.push_back(here + name(files[f].name));
            if (dirs[i].firstChild != kNone) stack.emplace_back(dirs[i].firstChild, here);
        }
    }
}

// Pak file (.pak): the paths in its index, when it isn't encrypted. v10+ keep them in the full
// directory index; older versions list every entry with its record.
void PakPaths(const std::wstring& path, std::vector<std::string>& out) {
    const File file(path);
    const uint64_t tailSize = std::min<uint64_t>(file.Size(), 256);
    const auto tail = file.Read(file.Size() - tailSize, tailSize);
    if (!tail) return;
    // The footer's size depends on the version (the compression method names were added in v8)
    size_t footer = std::string::npos;
    for (const size_t size : { 221u, 225u, 189u, 61u, 53u, 44u }) {
        if (size <= tail->size() && At<uint32_t>(*tail, tail->size() - size + 17) == 0x5A6F12E1) {
            footer = tail->size() - size;
            break;
        }
    }
    if (footer == std::string::npos || At<uint8_t>(*tail, footer + 16) != 0) return;   // missing or encrypted index
    const uint32_t version = At<uint32_t>(*tail, footer + 21);
    const auto index = file.Read(At<uint64_t>(*tail, footer + 25), At<uint64_t>(*tail, footer + 33));
    if (!index) return;
    Cursor c{ *index };
    const std::string mount = c.String();
    const int32_t count = c.Get<int32_t>();
    if (!c.ok || count < 0) return;

    if (version >= 10) {
        c.Skip(8);   // path hash seed
        if (c.Get<int32_t>()) c.Skip(8 + 8 + 20);   // path hash index: offset, size, hash
        if (!c.Get<int32_t>()) return;              // no full directory index
        const uint64_t offset = c.Get<uint64_t>(), size = c.Get<uint64_t>();
        if (!c.ok) return;
        const auto full = file.Read(offset, size);
        if (!full) return;
        Cursor d{ *full };
        const int32_t dirCount = d.Get<int32_t>();
        for (int32_t i = 0; d.ok && i < dirCount && out.size() < 200000; ++i) {
            const std::string dir = d.String();
            const int32_t fileCount = d.Get<int32_t>();
            for (int32_t j = 0; d.ok && j < fileCount; ++j) {
                const std::string name = d.String();
                d.Skip(4);   // encoded entry offset
                if (d.ok) out.push_back(mount + dir + name);
            }
        }
        return;
    }
    for (int32_t i = 0; c.ok && i < count && out.size() < 200000; ++i) {
        const std::string name = c.String();
        // FPakEntry: offset, size, uncompressed size, compression method, (v1) timestamp, hash,
        // (v3+) compression blocks, flags and block size
        c.Skip(8 + 8 + 8);
        const uint32_t method = c.Get<uint32_t>();
        if (version <= 1) c.Skip(8);
        c.Skip(20);
        if (version >= 3) {
            if (method != 0) {
                const int32_t blocks = c.Get<int32_t>();
                c.Skip(blocks > 0 ? static_cast<uint64_t>(blocks) * 16 : 0);
            }
            c.Skip(1 + 4);
        }
        if (c.ok) out.push_back(mount + name);
    }
}

// ── Naming what the paths touch ─────────────────────────────────────────────

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}

// The AI racers' look (Character/Skin/<folder>Skin, VOICE_<code>AI): "Charmy (Style)"
constexpr const wchar_t* kStyle = L" (Style)";

bool EndsWith(std::string_view s, std::string_view suffix) { return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix; }

// The game's character folders (01_Union/Asset/Character/<folder>) and voice / icon codes
struct Character {
    const char* folder;
    const char* code;
    const wchar_t* name;
};
constexpr Character kCharacters[] = {
    { "sonic", "son", L"Sonic" },        { "tails", "tai", L"Tails" },       { "knuckles", "knu", L"Knuckles" },
    { "amy", "amy", L"Amy" },            { "eggman", "egg", L"Eggman" },     { "shadow", "sha", L"Shadow" },
    { "rouge", "rou", L"Rouge" },        { "silver", "sil", L"Silver" },     { "blaze", "bla", L"Blaze" },
    { "metalsonic", "", L"Metal Sonic" }, { "omega", "ome", L"Omega" },      { "vector", "vec", L"Vector" },
    { "espio", "esp", L"Espio" },        { "charmy", "cha", L"Charmy" },     { "zavok", "zav", L"Zavok" },
    { "zazz", "zaz", L"Zazz" },          { "cream", "cre", L"Cream" },       { "big", "big", L"Big" },
    { "eggpawn", "egp", L"Egg Pawn" },   { "sage", "sag", L"Sage" },         { "jet", "jet", L"Jet" },
    { "wave", "wav", L"Wave" },          { "storm", "sto", L"Storm" },
};

// The guests (02_Union/Asset/Character/ExtndGG_CharacterGGNNN), by GGNNN (the game's charaGGNNN texts)
struct Guest {
    int id;
    const wchar_t* name;
};
constexpr Guest kGuests[] = {
    { 1001, L"Werehog" },     { 2001, L"Nine" },     { 2002, L"Rusty Rose" }, { 2003, L"Dread" },  { 4001, L"Steve" },
    { 4002, L"Alex" },        { 4003, L"Creeper" },  { 5001, L"SpongeBob" },  { 5002, L"Patrick" }, { 6001, L"PAC-MAN" },
    { 6002, L"Blinky" },      { 6003, L"Pinky" },    { 6004, L"Inky" },       { 6005, L"Clyde" },  { 10001, L"Hatsune Miku" },
    { 11001, L"Joker" },      { 12001, L"Ichiban Kasuga" }, { 13001, L"Tangle" }, { 13002, L"Whisper" }, { 14001, L"AiAi" },
    { 15001, L"NiGHTS" },
};
// A collab's other models (Joker's is Character11002) go by its first character
std::wstring GuestName(int group, int id) {
    for (const auto& g : kGuests)
        if (g.id == id) return g.name;
    for (const auto& g : kGuests)
        if (g.id / 1000 == group) return g.name;
    return L"Extnd" + std::to_wstring(group);
}

// "KnucklesPrime" → "Knuckles Prime", "Metalsonic" → "Metal Sonic", unknown ones split at capitals
std::wstring FolderName(const std::string& folder) {
    const std::string lower = Lower(folder);
    for (const auto& c : kCharacters) {
        const size_t n = std::strlen(c.folder);
        if (lower == c.folder) return c.name;
        if (lower.size() > n && lower.compare(0, n, c.folder) == 0 && lower.compare(n, std::string::npos, "prime") == 0)
            return std::wstring(c.name) + L" Prime";
    }
    std::wstring out;
    for (size_t i = 0; i < folder.size(); ++i) {
        const char ch = folder[i];
        if (i > 0 && std::isupper(static_cast<unsigned char>(ch)) && std::islower(static_cast<unsigned char>(folder[i - 1]))) out += L' ';
        out += static_cast<wchar_t>(ch == '_' ? ' ' : ch);
    }
    return out;
}

// "sha", "knuPr" (icons), "STO", "SONAI" (voices) → the character, empty when unknown
std::wstring CodeName(std::string code, bool& ai) {
    code = Lower(code);
    ai = code.size() > 3 && code.compare(code.size() - 2, 2, "ai") == 0;
    if (ai) code.resize(code.size() - 2);
    const bool prime = code.size() > 3 && code.compare(code.size() - 2, 2, "pr") == 0;
    if (prime) code.resize(code.size() - 2);
    for (const auto& c : kCharacters)
        if (*c.code && code == c.code) return std::wstring(c.name) + (prime ? L" Prime" : L"");
    return {};
}

// "Extnd10_Character10001" (folders) or "Extnd10_Character001" (icons) → the guest
std::wstring ExtndName(const std::string& text) {
    const std::string lower = Lower(text);
    if (lower.compare(0, 5, "extnd") != 0) return {};
    const int group = std::atoi(lower.c_str() + 5);
    const size_t at = lower.find("_character");
    int id = at == std::string::npos ? 0 : std::atoi(lower.c_str() + at + 10);
    if (id < 1000) id += group * 1000;
    return GuestName(group, id);
}

struct Collector {
    ModContents contents;
    bool characterAssets = false, ui = false;
    std::vector<std::wstring> iconCharacters;   // only named when the mod has no models (AI racers use the icons too)
    // The character folders touched, and whether one of the files carries the character's name
    // (SK_Shadow, AS_hvr_sonic_...): a mod can also add its own assets to another one's folder
    // (Crash's SK_Crashside_Skeleton in Sonic/Mesh), which doesn't replace that character
    std::vector<std::pair<std::wstring, bool>> folders;

    static void AddUnique(std::vector<std::wstring>& list, const std::wstring& name) {
        if (!name.empty() && std::find(list.begin(), list.end(), name) == list.end()) list.push_back(name);
    }
    void AddCharacter(const std::wstring& name) { AddUnique(contents.characters, name); }
    void AddKind(const char* key) {
        if (std::find(contents.kinds.begin(), contents.kinds.end(), key) == contents.kinds.end()) contents.kinds.push_back(key);
    }

    void Add(const std::string& path) {
        const std::string lower = Lower(path);
        const size_t slash = path.rfind('/');
        const std::string fileName = slash == std::string::npos ? path : path.substr(slash + 1);
        const std::string fileLower = Lower(fileName);

        if (const size_t at = lower.find("/asset/character/"); at != std::string::npos) {
            // <folder>/... or Skin/<folder>Skin/... (the AI racers)
            const size_t start = at + 17, end = path.find('/', start);
            if (end == std::string::npos) return;
            std::string folder = path.substr(start, end - start);
            const bool ai = Lower(folder) == "skin";
            if (ai) {
                const size_t end2 = path.find('/', end + 1);
                if (end2 == std::string::npos) return;
                folder = path.substr(end + 1, end2 - end - 1);
                if (Lower(folder).size() > 4 && Lower(folder).compare(folder.size() - 4, 4, "skin") == 0) folder.resize(folder.size() - 4);
            }
            std::wstring name = ExtndName(folder);
            if (name.empty()) {
                // Shared folders (Common, Master_Material)
                if (folder.find('_') != std::string::npos || Lower(folder) == "common") return;
                name = FolderName(folder);
            }
            if (ai) name += kStyle;
            const bool named = fileLower.find(Lower(folder)) != std::string::npos;
            const auto it = std::find_if(folders.begin(), folders.end(), [&](const auto& f) { return f.first == name; });
            if (it == folders.end()) folders.emplace_back(name, named);
            else it->second = it->second || named;
            return;
        }
        if (lower.find("/criwaredata/") != std::string::npos) {
            if (fileLower.compare(0, 6, "voice_") == 0) {
                bool ai = false;
                const std::wstring name = CodeName(fileName.substr(6, fileName.find('.') - 6), ai);
                if (!name.empty()) AddCharacter(ai ? name + kStyle : name);
                AddKind("mods_content_voice");
            } else if (lower.find("/movie/") != std::string::npos || EndsWith(fileLower, ".usm")) {
                AddKind("mods_content_video");
            } else if (fileLower.find("bgm") != std::string::npos) {
                AddKind("mods_content_music");
            } else {
                AddKind("mods_content_sound");
            }
            return;
        }
        if (lower.find("/localization/") != std::string::npos || EndsWith(fileLower, ".locres")) return AddKind("mods_content_text");
        if (lower.find("/jukebox/") != std::string::npos) return AddKind("mods_content_music");
        if (lower.find("/fx/") != std::string::npos) return AddKind("mods_content_effects");
        if (lower.find("content/mods/") != std::string::npos) return AddKind("mods_content_script");
        if (lower.find("/ui/") != std::string::npos) {
            ui = true;
            // Character select icons: UI_CharaIcon_sha_01, UI_Mask_CharaIconHUD_knuPr_01, UI_CharaCut_Extnd10_Character001
            for (const char* prefix : { "ui_charaicon_", "ui_mask_charaiconhud_", "ui_characut_" }) {
                if (fileLower.compare(0, std::strlen(prefix), prefix) != 0) continue;
                const std::string code = fileName.substr(std::strlen(prefix));
                std::wstring name = ExtndName(code);
                if (name.empty()) {
                    bool ai = false;
                    name = CodeName(code.substr(0, code.find('_')), ai);
                }
                AddUnique(iconCharacters, name);
            }
        }
    }
};

} // namespace

ModContents ReadModContents(const std::vector<std::wstring>& files) {
    Collector collector;
    std::vector<std::string> paths;
    for (const auto& file : files) {
        std::wstring name = file;
        if (Text::EndsWith(name, L".off", true)) name.resize(name.size() - 4);
        paths.clear();
        if (Text::EndsWith(name, L".utoc", true)) UtocPaths(file, paths);
        else if (Text::EndsWith(name, L".pak", true)) PakPaths(file, paths);
        for (const auto& path : paths) collector.Add(path);
    }
    // The replaced models first, then the characters only known from their voices
    std::vector<std::wstring> characters;
    for (const auto& [name, named] : collector.folders)
        if (named) Collector::AddUnique(characters, name);
    collector.characterAssets = !characters.empty();
    for (const auto& name : collector.contents.characters) Collector::AddUnique(characters, name);
    collector.contents.characters = std::move(characters);
    // Only the character select pictures: an interface mod, for the characters of the icons
    if (collector.ui && !collector.characterAssets) {
        for (const auto& name : collector.iconCharacters) collector.AddCharacter(name);
        collector.AddKind("mods_content_ui");
    }
    return std::move(collector.contents);
}

} // namespace SonicRacingSaveManager::Services::Mods

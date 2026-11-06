#include "Services/SaveFile/AppSave.h"
#include <algorithm>
#include <cstring>
#include <cwctype>
#include <set>
#include "Core/Files.h"
#include "Core/Text.h"
#include "Data/ColorCatalog.h"
#include "Data/GadgetCatalog.h"
#include "Data/MissionCatalog.h"
#include "Data/MusicCatalog.h"
#include "Data/SaveCatalog.h"
#include "Data/SaveNames.h"
#include "Data/StageCatalog.h"
#include "Localization/Localization.h"
#include "Services/SaveFile/SaveCrypto.h"

namespace SonicRacingSaveManager::Services::SaveFile {

namespace {

using namespace Gvas;
using Kind = Value::Kind;

// ── Building tags ───────────────────────────────────────────────────────────────────────────────

TypeName Simple(const char* name) { return { name, {} }; }
TypeName EnumType(const char* enumName) {
    return { "EnumProperty", { { enumName, { { "/Script/UnionSystem", {} } } }, { "ByteProperty", {} } } };
}
Property BoolProp(const char* name, bool v) {
    Property p;
    p.name = name;
    p.type = Simple("BoolProperty");
    p.value = Value::Bool(v);
    return p;
}
Property EnumProp(const char* name, const char* enumName, const std::wstring& v) {
    Property p;
    p.name = name;
    p.type = EnumType(enumName);
    p.value = Value::Str(v);
    return p;
}
Property UIntProp(const char* name, int64_t v) {
    Property p;
    p.name = name;
    p.type = Simple("UInt32Property");
    p.value = Value::Int(v);
    return p;
}

// A struct value: `like`'s fields (an entry the game wrote, so the struct is complete) with
// `fields` set over them; just `fields` when there is no entry to copy
Value NewStruct(const Value* like, std::vector<Property> fields) {
    Value v;
    v.kind = Kind::Struct;
    if (like && like->kind == Kind::Struct) v.props = like->props;
    for (auto& f : fields) {
        if (Property* has = Find(v.props, f.name)) has->value = std::move(f.value);
        else v.props.push_back(std::move(f));
    }
    return v;
}

void SetField(Value& s, const char* name, const Value& v) {
    if (s.kind != Kind::Struct) return;
    if (Property* p = Find(s.props, name)) p->value = v;
}
bool FieldBool(const Value& s, const char* name) { return s.kind == Kind::Struct && GetBool(s.props, name); }
std::wstring FieldStr(const Value& s, const char* name) {
    if (s.kind != Kind::Struct) return {};
    const Property* p = Find(s.props, name);
    return p && p->value.kind == Kind::Str ? p->value.s : std::wstring{};
}
int64_t FieldInt(const Value& s, const char* name) {
    if (s.kind != Kind::Struct) return 0;
    const Property* p = Find(s.props, name);
    return p && p->value.kind == Kind::Int ? p->value.i : 0;
}

// ── The lists kept as maps ──────────────────────────────────────────────────────────────────────

enum class Shape { Lock, Parts, Driver, Honor, Album, Track, Challenge };

struct MapList {
    const char* section;
    const char* field;
    Shape shape;
    const wchar_t* prefix;   // enum keys ("EDriverId::"); nullptr for int keys
};

const MapList* MapOf(ListId list) {
    static const MapList kDrivers{ "_UserDriverData", "UserDriverList", Shape::Driver, L"EDriverId::" };
    static const MapList kMachines{ "_UserMachineAssemblyData", "CompleteMachineList", Shape::Lock, L"EMachineId::" };
    static const MapList kFront{ "_UserMachineAssemblyData", "FrontPartsList", Shape::Parts, L"EMachineId::" };
    static const MapList kRear{ "_UserMachineAssemblyData", "RearPartsList", Shape::Parts, L"EMachineId::" };
    static const MapList kTire{ "_UserMachineAssemblyData", "TirePartsList", Shape::Parts, L"EMachineId::" };
    static const MapList kColors{ "_UserMachineAssemblyData", "ColorPresetList", Shape::Lock, L"EMachineColorPresetId::" };
    static const MapList kStickers{ "_UserMachineAssemblyData", "StickerlList", Shape::Lock, nullptr };
    static const MapList kHorns{ "_UserMachineAssemblyData", "HornList", Shape::Lock, L"EMachineHornType::" };
    static const MapList kAuras{ "_UserMachineAssemblyData", "AuraList", Shape::Lock, L"EMachineAuraId::" };
    static const MapList kGadgets{ "_UserMachineAssemblyData", "GadgetList", Shape::Lock, L"EGadgetId::" };
    static const MapList kChallenges{ "_UserChallengeData", "ChallengeStatsList", Shape::Challenge, L"EChallengeId::" };
    static const MapList kHonor{ "_UserHonorTitleData", "HonorTitleList", Shape::Honor, nullptr };
    static const MapList kAlbums{ "_UserJukeboxData", "AlbumCondition", Shape::Album, nullptr };
    static const MapList kTracks{ "_UserJukeboxData", "TrackCondition", Shape::Track, nullptr };
    switch (list) {
        case ListId::Drivers: return &kDrivers;
        case ListId::Machines: return &kMachines;
        case ListId::FrontParts: return &kFront;
        case ListId::RearParts: return &kRear;
        case ListId::TireParts: return &kTire;
        case ListId::ColorPresets: return &kColors;
        case ListId::Stickers: return &kStickers;
        case ListId::Horns: return &kHorns;
        case ListId::Auras: return &kAuras;
        case ListId::Gadgets: return &kGadgets;
        case ListId::Challenges: return &kChallenges;
        case ListId::HonorTitles: return &kHonor;
        case ListId::Albums: return &kAlbums;
        case ListId::Tracks: return &kTracks;
        default: return nullptr;
    }
}

std::vector<std::wstring> FromInts(const std::vector<int>& ids) {
    std::vector<std::wstring> out;
    out.reserve(ids.size());
    for (int id : ids) out.push_back(std::to_wstring(id));
    return out;
}

std::vector<std::wstring> CatalogOf(ListId list) {
    namespace C = Data::SaveCatalog;
    switch (list) {
        case ListId::Drivers: return C::Drivers();
        case ListId::Machines:
        case ListId::FrontParts:
        case ListId::RearParts:
        case ListId::TireParts: return C::Machines();
        case ListId::ColorPresets: return C::ColorPresets();
        case ListId::Stickers: return FromInts(C::Stickers());
        case ListId::Horns: return C::Horns();
        case ListId::Auras: return C::Auras();
        case ListId::Gadgets: return C::Gadgets();
        case ListId::Challenges: return C::Challenges();
        case ListId::HonorTitles: return FromInts(C::HonorTitles());
        case ListId::Albums: return FromInts(C::Albums());
        case ListId::Tracks: return FromInts(C::Tracks());
        default: return {};
    }
}

// DLC: the collab packs ("Extnd") and the Werehog (pre-order / time-limited pack). The game shows them
// from your Steam purchases; a save flag doesn't make them appear. Some stickers are the same: the
// Werehog's (10048), the Network Test's (10049) and the SEGA consoles' (22400..22403), and the "1st
// Test Racer" honor title (307); the game drops them again when you don't own what gives them.
// What the game's shop sells. Its lineup comes from SEGA's server, so the save is what tells: an item on
// sale that you haven't bought is kept locked "by price" (ELockType::Price); once bought it is unlocked
// like any other. These are the ones known to be the shop's, to tell them after you buy them too: the
// Duel Medal exchange's horns and auras (Season 2), the Donpa Ticket shop's AI racers in colour
// ("Characters", 5555 each), its Season 2 titles and its gadget kits.
bool KnownShopItem(ListId list, const std::wstring& id) {
    const auto number = [&](size_t from) { return static_cast<int>(Text::ToInt(id.substr(std::min(from, id.size()))).value_or(-1)); };
    switch (list) {
        case ListId::Horns: return Text::StartsWith(id, L"HORNTYPE_") && !Text::Contains(id, L"EXTND") && number(9) >= 40 && number(9) <= 49;
        case ListId::Auras: return Text::StartsWith(id, L"MACHINE_AURA_") && ((number(13) >= 1 && number(13) <= 5) || (number(13) >= 31 && number(13) <= 56));
        case ListId::HonorTitles: return number(0) >= 9001 && number(0) <= 9015;
        case ListId::Drivers: return Text::Contains(id, L"RacerColor");
        case ListId::Gadgets: {
            static const std::set<std::wstring> kits = {
                L"Various9_FastShortCut", L"Various17_AccelCharaCustom", L"Various18_HandleCharaCustom", L"Various19_PowerCharaCustom",
                L"Various20_DashCharaCustom", L"Various23_CatchUp", L"Various25_PlaneOperateCustom", L"Various26_BoatOperateCustom",
                L"Various27_LotsOfWisps", L"Various29_HandleCharaCustom2", L"Various30_DriftSpinKit", L"Various31_ItemBusterKit",
                L"Various32_DashComboKit", L"Various33_SpeedCharaCustom2", L"Various34_JustChargeKit", L"Various35_UltimateChargeKit",
                L"Various36_AirTrickActionKit", L"Various37_SlipstreamKit", L"Various38_SpinDashKit",
            };
            return kits.count(id) > 0;
        }
        default: return false;
    }
}

bool g_konamiExtras = false;

// The World Championship 2026's placing titles (2006, Witness to Greatness, is for taking part)
bool IsRankedTitle(const std::wstring& id) {
    const int64_t n = Text::ToInt(id).value_or(-1);
    return n >= 2000 && n <= 2005;
}

bool IsDlc(ListId list, const std::wstring& id) {
    // The collab characters' music comes with their packs, like the characters
    if (list == ListId::Albums || list == ListId::Tracks) {
        const int n = static_cast<int>(Text::ToInt(id).value_or(-1));
        const auto* t = list == ListId::Tracks ? Data::MusicCatalog::FindTrack(n) : nullptr;
        const auto* a = Data::MusicCatalog::FindAlbum(t ? t->album : n);
        return a && a->dlc;
    }
    if (list == ListId::HonorTitles) return id == L"307";
    // The festivals' horns (HORNTYPE_EXTND_..) are event rewards the save keeps, like their stickers
    if (list == ListId::Horns) return false;
    if (list == ListId::Stickers) {
        const int64_t n = Text::ToInt(id).value_or(-1);
        return n == 10048 || n == 10049 || (n >= 22400 && n <= 22403);
    }
    // The machines of the collab packs come in their packs (pakchunk50..53) like their characters: the
    // complete machines 11..14, with their parts and their own colours; 51 are the bonus ones (Blue Star,
    // the Werehog's Beast Spike)
    const auto collabMachine = [](const std::wstring& machine) {
        const size_t us = machine.rfind(L'_');
        const int n = us == std::wstring::npos ? -1 : static_cast<int>(Text::ToInt(machine.substr(us + 1)).value_or(-1));
        return n >= 11;
    };
    if (list == ListId::Machines || list == ListId::FrontParts || list == ListId::RearParts || list == ListId::TireParts) return collabMachine(id);
    if (list == ListId::ColorPresets) {
        const int number = static_cast<int>(Text::ToInt(id.substr(std::min<size_t>(id.size(), 11))).value_or(-1));
        for (const auto& c : Data::ColorCatalog::Presets())
            if (c.id == number) return *c.machine && collabMachine(Text::Widen(c.machine));
        return false;
    }
    return Text::Contains(id, L"extnd", true) || id == L"Werehog";
}

std::wstring KeyText(const MapList& m, const Value& key) {
    if (key.kind == Kind::Int) return std::to_wstring(key.i);
    if (m.prefix && Text::StartsWith(key.s, m.prefix)) return key.s.substr(wcslen(m.prefix));
    return key.s;
}

Value KeyValue(const MapList& m, const std::wstring& id) {
    if (!m.prefix) return Value::Int(Text::ToInt(id).value_or(0));
    return Value::Str(m.prefix + id);
}

Value* EntryOf(Value& map, const MapList& m, const std::wstring& id) {
    // The key as the save writes it, made once (this runs for every row of every list)
    if (!m.prefix) {
        const auto n = Text::ToInt(id);
        if (!n) return nullptr;
        for (auto& [key, val] : map.entries)
            if (key.kind == Kind::Int && key.i == *n) return &val;
        return nullptr;
    }
    const std::wstring full = m.prefix + id;
    for (auto& [key, val] : map.entries)
        if (key.kind == Kind::Str && key.s == full) return &val;
    return nullptr;
}

// "DrEggmanRacer" → "Dr Eggman Racer", "Item2_PlusStock" → "Item 2 Plus Stock"
std::wstring Pretty(const std::wstring& id) {
    std::wstring out;
    for (size_t k = 0; k < id.size(); ++k) {
        const wchar_t c = id[k];
        if (c == L'_') {
            out.push_back(L' ');
            continue;
        }
        if (k > 0 && !out.empty() && out.back() != L' ') {
            const wchar_t prev = id[k - 1];
            const bool lowerToUpper = std::iswupper(c) && std::iswlower(prev);
            const bool letterToDigit = std::iswdigit(c) && std::iswalpha(prev) && !std::iswupper(prev);
            if (lowerToUpper || letterToDigit) out.push_back(L' ');
        }
        out.push_back(c);
    }
    return out;
}

// The key of SaveNames for a list's ids (nullptr: the game has no names for it)
const char* NamePrefix(ListId list) {
    switch (list) {
        case ListId::Drivers: return "driver:";
        case ListId::Machines: return "machine:";
        case ListId::FrontParts: return "front:";
        case ListId::RearParts: return "rear:";
        case ListId::TireParts: return "tire:";
        case ListId::Gadgets: return "gadget:";
        case ListId::Horns: return "horn:";
        case ListId::Auras: return "aura:";
        case ListId::Challenges: return "challenge:";
        case ListId::Stickers: return "sticker:";
        case ListId::HonorTitles: return "title:";
        case ListId::Tracks: return "track:";
        default: return nullptr;
    }
}

// What the game calls `key` in the app's language
const wchar_t* GameName(const std::string& key) { return Data::SaveNames::Find(key, Loc::CurrentLang()); }

std::wstring RowName(ListId list, const std::wstring& id) {
    // The jukebox's, from its tables (its final laps written as the others: "Get Higher (for Final Lap)")
    if (list == ListId::Albums || list == ListId::Tracks) {
        const int n = static_cast<int>(Text::ToInt(id).value_or(-1));
        if (list == ListId::Albums)
            if (const auto* a = Data::MusicCatalog::FindAlbum(n)) return Data::MusicCatalog::AlbumName(*a, Loc::CurrentLang());
        if (list == ListId::Tracks)
            if (const auto* t = Data::MusicCatalog::FindTrack(n)) return Data::MusicCatalog::TrackName(*t, Loc::CurrentLang());
    }
    if (const char* prefix = NamePrefix(list))
        if (const wchar_t* name = GameName(prefix + Text::Narrow(id))) return name;
    switch (list) {
        case ListId::Drivers:
            // The guest characters: "Extnd10_Character10001" → "DLC 10 · 10001"
            if (Text::StartsWith(id, L"Extnd") && id.size() > 7) {
                const size_t us = id.find(L'_');
                const std::wstring pack = id.substr(5, us == std::wstring::npos ? 2 : us - 5);
                const std::wstring who = id.size() >= 5 ? id.substr(id.size() - 5) : id;
                return Loc::Fmt("savefile_dlc_character", pack, who);
            }
            return Pretty(id);
        case ListId::ColorPresets: {
            // Named after the machine whose colours they are, or the festival team's
            const int number = static_cast<int>(Text::ToInt(id.substr(std::min<size_t>(id.size(), 11))).value_or(-1));
            for (const auto& c : Data::ColorCatalog::Presets()) {
                if (c.id != number) continue;
                if (*c.machine)
                    if (const wchar_t* machine = Data::SaveNames::Find(std::string("machine:") + c.machine, Loc::CurrentLang()))
                        return Loc::Fmt("savefile_color_machine", machine);
                if (c.festival) return Loc::Fmt("savefile_color_festival", std::to_wstring(c.festival));
            }
            return Loc::Fmt("savefile_color_preset", id.substr(std::min<size_t>(id.size(), 11)));
        }
        case ListId::Horns: return Loc::Fmt("savefile_horn", Text::Replace(Text::Replace(id, L"HORNTYPE_", L""), L"EXTND_", L"DLC "));
        case ListId::Auras: return id == L"MACHINE_AURA_NONE" ? Loc::Get("savefile_aura_none") : Loc::Fmt("savefile_aura", Text::Replace(id, L"MACHINE_AURA_", L""));
        case ListId::Stickers: return Loc::Fmt("savefile_sticker", id);
        case ListId::HonorTitles: return Loc::Fmt("savefile_honor_title", id);
        case ListId::Albums: return Loc::Fmt("savefile_album", id);
        case ListId::Tracks: return Loc::Fmt("savefile_track", id);
        case ListId::Challenges: return Loc::Fmt("savefile_challenge", Text::Replace(id, L"CHALLENGE", L""));
        case ListId::Gadgets: {
            // "Run12_UltimateChargeAttack" → "Ultimate Charge Attack (Run 12)"
            const size_t us = id.find(L'_');
            if (us == std::wstring::npos) return Pretty(id);
            return Pretty(id.substr(us + 1)) + L"  (" + Pretty(id.substr(0, us)) + L")";
        }
        default: return Pretty(id);
    }
}

// "EDriverId::Sonic" → "Sonic"
// "SPD_13" -> 13 (-1 when it has no number)
int MachineNumber(const std::wstring& id) {
    const size_t us = id.rfind(L'_');
    return us == std::wstring::npos ? -1 : static_cast<int>(Text::ToInt(id.substr(us + 1)).value_or(-1));
}

std::wstring AfterColons(const std::wstring& s) {
    const size_t at = s.find(L"::");
    return at == std::wstring::npos ? s : s.substr(at + 2);
}

// ── Profile numbers ─────────────────────────────────────────────────────────────────────────────

struct NumberSpec {
    const char* key;
    const char* section;
    const char* field;
    int64_t min, max, suggested;
    // A currency (the Currencies section): shown at 0 when the save doesn't have it yet, and added on
    // the first edit. The game leaves out what is still at its default (0, or a section all at 0: the
    // Duel Medals' until you earn one), and reads what is added back.
    bool currency = false;
    const char* sectionType = nullptr;   // the section's struct, to add it when it's missing
};
constexpr NumberSpec kNumbers[] = {
    // Currencies, in the order they are shown
    { "donpa_tickets", "_UserCommonData", "DonpaTicket", 0, INT32_MAX, 9999999, true },
    { "duel_medals", "_UserDuelMedalData", "OnHand", 0, INT32_MAX, 9999999, true, "UserDuelMedalData" },   // Season 2 (2.0)
    { "red_star_rings", "_UserCommonData", "RedStarRing", 0, INT32_MAX, 999999, true },
    { "gadget_fragments", "_UserMachineAssemblyData", "GadgetFragmentNum", 0, INT32_MAX, 9999, true },
    { "total_donpa_tickets", "_UserCommonData", "TotalDonpaTicket", 0, INT32_MAX, 9999999, true },
    { "used_donpa_tickets", "_UserCommonData", "UseDonpaTicket", 0, INT32_MAX, 0, true },
    { "total_red_star_rings", "_UserCommonData", "TotalRedStarRing", 0, INT32_MAX, 999999, true },
    { "festa_points", "_UserCommonData", "festaPoint", 0, INT32_MAX, 9999, true },
    // The rest of the profile
    { "level", "_UserCommonData", "PlayerLevel", 1, 9999, 999 },
    { "race_count", "_UserCommonData", "TotalRaceCount", 0, INT32_MAX, 9999 },
    { "rate_high", "_UserCommonData", "RateHighPoint", 0, INT32_MAX, 9999 },
    { "gadget_slots", "_UserMachineAssemblyData", "GadgetSlotNum", 1, kGadgetSlotsAll, kGadgetSlotsAll },   // (7: the special slot too)
    { "gp_wins", "_UserGrandPrixData", "GrandPrixWinCount", 0, 255, 255 },
    { "gp_finishes", "_UserGrandPrixData", "GrandPrixFinishesCount", 0, 255, 255 },
    { "player_icon", "_AppFriendUserData", "_PlayerIconId", 0, INT32_MAX, 0 },   // (gone since 2.0)
};

constexpr uint8_t kBestRank = 6;   // GrandPrixCompletionStatus: 6 = won, per speed class

} // namespace

// ── File ────────────────────────────────────────────────────────────────────────────────────────

void SetKonamiExtras(bool on) { g_konamiExtras = on; }
bool KonamiExtras() { return g_konamiExtras; }

AppSave AppSave::Open(const std::wstring& path) {
    const auto bytes = Files::ReadBytes(path);
    if (!bytes) throw SaveError(Loc::Fmt("savefile_error_read", path));
    const auto plain = SaveCrypto::Decrypt(*bytes);
    if (!plain) throw SaveError(Loc::Get("savefile_error_decrypt"));
    AppSave s;
    try {
        s.file = Gvas::Parse(*plain);
    } catch (const Gvas::Error& e) {
        throw SaveError(Loc::Fmt("savefile_error_parse", Text::Widen(e.what())));
    }
    // Only edit what reads back to the same bytes
    if (Gvas::Serialize(s.file) != *plain) throw SaveError(Loc::Fmt("savefile_error_parse", L"roundtrip"));
    if (!s.Section("_UserCommonData")) throw SaveError(Loc::Get("savefile_error_not_app"));
    s.path = path;
    s.size = bytes->size();
    s.writtenAt = Files::LastWriteTime(path).value_or(0);
    return s;
}

void AppSave::Write() {
    const std::string encrypted = Encoded();
    WriteFile(path, encrypted);
    Written(encrypted.size());
}

std::string AppSave::Encoded() const { return SaveCrypto::Encrypt(Gvas::Serialize(file)); }

void AppSave::WriteFile(const std::wstring& target, const std::string& bytes) {
    // Through a temporary file, so a failed write never leaves half a save
    const std::wstring temp = target + L".tmp";
    if (!Files::WriteBytes(temp, bytes) || !Files::Move(temp, target, true)) {
        Files::Delete(temp);
        throw SaveError(Loc::Fmt("savefile_error_write", target));
    }
}

void AppSave::Written(uint64_t bytes) {
    size = bytes;
    writtenAt = Files::LastWriteTime(path).value_or(0);
    dirty = false;
}

bool AppSave::ChangedOnDisk() const { return Files::LastWriteTime(path).value_or(0) != writtenAt; }

Gvas::Props* AppSave::Section(const char* name) { return Gvas::Struct(file.root, name); }
const Gvas::Props* AppSave::Section(const char* name) const { return const_cast<AppSave*>(this)->Section(name); }

// ── Numbers ─────────────────────────────────────────────────────────────────────────────────────

std::vector<NumberField> AppSave::Numbers() const {
    std::vector<NumberField> out;
    for (const auto& n : kNumbers) {
        const Props* sec = Section(n.section);
        const Property* p = sec ? Gvas::Find(*sec, n.field) : nullptr;
        if (p && p->value.kind == Kind::Int) out.push_back({ n.key, p->value.i, n.min, n.max, n.suggested, n.currency });
        // Not in the save yet (still 0): a currency shows anyway, when it can be added
        else if (!p && n.currency && (sec || n.sectionType)) out.push_back({ n.key, 0, n.min, n.max, n.suggested, n.currency });
    }
    return out;
}

void AppSave::SetNumber(const std::string& key, int64_t value) {
    // The statistics' race totals
    if (key.rfind("stat_", 0) == 0) {
        Props* stats = Section("_StatsData");
        Props* details = stats ? Gvas::Struct(*stats, "RaceDetails") : nullptr;
        Property* p = details ? Gvas::Find(*details, key.substr(5)) : nullptr;
        const int64_t v = std::clamp<int64_t>(value, 0, INT32_MAX);
        if (p && p->value.kind == Kind::Int && p->value.i != v) {
            p->value.i = v;
            dirty = true;
        }
        return;
    }
    for (const auto& n : kNumbers) {
        if (key != n.key) continue;
        const int64_t v = std::clamp(value, n.min, n.max);
        Props* sec = Section(n.section);
        if (!sec) {
            if (!n.sectionType || v == 0) return;
            // The section, as the game writes its others: StructProperty <its struct> (/Script/UnionSystem),
            // before the mission data that follows it in the game's own order (or last)
            Property section;
            section.name = n.section;
            section.type = { "StructProperty", { { n.sectionType, { { "/Script/UnionSystem", {} } } } } };
            section.value.kind = Kind::Struct;
            auto at = std::find_if(file.root.begin(), file.root.end(), [](const Property& q) {
                return q.name == "_UserPlayMissionData" || q.name == "_UserCurrencyData" || q.name == "_UserAppendDodonpaEvent";
            });
            file.root.insert(at, std::move(section));
            sec = Section(n.section);
            if (!sec) return;
        }
        Property* p = Gvas::Find(*sec, n.field);
        if (!p) {
            if (!n.currency) return;
            Property field;
            field.name = n.field;
            field.type = Simple("IntProperty");
            field.value = Value::Int(v);
            sec->push_back(std::move(field));
            dirty = true;
            return;
        }
        if (p->value.kind == Kind::Int && p->value.i != v) {
            p->value.i = v;
            dirty = true;
        }
        return;
    }
}

// ── Rows ────────────────────────────────────────────────────────────────────────────────────────

std::vector<ItemRow> AppSave::Rows(ListId list) const {
    std::vector<ItemRow> rows;
    auto& self = const_cast<AppSave&>(*this);

    if (const MapList* m = MapOf(list)) {
        Props* sec = self.Section(m->section);
        Value* map = sec ? Gvas::Container(*sec, m->field, Kind::Map) : nullptr;
        if (!map) return rows;
        // What the game needs: the selected character / machine, the parts your machines use
        std::set<std::wstring> kept;
        if (list == ListId::Drivers || list == ListId::Machines)
            if (Props* common = self.Section("_UserCommonData")) {
                const Property* d = Gvas::Find(*common, list == ListId::Drivers ? "SelectedDriverId" : "SelectedMachineId");
                if (d && d->value.kind == Kind::Str) kept.insert(AfterColons(d->value.s));
            }
        if (m->shape == Shape::Parts) {
            const char* field = list == ListId::FrontParts ? "FrontPartsId" : list == ListId::RearParts ? "RearPartsId" : "TirePartsId";
            if (Value* custom = Gvas::Container(*sec, "CustomizeDataList", Kind::Map))
                for (auto& [k, v] : custom->entries) kept.insert(AfterColons(FieldStr(v, field)));
        }
        // ...and the title on the player's card
        if (list == ListId::HonorTitles)
            if (const int title = SelectedTitle(); title >= 0) kept.insert(std::to_wstring(title));
        std::set<std::wstring> honorUnlocked;
        if (m->shape == Shape::Honor)
            if (Value* a = Gvas::Container(*sec, "UnlockedHonorTitleList", Kind::Array))
                for (const auto& v : a->items) honorUnlocked.insert(std::to_wstring(v.i));

        std::vector<std::wstring> ids = CatalogOf(list);
        std::set<std::wstring> known(ids.begin(), ids.end());
        const std::set<std::wstring> catalog = known;   // what the game has, before the save's own ids
        for (const auto& [key, val] : map->entries) {
            const std::wstring id = KeyText(*m, key);
            if (!known.count(id)) {
                ids.push_back(id);
                known.insert(id);
            }
        }
        for (const std::wstring& id : ids) {
            Value* e = EntryOf(*map, *m, id);
            if (list == ListId::Gadgets && id == L"NonGadget") continue;   // the empty slot, not a gadget
            ItemRow row;
            row.id = id;
            row.name = RowName(list, id);
            row.canLock = !kept.count(id);
            const bool isNew = e && (FieldBool(*e, "bIsNew") || FieldBool(*e, "bNew"));
            switch (m->shape) {
                case Shape::Lock:
                    row.state = !e ? ItemState::Missing : FieldStr(*e, "LockType") == L"ELockType::None" ? ItemState::Unlocked : ItemState::Locked;
                    if (e && row.state == ItemState::Locked)
                        row.detail = FieldStr(*e, "LockType") == L"ELockType::Price" ? Loc::Get("savefile_lock_price") : Loc::Get("savefile_lock_norm");
                    break;
                case Shape::Parts: row.state = e ? ItemState::Unlocked : ItemState::Missing; break;
                case Shape::Driver: row.state = !e ? ItemState::Missing : FieldBool(*e, "bIsSelectable") ? ItemState::Unlocked : ItemState::Locked; break;
                case Shape::Honor: {
                    // Unlocked when it is in both the title list and the unlocked ids
                    const bool listed = honorUnlocked.count(id) > 0;
                    row.state = e && listed ? ItemState::Unlocked : e || listed ? ItemState::Locked : ItemState::Missing;
                    break;
                }
                case Shape::Album:
                    row.state = !e ? ItemState::Missing : FieldBool(*e, "bUnlocked") && FieldBool(*e, "bAvailable") ? ItemState::Unlocked : ItemState::Locked;
                    break;
                case Shape::Track: row.state = !e ? ItemState::Missing : FieldBool(*e, "bAvailable") ? ItemState::Unlocked : ItemState::Locked; break;
                case Shape::Challenge: {
                    const std::wstring st = e ? FieldStr(*e, "ProgressState") : L"";
                    row.state = !e ? ItemState::Missing
                                   : (st == L"EChallengeProgressState::Acquired" || st == L"EChallengeProgressState::Verified") ? ItemState::Unlocked
                                                                                                                                  : ItemState::Locked;
                    // Its count against what it needs: "3 / 10"
                    const auto n = Text::ToInt(Text::Replace(id, L"CHALLENGE", L""));
                    row.progress = e ? FieldInt(*e, "StatsCount") : 0;
                    row.goal = n ? Data::SaveCatalog::ChallengeGoal(static_cast<int>(*n)) : 0;
                    if (row.goal > 0) row.detail = Loc::Fmt("savefile_challenge_progress", Text::FromInt(row.progress), Text::FromInt(row.goal));
                    else if (e) row.detail = Loc::Fmt("savefile_challenge_count", std::to_wstring(row.progress));
                    if (n)
                        if (const wchar_t* title = GameName("title:" + std::to_string(*n))) row.hint = Loc::Fmt("savefile_challenge_reward", title);
                    break;
                }
            }
            if (const char* prefix = NamePrefix(list)) {
                const std::string key = prefix + Text::Narrow(id);
                // The game has no name for an enum id it doesn't use yet (ids from saves are real)
                const bool fromEnum = list != ListId::Stickers && list != ListId::HonorTitles && list != ListId::Tracks;
                if (fromEnum && !GameName(key) && id != L"MACHINE_AURA_NONE") {
                    row.detail = Loc::Get("savefile_unreleased");
                    row.unreleased = true;
                } else if (row.state != ItemState::Unlocked) {
                    // How the game says to get it (parts, auras)
                    if (const wchar_t* how = GameName(key + "#unlock")) row.hint = how;
                }
            }
            // DLC: Steam gives it from your purchases; it is left as it is. What the save doesn't have is
            // listed too, as locked (not a slot the game doesn't use yet: one it has no name for)
            if (IsDlc(list, id)) {
                if (!e && row.unreleased) continue;
                row.dlc = true;
                row.canLock = row.canUnlock = false;
                row.detail = Loc::Get("savefile_dlc");
                if (row.state == ItemState::Missing) row.state = ItemState::Locked;
            }
            if (isNew && row.detail.empty()) row.detail = Loc::Get("savefile_new");
            row.shop = (e && FieldStr(*e, "LockType") == L"ELockType::Price") || KnownShopItem(list, id);
            // The horn and machine enums keep slots for what is still to come ("Horn DLC 23", "SPD_20"...):
            // left out unless a save has one unlocked (a locked one there is only an empty slot, like
            // HORNTYPE_EXTND_22)
            const bool machineLike = list == ListId::Machines || list == ListId::FrontParts || list == ListId::RearParts || list == ListId::TireParts;
            if (list == ListId::Horns && row.unreleased && row.state != ItemState::Unlocked) continue;
            // An aura the game has no data for (the enum's empty 021..030) isn't one, even when the save keeps it
            if (list == ListId::Auras && row.unreleased) continue;
            // The championship's placings: only with the Konami code (or when the save has one)
            if (list == ListId::HonorTitles && !g_konamiExtras && IsRankedTitle(id) && row.state != ItemState::Unlocked) continue;
            // An album with no tracks of its own is a slot the game keeps for one still to come (5003), unless a
            // save has it; Favourites (3002) is the one the game fills itself
            if (list == ListId::Albums && !e && id != L"3002" && Data::MusicCatalog::TracksOf(static_cast<int>(Text::ToInt(id).value_or(-1))).empty())
                continue;
            // A part or colour the game doesn't have is left out even when the save keeps it: the game lists
            // tyres for the Boost machines, which float ("DSH_01"), and empty colour slots ("ColorPreset35")
            if (machineLike && row.unreleased) continue;
            if (list == ListId::ColorPresets && !catalog.count(id)) continue;
            // The complete machines are 11 and up (and 51): 01..10 are built from their parts
            if (list == ListId::Machines && !e && MachineNumber(id) < 11) continue;
            rows.push_back(std::move(row));
        }
        return rows;
    }

    switch (list) {
        case ListId::GrandPrix: {
            Props* gp = self.Section("_UserGrandPrixData");
            if (!gp) break;
            if (Value* cups = Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array))
                for (size_t k = 0; k < cups->items.size(); ++k) {
                    const Value& cup = cups->items[k];
                    ItemRow row;
                    row.id = L"cup:" + std::to_wstring(k);
                    const wchar_t* name = GameName("cup:" + std::to_string(k));
                    row.name = name ? name : Loc::Fmt("savefile_cup", std::to_wstring(k + 1));
                    row.unreleased = !name;
                    int won = 0, total = 0;
                    for (const char* f : { "Normal", "Mirror" })
                        if (const Property* p = Gvas::Find(cup.props, f); p && p->value.kind == Kind::Array)
                            for (const auto& r : p->value.items) {
                                ++total;
                                won += r.i >= kBestRank;
                            }
                    const bool opened = FieldBool(cup, "bOpened");
                    row.state = opened && won == total ? ItemState::Unlocked : ItemState::Locked;
                    row.detail = row.unreleased ? Loc::Get("savefile_unreleased")
                                 : opened       ? Loc::Fmt("savefile_cup_progress", std::to_wstring(won), std::to_wstring(total))
                                                : Loc::Get("savefile_closed");
                    rows.push_back(std::move(row));
                }
            if (Value* ev = Gvas::Container(*gp, "EventCompleted", Kind::Array))
                for (size_t k = 0; k < ev->items.size(); ++k)
                    rows.push_back({ L"event:" + std::to_wstring(k), Loc::Fmt("savefile_event", std::to_wstring(k + 1)),
                                     ev->items[k].b ? ItemState::Unlocked : ItemState::Locked, L"" });
            for (const char* f : { "OpenSuperSonicSpeed", "OpenMirror" })
                if (const Property* p = Gvas::Find(*gp, f))
                    rows.push_back({ Text::Widen(f), Loc::Get(std::string("savefile_") + f), p->value.b ? ItemState::Unlocked : ItemState::Locked, L"" });
            break;
        }
        case ListId::TimeTrial:
        case ListId::RedStarRings: {
            const bool tt = list == ListId::TimeTrial;
            Props* sec = self.Section(tt ? "_UserTimeTrialData" : "_UserCommonData");
            Value* arr = sec ? Gvas::Container(*sec, tt ? "TimeTrialCourseDataArray" : "RedStarRingAcquisitionPerStage", Kind::Array) : nullptr;
            if (!arr) break;
            for (size_t k = 0; k < arr->items.size(); ++k) {
                const Property* flags = Gvas::Find(arr->items[k].props, tt ? "Acquistion" : "Acquisition");
                if (!flags || flags->value.kind != Kind::Array) continue;
                int on = 0;
                for (const auto& b : flags->value.items) on += b.b;
                const int total = static_cast<int>(flags->value.items.size());
                // The arrays have a slot per EStageId; only the real courses have a name
                const wchar_t* course = GameName("stageidx:" + std::to_string(k));
                if (!course) continue;
                ItemRow row{ std::to_wstring(k), course, on == total ? ItemState::Unlocked : ItemState::Locked, std::to_wstring(on) + L" / " + std::to_wstring(total) };
                for (const auto& b : flags->value.items) row.flags.push_back(b.b);
                rows.push_back(std::move(row));
            }
            break;
        }
        case ListId::Tutorials: {
            Props* sec = self.Section("_UserTutorialData");
            if (Value* m = sec ? Gvas::Container(*sec, "TutorialCompletedFlags", Kind::Map) : nullptr)
                for (const auto& [key, val] : m->entries) {
                    const std::wstring id = AfterColons(key.s);
                    const wchar_t* course = GameName("stage:" + Text::Narrow(id));
                    rows.push_back({ id, Loc::Fmt("savefile_tutorial", course ? std::wstring(course) : id), val.b ? ItemState::Unlocked : ItemState::Locked, L"" });
                }
            if (Props* hints = self.Section("_UserHintData"))
                if (Value* a = Gvas::Container(*hints, "UserHintArray", Kind::Array)) {
                    int read = 0;
                    for (const auto& h : a->items) read += FieldBool(h, "bIsRead");
                    const int total = static_cast<int>(a->items.size());
                    rows.push_back({ L"hints", Loc::Get("savefile_hints"), read == total ? ItemState::Unlocked : ItemState::Locked,
                                     std::to_wstring(read) + L" / " + std::to_wstring(total) });
                }
            break;
        }
        default: break;
    }
    return rows;
}

// ── Unlock / lock ───────────────────────────────────────────────────────────────────────────────

bool AppSave::Unlock(ListId list, const std::wstring& id) {
    bool changed = false;
    const auto set = [&](Value& v, bool on) {
        if (v.b != on) {
            v.b = on;
            changed = true;
        }
    };
    const auto setField = [&](Value& s, const char* name, const Value& v) {
        if (s.kind != Kind::Struct) return;
        Property* p = Find(s.props, name);
        if (!p) {
            return;
        }
        const bool same = p->value.kind == v.kind && p->value.i == v.i && p->value.b == v.b && p->value.s == v.s;
        if (!same) {
            p->value = v;
            changed = true;
        }
    };

    if (const MapList* m = MapOf(list)) {
        Props* sec = Section(m->section);
        Value* map = sec ? Gvas::Container(*sec, m->field, Kind::Map) : nullptr;
        if (!map) return false;
        Value* e = EntryOf(*map, *m, id);
        if (IsDlc(list, id)) return false;   // DLC: Steam decides (what you own is unlocked already)
        const Value* like = map->entries.empty() ? nullptr : &map->entries.front().second;
        const auto add = [&](std::vector<Property> fields) {
            map->entries.emplace_back(KeyValue(*m, id), NewStruct(like, std::move(fields)));
            changed = true;
        };
        switch (m->shape) {
            case Shape::Lock:
                if (e) setField(*e, "LockType", Value::Str(L"ELockType::None"));
                else add({ BoolProp("bIsNew", true), EnumProp("LockType", "ELockType", L"ELockType::None") });
                break;
            case Shape::Parts:
                if (!e) add({ BoolProp("bIsNew", true) });
                break;
            case Shape::Driver:
                if (e) setField(*e, "bIsSelectable", Value::Bool(true));
                else add({ BoolProp("bIsSelectable", true), BoolProp("bIsNew", true) });
                break;
            case Shape::Honor: {
                if (!e) add({ BoolProp("bIsNew", true) });
                if (Value* a = Gvas::Container(*sec, "UnlockedHonorTitleList", Kind::Array)) {
                    const int64_t n = Text::ToInt(id).value_or(-1);
                    if (std::none_of(a->items.begin(), a->items.end(), [n](const Value& v) { return v.i == n; })) {
                        a->items.push_back(Value::Int(n));
                        changed = true;
                    }
                }
                break;
            }
            case Shape::Album:
                if (e) {
                    setField(*e, "bUnlocked", Value::Bool(true));
                    setField(*e, "bAvailable", Value::Bool(true));
                } else {
                    add({ BoolProp("bUnlocked", true), BoolProp("bAvailable", true), BoolProp("bFavorite", false), BoolProp("bNew", true) });
                }
                break;
            case Shape::Track:
                if (e) setField(*e, "bAvailable", Value::Bool(true));
                else add({ BoolProp("bAvailable", true), BoolProp("bFavorite", false), BoolProp("bNew", true) });
                break;
            case Shape::Challenge: {
                // Done, and its count up to what it needs (x / x, as the game would have it)
                const int64_t goal = Data::SaveCatalog::ChallengeGoal(static_cast<int>(Text::ToInt(Text::Replace(id, L"CHALLENGE", L"")).value_or(0)));
                const std::wstring st = e ? FieldStr(*e, "ProgressState") : L"";
                if (e && st != L"EChallengeProgressState::Acquired" && st != L"EChallengeProgressState::Verified")
                    setField(*e, "ProgressState", Value::Str(L"EChallengeProgressState::Acquired"));
                if (e && goal > 0 && FieldInt(*e, "StatsCount") < goal) setField(*e, "StatsCount", Value::Int(goal));
                else if (!e)
                    add({ UIntProp("StatsCount", goal), EnumProp("ProgressState", "EChallengeProgressState", L"EChallengeProgressState::Acquired") });
                break;
            }
        }
        if (changed) dirty = true;
        return changed;
    }

    switch (list) {
        case ListId::GrandPrix: {
            Props* gp = Section("_UserGrandPrixData");
            if (!gp) break;
            if (Text::StartsWith(id, L"cup:")) {
                Value* cups = Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array);
                const size_t k = static_cast<size_t>(Text::ToInt(id.substr(4)).value_or(-1));
                if (!cups || k >= cups->items.size()) break;
                Value& cup = cups->items[k];
                for (const char* f : { "bOpened", "bLeast1Play" })
                    if (Property* p = Find(cup.props, f)) set(p->value, true);
                for (const char* f : { "Normal", "Mirror" })
                    if (Property* p = Find(cup.props, f); p && p->value.kind == Kind::Array)
                        for (auto& r : p->value.items)
                            if (r.i < kBestRank) {
                                r.i = kBestRank;
                                changed = true;
                            }
            } else if (Text::StartsWith(id, L"event:")) {
                Value* ev = Gvas::Container(*gp, "EventCompleted", Kind::Array);
                const size_t k = static_cast<size_t>(Text::ToInt(id.substr(6)).value_or(-1));
                if (ev && k < ev->items.size()) set(ev->items[k], true);
            } else if (Property* p = Find(*gp, Text::Narrow(id))) {
                set(p->value, true);
            }
            break;
        }
        case ListId::TimeTrial:
        case ListId::RedStarRings: {
            const bool tt = list == ListId::TimeTrial;
            Props* sec = Section(tt ? "_UserTimeTrialData" : "_UserCommonData");
            Value* arr = sec ? Gvas::Container(*sec, tt ? "TimeTrialCourseDataArray" : "RedStarRingAcquisitionPerStage", Kind::Array) : nullptr;
            const size_t k = static_cast<size_t>(Text::ToInt(id).value_or(-1));
            if (!arr || k >= arr->items.size()) break;
            if (Property* flags = Find(arr->items[k].props, tt ? "Acquistion" : "Acquisition"); flags && flags->value.kind == Kind::Array)
                for (auto& b : flags->value.items) set(b, true);
            break;
        }
        case ListId::Tutorials: {
            if (id == L"hints") {
                if (Props* hints = Section("_UserHintData"))
                    if (Value* a = Gvas::Container(*hints, "UserHintArray", Kind::Array))
                        for (auto& h : a->items)
                            if (Property* p = Find(h.props, "bIsRead")) set(p->value, true);
                break;
            }
            Props* sec = Section("_UserTutorialData");
            if (Value* m = sec ? Gvas::Container(*sec, "TutorialCompletedFlags", Kind::Map) : nullptr)
                for (auto& [key, val] : m->entries)
                    if (Text::EndsWith(key.s, L"::" + id)) set(val, true);
            break;
        }
        default: break;
    }
    if (changed) dirty = true;
    return changed;
}

bool AppSave::Lock(ListId list, const std::wstring& id) {
    bool changed = false;
    const auto set = [&](Value& v, bool on) {
        if (v.b != on) {
            v.b = on;
            changed = true;
        }
    };
    if (IsDlc(list, id)) return false;   // DLC: Steam decides
    if (const MapList* m = MapOf(list)) {
        // The rows say what may not be locked (the selected character, parts in use, DLC...)
        for (const ItemRow& r : Rows(list))
            if (r.id == id && !r.canLock) return false;
        Props* sec = Section(m->section);
        Value* map = sec ? Gvas::Container(*sec, m->field, Kind::Map) : nullptr;
        if (!map) return false;
        Value* e = EntryOf(*map, *m, id);
        const auto remove = [&] {
            const auto it = std::find_if(map->entries.begin(), map->entries.end(), [&](const auto& kv) { return KeyText(*m, kv.first) == id; });
            if (it != map->entries.end()) {
                map->entries.erase(it);
                changed = true;
            }
        };
        switch (m->shape) {
            case Shape::Lock:
                if (e && FieldStr(*e, "LockType") == L"ELockType::None") {
                    SetField(*e, "LockType", Value::Str(L"ELockType::Norm"));
                    changed = true;
                }
                break;
            case Shape::Parts: remove(); break;
            case Shape::Driver:
                if (e)
                    if (Property* p = Find(e->props, "bIsSelectable")) set(p->value, false);
                break;
            case Shape::Honor:
                remove();
                if (Value* a = Gvas::Container(*sec, "UnlockedHonorTitleList", Kind::Array)) {
                    const int64_t n = Text::ToInt(id).value_or(-1);
                    const auto before = a->items.size();
                    a->items.erase(std::remove_if(a->items.begin(), a->items.end(), [n](const Value& v) { return v.i == n; }), a->items.end());
                    changed |= a->items.size() != before;
                }
                break;
            case Shape::Album:
                if (e)
                    for (const char* f : { "bUnlocked", "bAvailable" })
                        if (Property* p = Find(e->props, f)) set(p->value, false);
                break;
            case Shape::Track:
                if (e)
                    if (Property* p = Find(e->props, "bAvailable")) set(p->value, false);
                break;
            case Shape::Challenge:
                // Not done, and its count back to 0
                if (e && FieldStr(*e, "ProgressState") != L"EChallengeProgressState::NotAcquired") {
                    SetField(*e, "ProgressState", Value::Str(L"EChallengeProgressState::NotAcquired"));
                    changed = true;
                }
                if (e && FieldInt(*e, "StatsCount") != 0) {
                    SetField(*e, "StatsCount", Value::Int(0));
                    changed = true;
                }
                break;
        }
        if (changed) dirty = true;
        return changed;
    }

    switch (list) {
        case ListId::GrandPrix: {
            Props* gp = Section("_UserGrandPrixData");
            if (!gp) break;
            if (Text::StartsWith(id, L"cup:")) {
                Value* cups = Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array);
                const size_t k = static_cast<size_t>(Text::ToInt(id.substr(4)).value_or(-1));
                if (cups && k < cups->items.size())
                    if (Property* p = Find(cups->items[k].props, "bOpened")) set(p->value, false);
            } else if (Text::StartsWith(id, L"event:")) {
                Value* ev = Gvas::Container(*gp, "EventCompleted", Kind::Array);
                const size_t k = static_cast<size_t>(Text::ToInt(id.substr(6)).value_or(-1));
                if (ev && k < ev->items.size()) set(ev->items[k], false);
            } else if (Property* p = Find(*gp, Text::Narrow(id))) {
                set(p->value, false);
            }
            break;
        }
        case ListId::TimeTrial:
        case ListId::RedStarRings: {
            const bool tt = list == ListId::TimeTrial;
            Props* sec = Section(tt ? "_UserTimeTrialData" : "_UserCommonData");
            Value* arr = sec ? Gvas::Container(*sec, tt ? "TimeTrialCourseDataArray" : "RedStarRingAcquisitionPerStage", Kind::Array) : nullptr;
            const size_t k = static_cast<size_t>(Text::ToInt(id).value_or(-1));
            if (!arr || k >= arr->items.size()) break;
            if (Property* flags = Find(arr->items[k].props, tt ? "Acquistion" : "Acquisition"); flags && flags->value.kind == Kind::Array)
                for (auto& b : flags->value.items) set(b, false);
            break;
        }
        case ListId::Tutorials: {
            if (id == L"hints") {
                if (Props* hints = Section("_UserHintData"))
                    if (Value* a = Gvas::Container(*hints, "UserHintArray", Kind::Array))
                        for (auto& h : a->items)
                            if (Property* p = Find(h.props, "bIsRead")) set(p->value, false);
                break;
            }
            Props* sec = Section("_UserTutorialData");
            if (Value* m = sec ? Gvas::Container(*sec, "TutorialCompletedFlags", Kind::Map) : nullptr)
                for (auto& [key, val] : m->entries)
                    if (Text::EndsWith(key.s, L"::" + id)) set(val, false);
            break;
        }
        default: break;
    }
    if (changed) dirty = true;
    return changed;
}

int AppSave::UnlockAll(ListId list) {
    int n = 0;
    for (const ItemRow& r : Rows(list))
        if (r.state != ItemState::Unlocked && Unlock(list, r.id)) ++n;
    return n;
}

bool AppSave::SetFlag(ListId list, const std::wstring& id, size_t index, bool on) {
    if (list != ListId::TimeTrial && list != ListId::RedStarRings) return false;
    const bool tt = list == ListId::TimeTrial;
    Props* sec = Section(tt ? "_UserTimeTrialData" : "_UserCommonData");
    Value* arr = sec ? Gvas::Container(*sec, tt ? "TimeTrialCourseDataArray" : "RedStarRingAcquisitionPerStage", Kind::Array) : nullptr;
    const size_t k = static_cast<size_t>(Text::ToInt(id).value_or(-1));
    if (!arr || k >= arr->items.size()) return false;
    Property* flags = Find(arr->items[k].props, tt ? "Acquistion" : "Acquisition");
    if (!flags || flags->value.kind != Kind::Array || index >= flags->value.items.size()) return false;
    Value& b = flags->value.items[index];
    if (b.b == on) return false;
    b.b = on;
    dirty = true;
    return true;
}

// ── Courses ─────────────────────────────────────────────────────────────────────────────────────

std::vector<Course> AppSave::Courses() const {
    auto& self = const_cast<AppSave&>(*this);
    Props* common = self.Section("_UserCommonData");
    Props* tt = self.Section("_UserTimeTrialData");
    Props* gp = self.Section("_UserGrandPrixData");
    Value* rings = common ? Gvas::Container(*common, "RedStarRingAcquisitionPerStage", Kind::Array) : nullptr;
    Value* trials = tt ? Gvas::Container(*tt, "TimeTrialCourseDataArray", Kind::Array) : nullptr;
    Value* cups = gp ? Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array) : nullptr;
    std::vector<Course> out;
    for (const auto& c : Data::StageCatalog::Courses()) {
        const wchar_t* name = GameName("stage:" + Text::Narrow(c.id));
        if (!name) continue;   // not in the game yet
        Course course;
        course.id = c.id;
        course.index = Data::StageCatalog::StageIndex(c.id);
        course.name = name;
        const size_t k = static_cast<size_t>(course.index);
        if (rings && k < rings->items.size())
            if (const Property* f = Find(rings->items[k].props, "Acquisition"); f && f->value.kind == Kind::Array)
                for (const auto& b : f->value.items) course.rings.push_back(b.b);
        if (trials && k < trials->items.size()) {
            const Value& e = trials->items[k];
            const Property* got = Find(e.props, "Acquistion");
            const Property* races = Find(e.props, "RaceData");
            for (size_t t = 0; t < 2; ++t) {
                TrialRecord& r = course.trials[t];
                if (got && got->value.kind == Kind::Array && t < got->value.items.size()) r.reward = got->value.items[t].b;
                if (!races || races->value.kind != Kind::Array || t >= races->value.items.size()) continue;
                const Value& race = races->value.items[t];
                r.time = FieldInt(race, "TotalLapTime");
                r.valid = FieldBool(race, "bIsValidData") && r.time > 0;
                r.laps[0] = FieldInt(race, "FirstLapTime");
                r.laps[1] = FieldInt(race, "SecondLapTime");
                r.laps[2] = FieldInt(race, "ThirdLapTime");
                r.rank = AfterColons(FieldStr(race, "ClearRankId"));
                r.date = FieldStr(race, "DateTime");
                r.driver = AfterColons(FieldStr(race, "DirverId"));   // sic
            }
        }
        if (c.cup >= 0 && cups && static_cast<size_t>(c.cup) < cups->items.size()) {
            const Value& cup = cups->items[static_cast<size_t>(c.cup)];
            course.cup = c.cup;
            const wchar_t* cupName = GameName("cup:" + std::to_string(c.cup));
            course.cupName = cupName ? cupName : Loc::Fmt("savefile_cup", std::to_wstring(c.cup));
            course.cupOpen = FieldBool(cup, "bOpened");
            if (const Property* n = Find(cup.props, "Normal"); n && n->value.kind == Kind::Array)
                for (size_t s = 0; s < n->value.items.size() && s < 4; ++s) course.cupRank[s] = static_cast<int>(n->value.items[s].i);
            if (const Property* m = Find(cup.props, "Mirror"); m && m->value.kind == Kind::Array && !m->value.items.empty())
                course.cupRank[4] = static_cast<int>(m->value.items[0].i);
        }
        out.push_back(std::move(course));
    }
    return out;
}

std::vector<MachineBuild> AppSave::Garage() const {
    std::vector<MachineBuild> out;
    const Props* sec = Section("_UserMachineAssemblyData");
    const Property* custom = sec ? Find(*sec, "CustomizeDataList") : nullptr;
    if (!custom || custom->value.kind != Kind::Map) return out;
    std::wstring selected;
    if (const Props* common = Section("_UserCommonData"))
        if (const Property* p = Find(*common, "SelectedMachineId"); p && p->value.kind == Kind::Str) selected = AfterColons(p->value.s);
    for (const auto& [key, v] : custom->value.entries) {
        if (key.kind != Kind::Str) continue;
        MachineBuild b;
        b.slot = AfterColons(key.s);
        b.used = FieldBool(v, "bIsUsed");
        b.selected = b.slot == selected;
        b.front = AfterColons(FieldStr(v, "FrontPartsId"));
        b.rear = AfterColons(FieldStr(v, "RearPartsId"));
        b.tire = AfterColons(FieldStr(v, "TirePartsId"));
        if (FieldStr(v, "ColorMode") == L"EMachineColorMode::Preset") {
            b.colorPreset = AfterColons(FieldStr(v, "ColorPresetId"));
            b.colorName = RowName(ListId::ColorPresets, b.colorPreset);
        }
        b.horn = AfterColons(FieldStr(v, "HornType"));
        b.aura = AfterColons(FieldStr(v, "AuraId"));
        if (const Property* sticker = v.kind == Kind::Struct ? Find(v.props, "Sticker") : nullptr; sticker && sticker->value.kind == Kind::Struct)
            for (const char* side : { "FrontStickerArray", "RearStickerArray" })
                if (const Property* a = Find(sticker->value.props, side); a && a->value.kind == Kind::Array)
                    for (const Value& s : a->value.items) b.stickers += FieldInt(s, "stickerId") >= 0;
        out.push_back(std::move(b));
    }
    std::stable_sort(out.begin(), out.end(), [](const MachineBuild& a, const MachineBuild& b) {
        return std::make_pair(!a.selected, !a.used) < std::make_pair(!b.selected, !b.used);
    });
    return out;
}

Value* AppSave::BuildEntry(const std::wstring& slot) {
    Props* sec = Section("_UserMachineAssemblyData");
    Value* custom = sec ? Gvas::Container(*sec, "CustomizeDataList", Kind::Map) : nullptr;
    if (!custom) return nullptr;
    for (auto& [key, v] : custom->entries)
        if (key.kind == Kind::Str && AfterColons(key.s) == slot && v.kind == Kind::Struct) return &v;
    return nullptr;
}

bool AppSave::SetBuild(const std::wstring& slot, BuildPart part, const std::wstring& value) {
    Value* e = BuildEntry(slot);
    if (!e || !FieldBool(*e, "bIsUsed") || value.empty()) return false;
    static const struct {
        const char* field;
        const wchar_t* prefix;
    } kFields[] = { { "FrontPartsId", L"EMachineId::" },  { "RearPartsId", L"EMachineId::" },          { "TirePartsId", L"EMachineId::" },
                    { "ColorPresetId", L"EMachineColorPresetId::" }, { "HornType", L"EMachineHornType::" }, { "AuraId", L"EMachineAuraId::" } };
    const auto& f = kFields[static_cast<int>(part)];
    Property* p = Find(e->props, f.field);
    if (!p || p->value.kind != Kind::Str) return false;
    const std::wstring full = f.prefix + value;
    bool changed = p->value.s != full;
    p->value.s = full;
    if (part == BuildPart::Color) {
        // A preset's colours go into the machine too: the body's, tyres' and cockpit's (colour, surface) x 3
        // and the body's and tyres' glow, as the game writes them
        const int number = static_cast<int>(Text::ToInt(value.substr(std::min<size_t>(value.size(), 11))).value_or(-1));
        const Data::ColorCatalog::ColorPreset* preset = nullptr;
        for (const auto& c : Data::ColorCatalog::Presets())
            if (c.id == number) preset = &c;
        if (!preset) return false;
        static const char* kAreas[3][7] = {
            { "BodyColorArea1", "BodySurfaceArea1", "BodyColorArea2", "BodySurfaceArea2", "BodyColorArea3", "BodySurfaceArea3", "BodyGlowColor" },
            { "TireColorArea1", "TireSurfaceArea1", "TireColorArea2", "TireSurfaceArea2", "TireColorArea3", "TireSurfaceArea3", "TireGlowColor" },
            { "CockpitColorArea1", "CockpitSurfaceArea1", "CockpitColorArea2", "CockpitSurfaceArea2", "CockpitColorArea3", "CockpitSurfaceArea3", nullptr },
        };
        for (int g = 0; g < 3; ++g)
            for (int k = 0; k < 7; ++k) {
                if (!kAreas[g][k]) continue;
                Property* area = Find(e->props, kAreas[g][k]);
                if (!area || area->value.kind != Kind::Int) continue;
                changed |= area->value.i != preset->values[g * 7 + k];
                area->value.i = preset->values[g * 7 + k];
            }
        if (Property* mode = Find(e->props, "ColorMode"); mode && mode->value.kind == Kind::Str && mode->value.s != L"EMachineColorMode::Preset") {
            mode->value.s = L"EMachineColorMode::Preset";
            changed = true;
        }
    }
    dirty |= changed;
    return changed;
}

std::wstring AppSave::NewBuild(const std::wstring& type) {
    for (int n = 101; n <= 105; ++n) {
        const std::wstring slot = type + L"_" + std::to_wstring(n);
        Value* e = BuildEntry(slot);
        if (!e || FieldBool(*e, "bIsUsed")) continue;
        if (Property* used = Find(e->props, "bIsUsed")) used->value = Value::Bool(true);
        else continue;
        // The type's first machine (the Boost machines float: their tyres slot keeps the game's SPD_01)
        const std::wstring first = type + L"_01";
        SetBuild(slot, BuildPart::Front, first);
        SetBuild(slot, BuildPart::Rear, first);
        SetBuild(slot, BuildPart::Tire, type == L"DSH" ? L"SPD_01" : first);
        dirty = true;
        return slot;
    }
    return {};
}

bool AppSave::ClearBuild(const std::wstring& slot) {
    Value* e = BuildEntry(slot);
    if (!e || !FieldBool(*e, "bIsUsed")) return false;
    for (const MachineBuild& b : Garage())
        if (b.slot == slot && b.selected) return false;
    Property* used = Find(e->props, "bIsUsed");
    if (!used) return false;
    used->value = Value::Bool(false);
    dirty = true;
    return true;
}

// ── Gadget presets ──────────────────────────────────────────────────────────────────────────────

Value* AppSave::GadgetList(int player, int preset) {
    Props* sec = Section("_UserMachineAssemblyData");
    Value* players = sec ? Gvas::Container(*sec, "GadgetPresetDataLists", Kind::Array) : nullptr;
    if (!players || player < 0 || player >= static_cast<int>(players->items.size())) return nullptr;
    Value& lists = players->items[static_cast<size_t>(player)];
    if (lists.kind != Kind::Struct) return nullptr;
    Value* presets = Gvas::Container(lists.props, "list", Kind::Array);
    if (!presets || preset < 0 || preset >= static_cast<int>(presets->items.size())) return nullptr;
    Value& p = presets->items[static_cast<size_t>(preset)];
    return p.kind == Kind::Struct ? Gvas::Container(p.props, "GadgetDataList", Kind::Array) : nullptr;
}

Value* AppSave::SpecialList(int player, int preset) {
    Props* sec = Section("_UserMachineAssemblyData");
    Value* players = sec ? Gvas::Container(*sec, "SpGadgetPresetDataLists", Kind::Array) : nullptr;
    if (!players || player < 0 || player >= static_cast<int>(players->items.size())) return nullptr;
    Value& lists = players->items[static_cast<size_t>(player)];
    if (lists.kind != Kind::Struct) return nullptr;
    Value* presets = Gvas::Container(lists.props, "Presets", Kind::Array);
    if (!presets || preset < 0 || preset >= static_cast<int>(presets->items.size())) return nullptr;
    Value& p = presets->items[static_cast<size_t>(preset)];
    return p.kind == Kind::Struct ? Gvas::Container(p.props, "EquippedSpGadgets", Kind::Array) : nullptr;
}

bool AppSave::SpecialSlotOpen() const {
    const Props* sec = Section("_UserMachineAssemblyData");
    const Property* p = sec ? Find(*sec, "GadgetSlotNum") : nullptr;
    return p && p->value.kind == Kind::Int && p->value.i >= kGadgetSlotsAll;
}

bool AppSave::SetSpecialGadget(int player, int preset, const std::wstring& id) {
    Value* list = SpecialList(player, preset);
    if (!list) return false;
    std::wstring now;
    for (const Value& g : list->items)
        if (const std::wstring on = AfterColons(FieldStr(g, "GadgetID")); !on.empty() && on != L"NonGadget") now = on;
    if (id == now) return false;
    if (id.empty()) {
        list->items.clear();
        dirty = true;
        return true;
    }
    // One the slot takes, and not on the preset's plate already
    if (!Data::GadgetCatalog::IsSpecial(id)) return false;
    if (const Value* plate = GadgetList(player, preset))
        for (const Value& g : plate->items)
            if (AfterColons(FieldStr(g, "GadgetID")) == id) return false;
    // A UserGadgetData like the game's: a copy of one it wrote (any player's, any preset's), else built
    const Value* like = list->items.empty() ? nullptr : &list->items.front();
    for (int pl = 0; !like && pl < kGadgetPlayers; ++pl)
        for (int pr = 0; !like && pr < kGadgetPresets; ++pr)
            if (const Value* other = SpecialList(pl, pr); other && !other->items.empty()) like = &other->items.front();
    Property slotIndex;
    slotIndex.name = "SlotIndex";
    slotIndex.type = Simple("IntProperty");
    slotIndex.value = Value::Int(kGadgetSpecialSlot);
    Value entry = NewStruct(like, { EnumProp("GadgetID", "EGadgetId", L"EGadgetId::" + id), std::move(slotIndex) });
    list->items.clear();
    list->items.push_back(std::move(entry));
    dirty = true;
    return true;
}

int AppSave::GadgetSlotCount() const {
    const Props* sec = Section("_UserMachineAssemblyData");
    const Property* p = sec ? Find(*sec, "GadgetSlotNum") : nullptr;
    return p && p->value.kind == Kind::Int ? static_cast<int>(std::clamp<int64_t>(p->value.i, 1, kGadgetPlate)) : kGadgetPlate;
}

std::vector<GadgetPreset> AppSave::GadgetPresets(int player) const {
    std::vector<GadgetPreset> out;
    auto& self = const_cast<AppSave&>(*this);
    int selected = -1;
    if (Props* common = self.Section("_UserCommonData"))
        if (Value* ids = Gvas::Container(*common, "SelectedGadgetPresetIds", Kind::Array))
            if (player >= 0 && player < static_cast<int>(ids->items.size()) && ids->items[static_cast<size_t>(player)].kind == Kind::Int)
                selected = static_cast<int>(ids->items[static_cast<size_t>(player)].i);
    for (int k = 0; k < kGadgetPresets; ++k) {
        const Value* list = self.GadgetList(player, k);
        if (!list) break;
        GadgetPreset preset;
        preset.index = k;
        preset.selected = k == selected;
        for (const Value& g : list->items) {
            const std::wstring id = AfterColons(FieldStr(g, "gadgetId"));
            if (id.empty() || id == L"NonGadget") continue;
            preset.gadgets.push_back({ id, static_cast<int>(FieldInt(g, "SlotIndex")), Data::GadgetCatalog::Slots(id) });
        }
        std::sort(preset.gadgets.begin(), preset.gadgets.end(), [](const auto& a, const auto& b) { return a.slot < b.slot; });
        if (const Value* sp = self.SpecialList(player, k))
            for (const Value& g : sp->items)
                if (const std::wstring id = AfterColons(FieldStr(g, "GadgetID")); !id.empty() && id != L"NonGadget") preset.special = id;
        out.push_back(std::move(preset));
    }
    return out;
}

bool AppSave::PlaceGadget(int player, int preset, const std::wstring& id, int slot) {
    Value* list = GadgetList(player, preset);
    if (!list || id.empty() || id == L"NonGadget") return false;
    if (const Value* sp = SpecialList(player, preset))
        for (const Value& g : sp->items)
            if (AfterColons(FieldStr(g, "GadgetID")) == id) return false;
    const int span = Data::GadgetCatalog::Slots(id);
    if (slot < 0 || slot + span > GadgetSlotCount()) return false;
    // Free slots, and not on the plate already
    bool used[kGadgetPlate] = {};
    for (const Value& g : list->items) {
        const std::wstring on = AfterColons(FieldStr(g, "gadgetId"));
        if (on == id) return false;
        const int at = static_cast<int>(FieldInt(g, "SlotIndex"));
        for (int s = at; s < at + Data::GadgetCatalog::Slots(on) && s < kGadgetPlate; ++s)
            if (s >= 0) used[s] = true;
    }
    for (int s = slot; s < slot + span; ++s)
        if (used[s]) return false;
    // A UserGadgetData like the game's (a copy of one it wrote when there is one), kept in slot order
    const Value* like = list->items.empty() ? nullptr : &list->items.front();
    Property slotIndex;
    slotIndex.name = "SlotIndex";
    slotIndex.type = Simple("IntProperty");
    slotIndex.value = Value::Int(slot);
    Value entry = NewStruct(like, { EnumProp("GadgetID", "EGadgetId", L"EGadgetId::" + id), std::move(slotIndex) });
    auto at = std::find_if(list->items.begin(), list->items.end(), [&](const Value& g) { return FieldInt(g, "SlotIndex") > slot; });
    list->items.insert(at, std::move(entry));
    dirty = true;
    return true;
}

bool AppSave::RemoveGadget(int player, int preset, int slot) {
    Value* list = GadgetList(player, preset);
    if (!list) return false;
    for (auto it = list->items.begin(); it != list->items.end(); ++it) {
        const int at = static_cast<int>(FieldInt(*it, "SlotIndex"));
        if (slot >= at && slot < at + Data::GadgetCatalog::Slots(AfterColons(FieldStr(*it, "gadgetId")))) {
            list->items.erase(it);
            dirty = true;
            return true;
        }
    }
    return false;
}

bool AppSave::ClearGadgetPreset(int player, int preset) {
    // Its plate and its special slot
    bool changed = false;
    for (Value* list : { GadgetList(player, preset), SpecialList(player, preset) })
        if (list && !list->items.empty()) {
            list->items.clear();
            changed = true;
        }
    if (changed) dirty = true;
    return changed;
}

bool AppSave::SelectGadgetPreset(int player, int preset) {
    Props* common = Section("_UserCommonData");
    Value* ids = common ? Gvas::Container(*common, "SelectedGadgetPresetIds", Kind::Array) : nullptr;
    if (!ids || player < 0 || player >= static_cast<int>(ids->items.size()) || preset < 0 || preset >= kGadgetPresets) return false;
    Value& v = ids->items[static_cast<size_t>(player)];
    if (v.kind != Kind::Int || v.i == preset) return false;
    v.i = preset;
    dirty = true;
    return true;
}

bool AppSave::SetSelectedMachine(const std::wstring& slot) {
    Props* common = Section("_UserCommonData");
    Property* p = common ? Find(*common, "SelectedMachineId") : nullptr;
    if (!p || p->value.kind != Kind::Str || AfterColons(p->value.s) == slot) return false;
    bool built = false;
    for (const MachineBuild& b : Garage()) built |= b.slot == slot && b.used;
    if (!built) return false;
    p->value.s = L"EMachineId::" + slot;
    dirty = true;
    return true;
}

int AppSave::SelectedTitle() const {
    const Props* sec = Section("_UserHonorTitleData");
    const Property* p = sec ? Find(*sec, "HonorTitleIndex") : nullptr;
    return p && p->value.kind == Kind::Int ? static_cast<int>(p->value.i) : -1;
}

bool AppSave::SetSelectedTitle(int id) {
    Props* sec = Section("_UserHonorTitleData");
    Property* p = sec ? Find(*sec, "HonorTitleIndex") : nullptr;
    if (!p || p->value.kind != Kind::Int || p->value.i == id) return false;
    const std::wstring key = std::to_wstring(id);
    bool unlocked = false;
    for (const ItemRow& r : Rows(ListId::HonorTitles)) unlocked |= r.id == key && r.state == ItemState::Unlocked;
    if (!unlocked) return false;
    p->value.i = id;
    dirty = true;
    return true;
}

bool AppSave::SetRing(const Course& course, size_t ring, bool on) { return SetFlag(ListId::RedStarRings, std::to_wstring(course.index), ring, on); }

bool AppSave::SetTrialReward(const Course& course, size_t trial, bool on) {
    return SetFlag(ListId::TimeTrial, std::to_wstring(course.index), trial, on);
}

Value* AppSave::TrialRace(int courseIndex, size_t trial) {
    Props* tt = Section("_UserTimeTrialData");
    Value* arr = tt ? Gvas::Container(*tt, "TimeTrialCourseDataArray", Kind::Array) : nullptr;
    if (!arr || courseIndex < 0 || static_cast<size_t>(courseIndex) >= arr->items.size()) return nullptr;
    Property* races = Find(arr->items[static_cast<size_t>(courseIndex)].props, "RaceData");
    if (!races || races->value.kind != Kind::Array || trial >= races->value.items.size()) return nullptr;
    return &races->value.items[trial];
}

bool AppSave::SetTrialLap(int courseIndex, size_t trial, size_t lap, int64_t time) {
    static const char* const kLaps[3] = { "FirstLapTime", "SecondLapTime", "ThirdLapTime" };
    Value* race = TrialRace(courseIndex, trial);
    // Only a record the game wrote: one that never was has no ghost to go with it
    if (!race || lap >= 3 || time <= 0 || !FieldBool(*race, "bIsValidData")) return false;
    Property* p = Find(race->props, kLaps[lap]);
    if (!p || p->value.kind != Kind::Int || p->value.i == time) return false;
    p->value.i = time;
    int64_t total = 0;
    for (const char* f : kLaps) total += FieldInt(*race, f);
    if (Property* t = Find(race->props, "TotalLapTime"); t && t->value.kind == Kind::Int) t->value.i = total;
    dirty = true;
    return true;
}

bool AppSave::SetTrialRank(int courseIndex, size_t trial, const std::wstring& rank) {
    Value* race = TrialRace(courseIndex, trial);
    if (!race || !FieldBool(*race, "bIsValidData")) return false;
    const std::wstring value = L"ETimeTrialClearRankId::" + rank;
    if (FieldStr(*race, "ClearRankId") == value) return false;
    SetField(*race, "ClearRankId", Value::Str(value));
    dirty = true;
    return true;
}

bool AppSave::SetCupOpen(int cup, bool open) {
    Props* gp = Section("_UserGrandPrixData");
    Value* cups = gp ? Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array) : nullptr;
    if (!cups || cup < 0 || static_cast<size_t>(cup) >= cups->items.size()) return false;
    Property* p = Find(cups->items[static_cast<size_t>(cup)].props, "bOpened");
    if (!p || p->value.b == open) return false;
    p->value.b = open;
    dirty = true;
    return true;
}

bool AppSave::SetCupRank(int cup, int speed, int rank) {
    Props* gp = Section("_UserGrandPrixData");
    Value* cups = gp ? Gvas::Container(*gp, "GrandPrixCompletionStatus", Kind::Array) : nullptr;
    if (!cups || cup < 0 || static_cast<size_t>(cup) >= cups->items.size() || speed < 0 || speed >= kSpeedCount) return false;
    Value& c = cups->items[static_cast<size_t>(cup)];
    Property* ranks = Find(c.props, speed < 4 ? "Normal" : "Mirror");
    const size_t at = speed < 4 ? static_cast<size_t>(speed) : 0;
    if (!ranks || ranks->value.kind != Kind::Array || at >= ranks->value.items.size()) return false;
    Value& r = ranks->value.items[at];
    const int64_t v = std::clamp<int64_t>(rank, 0, kCupWon);
    if (r.i == v) return false;
    r.i = v;
    // A Grand Prix with a result is open and has been played
    if (v > 0)
        for (const char* f : { "bOpened", "bLeast1Play" })
            if (Property* p = Find(c.props, f)) p->value.b = true;
    dirty = true;
    return true;
}

int AppSave::ResetChallenges() {
    Props* sec = Section("_UserChallengeData");
    Value* map = sec ? Gvas::Container(*sec, "ChallengeStatsList", Kind::Map) : nullptr;
    if (!map) return 0;
    int n = 0;
    for (auto& [key, e] : map->entries) {
        bool changed = false;
        if (FieldStr(e, "ProgressState") != L"EChallengeProgressState::NotAcquired") {
            SetField(e, "ProgressState", Value::Str(L"EChallengeProgressState::NotAcquired"));
            changed = true;
        }
        if (FieldInt(e, "StatsCount") != 0) {
            SetField(e, "StatsCount", Value::Int(0));
            changed = true;
        }
        n += changed;
    }
    if (n) dirty = true;
    return n;
}

// ── Statistics ──────────────────────────────────────────────────────────────────────────────────

std::vector<NumberField> AppSave::StatNumbers() const {
    std::vector<NumberField> out;
    const Props* stats = Section("_StatsData");
    const Property* details = stats ? Gvas::Find(*stats, "RaceDetails") : nullptr;
    if (!details || details->value.kind != Kind::Struct) return out;
    // In the save's order (the game's: distance, time, races, wins, then what you did in them)
    for (const auto& p : details->value.props)
        if (p.value.kind == Kind::Int) out.push_back({ "stat_" + p.name, p.value.i, 0, INT32_MAX, 0, false });
    return out;
}

std::vector<StatUsage> AppSave::Usage(bool machines) const {
    std::vector<StatUsage> out;
    Props* stats = const_cast<AppSave*>(this)->Section("_StatsData");
    const Value* map = stats ? Gvas::Container(*stats, machines ? "MachineList" : "DriverList", Kind::Map) : nullptr;
    if (!map) return out;
    const std::wstring prefix = machines ? L"EMachineId::" : L"EDriverId::";
    for (const auto& [key, e] : map->entries) {
        if (key.kind != Kind::Str) continue;
        StatUsage u;
        u.id = key.s.rfind(prefix, 0) == 0 ? key.s.substr(prefix.size()) : key.s;
        u.used = FieldInt(e, "UsedCount");
        u.hasWins = e.kind == Kind::Struct && Gvas::Find(e.props, "winCount");
        u.wins = FieldInt(e, "winCount");
        out.push_back(std::move(u));
    }
    // The most raced with first
    std::stable_sort(out.begin(), out.end(), [](const StatUsage& a, const StatUsage& b) { return a.used > b.used; });
    return out;
}

bool AppSave::SetUsage(bool machines, const std::wstring& id, bool wins, int64_t value) {
    Props* stats = Section("_StatsData");
    Value* map = stats ? Gvas::Container(*stats, machines ? "MachineList" : "DriverList", Kind::Map) : nullptr;
    if (!map) return false;
    const std::wstring key = (machines ? L"EMachineId::" : L"EDriverId::") + id;
    for (auto& [k, e] : map->entries) {
        if (k.kind != Kind::Str || k.s != key || e.kind != Kind::Struct) continue;
        Property* p = Gvas::Find(e.props, wins ? "winCount" : "UsedCount");
        if (!p || p->value.kind != Kind::Int) return false;
        // Wins never above races
        int64_t v = std::clamp<int64_t>(value, 0, INT32_MAX);
        if (wins) v = std::min(v, FieldInt(e, "UsedCount"));
        bool changed = p->value.i != v;
        p->value.i = v;
        if (!wins)
            if (Property* w = Gvas::Find(e.props, "winCount"); w && w->value.kind == Kind::Int && w->value.i > v) {
                w->value.i = v;
                changed = true;
            }
        if (changed) dirty = true;
        return changed;
    }
    return false;
}

// ── Play Missions ───────────────────────────────────────────────────────────────────────────────

namespace {
constexpr const wchar_t* kMissionStates[] = { L"EMissionProgressState::NotStarted", L"EMissionProgressState::InProgress",
                                              L"EMissionProgressState::Cleared" };
constexpr const wchar_t* kClaimed = L"EMissionRewardState::Claimed";
constexpr const wchar_t* kNotClaimed = L"EMissionRewardState::NotClaimed";
// DateTime (100 ns since year 1) as a FILETIME (100 ns since 1601)
constexpr int64_t kDateTimeToFileTime = 504911232000000000LL;

int64_t DateTimeOf(const Value& period, const char* name) {
    if (period.kind != Kind::Struct) return 0;
    const Property* p = Find(period.props, name);
    if (!p || p->value.kind != Kind::Raw || p->value.raw.size() != 8) return 0;
    int64_t ticks = 0;
    memcpy(&ticks, p->value.raw.data(), 8);
    return ticks > kDateTimeToFileTime ? ticks - kDateTimeToFileTime : 0;
}

Value* MissionProgress(Value& detail) {
    if (detail.kind != Kind::Struct) return nullptr;
    Property* p = Find(detail.props, "MissionProgress");
    return p && p->value.kind == Kind::Struct ? &p->value : nullptr;
}
} // namespace

std::vector<Mission> AppSave::Missions() const {
    std::vector<Mission> out;
    auto& root = const_cast<AppSave*>(this)->file.root;
    Property* data = Gvas::Find(root, "_UserPlayMissionData");
    if (!data || data->value.kind != Kind::Array) return out;
    for (size_t pi = 0; pi < data->value.items.size(); ++pi) {
        Value& period = data->value.items[pi];
        if (period.kind != Kind::Struct) continue;
        const int64_t ends = DateTimeOf(period, "MissionEndTime");
        Value* details = Gvas::Container(period.props, "MissionDetails", Kind::Array);
        if (!details) continue;
        for (size_t mi = 0; mi < details->items.size(); ++mi) {
            Value& d = details->items[mi];
            if (d.kind != Kind::Struct) continue;
            Mission m;
            m.period = static_cast<int>(pi);
            m.index = static_cast<int>(mi);
            m.id = static_cast<int>(FieldInt(d, "MissionId"));
            m.type = static_cast<int>(FieldInt(d, "MissionTypeId"));
            if (const Property* par = Find(d.props, "MissionParameter"); par && par->value.kind == Kind::Struct) {
                m.params[0] = FieldInt(par->value, "Parameter_A");
                m.params[1] = FieldInt(par->value, "Parameter_B");
                m.params[2] = FieldInt(par->value, "Parameter_C");
            }
            m.goal = Data::MissionCatalog::Goal(m.type, m.params);
            if (const Value* pr = MissionProgress(d)) {
                m.progress = FieldInt(*pr, "ProgressCount");
                const std::wstring st = FieldStr(*pr, "ProgressState");
                m.state = st == kMissionStates[2] ? MissionState::Cleared : st == kMissionStates[1] ? MissionState::InProgress : MissionState::NotStarted;
                m.claimed = FieldStr(*pr, "RewardState") == kClaimed;
            }
            m.rewardType = static_cast<int>(FieldInt(d, "ItemCategoryId"));
            m.rewardCount = FieldInt(d, "ItemCount");
            m.endsAt = ends;
            out.push_back(m);
        }
    }
    return out;
}

bool AppSave::SetMissionProgress(int period, int index, int64_t progress) {
    Property* data = Gvas::Find(file.root, "_UserPlayMissionData");
    if (!data || data->value.kind != Kind::Array || period < 0 || static_cast<size_t>(period) >= data->value.items.size()) return false;
    Value& p = data->value.items[static_cast<size_t>(period)];
    Value* details = p.kind == Kind::Struct ? Gvas::Container(p.props, "MissionDetails", Kind::Array) : nullptr;
    if (!details || index < 0 || static_cast<size_t>(index) >= details->items.size()) return false;
    Value& d = details->items[static_cast<size_t>(index)];
    Value* pr = MissionProgress(d);
    if (!pr) return false;
    int64_t params[3] = {};
    if (const Property* par = Find(d.props, "MissionParameter"); par && par->value.kind == Kind::Struct) {
        params[0] = FieldInt(par->value, "Parameter_A");
        params[1] = FieldInt(par->value, "Parameter_B");
        params[2] = FieldInt(par->value, "Parameter_C");
    }
    const int64_t goal = Data::MissionCatalog::Goal(static_cast<int>(FieldInt(d, "MissionTypeId")), params);
    const int64_t v = std::clamp<int64_t>(progress, 0, goal);
    const int state = v >= goal ? 2 : v > 0 ? 1 : 0;
    bool changed = false;
    if (Property* c = Find(pr->props, "ProgressCount"); c && c->value.kind == Kind::Int && c->value.i != v) {
        c->value.i = v;
        changed = true;
    }
    if (FieldStr(*pr, "ProgressState") != kMissionStates[state]) {
        SetField(*pr, "ProgressState", Value::Str(kMissionStates[state]));
        changed = true;
    }
    // Not done any more: its reward is to receive again when it is
    if (state != 2 && FieldStr(*pr, "RewardState") == kClaimed) {
        SetField(*pr, "RewardState", Value::Str(kNotClaimed));
        changed = true;
    }
    if (changed) dirty = true;
    return changed;
}

int AppSave::CompleteMissions() {
    int n = 0;
    for (const auto& m : Missions())
        if (m.state != MissionState::Cleared) n += SetMissionProgress(m.period, m.index, m.goal);
    return n;
}

int AppSave::UnlockEverything() {
    int n = 0;
    for (int k = 0; k < static_cast<int>(ListId::Count); ++k) n += UnlockAll(static_cast<ListId>(k));
    return n;
}

} // namespace SonicRacingSaveManager::Services::SaveFile

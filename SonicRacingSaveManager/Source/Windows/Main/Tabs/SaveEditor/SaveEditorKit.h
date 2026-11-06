#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include "Graphics/Color.h"
#include "Graphics/Graphics.h"
#include "Graphics/Images.h"
#include "Services/SaveFile/AppSave.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

// The Save Editor's look, shared by its files: calm cards, progress rings and counters that fill as
// they appear, pill chips, a colour per group of categories and per state, and the "burst" an item
// plays when it gets unlocked.
namespace SonicRacingSaveManager::Main::SaveEditorKit {

// ── Sizes ───────────────────────────────────────────────────────────────────────────────────────
inline constexpr float kGap = 14.f, kRadius = 16.f;
inline constexpr float kHeaderH = 78.f, kBarH = 58.f;
inline constexpr float kCardW = 250.f, kCardH = 112.f;           // overview category cards
inline constexpr float kTileW = 150.f, kTileH = 182.f;           // item tiles (categories with pictures)
inline constexpr float kRowH = 54.f, kStatH = 132.f, kAccountH = 92.f;
inline constexpr float kSectionsH = 46.f;                        // Characters / Courses
inline constexpr float kCourseW = 220.f, kCourseH = 200.f;       // course tiles

// ── Colours ─────────────────────────────────────────────────────────────────────────────────────
inline constexpr Color kGreen = Rgba(34, 197, 94);    // unlocked / saved
inline constexpr Color kAmber = Rgba(245, 158, 11);   // locked / unsaved / needs buying
inline constexpr Color kBlue = Rgba(59, 130, 246);    // missing (can be added)
inline constexpr Color kRing = Rgba(239, 68, 68);     // a Red Star Ring you have
inline constexpr Color kGold = Rgba(234, 179, 8);     // a Time Trial reward you have
// Profile gold, Characters & machines blue, Customisation violet, Progress teal
Color GroupColor(int group);

// The colour, icon and name of an item's state
Color StateColor(Services::SaveFile::ItemState s);
UI::Material::Icon StateIcon(Services::SaveFile::ItemState s);
const char* StateKey(Services::SaveFile::ItemState s);
// Whether a click may change the item, and what it would do (for the tooltip)
bool CanToggle(const Services::SaveFile::ItemRow& row);
std::wstring ActionText(const Services::SaveFile::ItemRow& row);

// ── Pieces ──────────────────────────────────────────────────────────────────────────────────────
// A progress ring: the track and the arc of `value` (0..1)
void Ring(UI::Ui& ui, float cx, float cy, float radius, float width, float value, Color tone);
// A rounded chip with an optional dot, cut with "..." past `maxWidth` (when > 0); returns its width
// (ChipWidth measures)
float Chip(UI::Ui& ui, float x, float cy, const std::wstring& text, Color tone, bool filled = false, bool dot = false, float maxWidth = 0.f);
float ChipWidth(UI::Ui& ui, const std::wstring& text, bool dot = false);
// A number that counts up / down to `target` (from 0 the first time it is drawn)
long long Counter(UI::Ui& ui, UI::Id id, long long target, float seconds = 0.7f);
// A soft card: fill, hairline border, and a lift and glow in `tone` with `hover`
void Card(UI::Ui& ui, const Rect& r, float radius, Color tone, float hover, float alpha = 1.f);

// A Red Star Ring as the game draws it (lit / unlit), `size` wide, bouncing with its burst and
// growing on hover; returns whether it was clicked
bool RingToken(UI::Ui& ui, UI::Id id, float cx, float cy, float size, bool on, float burst, bool enabled = true);
// A Time Trial time as the game shows it, "1:26.305" (the save keeps 1/6000 s)
std::wstring CourseTime(int64_t time);
// "1:26.305", "86.305", "1:26" back to the save's unit; nothing when it isn't a time
std::optional<int64_t> ParseCourseTime(const std::wstring& text);
// A colour per speed class (EMenuSpeedClassId): Normal green, High blue, Sonic indigo, Super Sonic gold, Mirror violet
Color SpeedColor(int speed);

// Items that just changed play a burst: a ring of light that grows and fades, and a little bounce
class Bursts {
public:
    void Fire(const std::wstring& id, double now) { at[id] = now; }
    // 0..1 while it plays, -1 otherwise
    float Progress(const std::wstring& id, double now) const;
    void Clear() { at.clear(); }

private:
    std::unordered_map<std::wstring, double> at;
};
inline constexpr float kBurstSeconds = 0.6f;
// The ring of a burst at progress t around (cx, cy)
void BurstRing(UI::Ui& ui, float cx, float cy, float radius, float t, Color tone);
// The bounce scale of a burst at progress t (1 when idle)
float BurstScale(float t);

// ── Machines ────────────────────────────────────────────────────────────────────────────────────
// The translation key of a slot's / part's machine type ("DSH_102" -> Boost)
const char* MachineTypeKey(const std::wstring& id);
// A garage machine's name: its machine's when all three parts are one machine's, else "Custom"
std::wstring BuildName(const Services::SaveFile::MachineBuild& b);
// Built from parts of more than one machine type (a custom build): parts of another type than its slot's
bool MixedBuild(const Services::SaveFile::MachineBuild& b);
// The type chip of a garage machine: its slot's type, or "Mixed" for a custom build
const std::wstring& BuildTypeLabel(const Services::SaveFile::MachineBuild& b);

// ── The game's pictures ─────────────────────────────────────────────────────────────────────────
// Embedded in the app (Assets\GameImages\<kind>\<id>.png, made from the game's files by
// Tools\ExtractGameImages.py and Tools\PackGameImages.py), unpacked with the other assets
bool HasPictures(Services::SaveFile::ListId list);
// The picture of an item (cached, misses too); nullptr when there is none
ImagePtr Picture(Services::SaveFile::ListId list, const std::wstring& id);
// Where an honor title's plate art is in its picture, as fractions of it (most plates fill the middle 80%
// of their width, transparent around; some the whole of it). The whole picture ({0, 0, 1, 1}) when unknown.
// Measured once per title.
Rect TitleArt(const std::wstring& id);
// A gadget of 2 slots or more as the game draws it on the plate: its icon on the frame as wide as its slots
// (gold for a kit); nullptr for the 1-slot ones (their hexagon is Picture's)
ImagePtr GadgetPlatePicture(const std::wstring& id);
// The shop's badge (a storefront in a gold disc) centred at x, y: on what the game's shop sells
void ShopBadge(UI::Ui& ui, float x, float y);
// A horn's sound from the cache (GameImages\horn\<HORNTYPE_..>.wav); empty when there is none
std::wstring HornSound(const std::wstring& id);
// Plays a .wav, stopping the one playing
void PlayWav(const std::wstring& path);
// Any other picture of the cache: GameImages\<kind>\<id>.png ("stage" STG1001, "stageicon", "speed" 0..4,
// "rsr" on / off)
ImagePtr GameImage(const wchar_t* kind, const std::wstring& id, int size = 256);

} // namespace SonicRacingSaveManager::Main::SaveEditorKit

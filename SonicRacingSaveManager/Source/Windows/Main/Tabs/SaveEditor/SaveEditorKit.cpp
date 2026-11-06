#include "Windows/Main/Tabs/SaveEditor/SaveEditorKit.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Data/SaveNames.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Material/Motion.h"
#include "UI/Material/Theme.h"
#include "Windows/Main/Tabs/TabKit.h"

namespace SonicRacingSaveManager::Main::SaveEditorKit {

namespace M = UI::Material;

Color GroupColor(int group) {
    switch (group) {
        case 0: return Rgba(234, 179, 8);
        case 1: return Rgba(59, 130, 246);
        case 2: return Rgba(168, 85, 247);
        default: return Rgba(20, 184, 166);
    }
}

using Services::SaveFile::ItemState;

Color StateColor(ItemState s) { return s == ItemState::Unlocked ? kGreen : s == ItemState::Locked ? kAmber : kBlue; }
M::Icon StateIcon(ItemState s) { return s == ItemState::Unlocked ? M::Icon::Check : s == ItemState::Locked ? M::Icon::Lock : M::Icon::PlusCircle; }
const char* StateKey(ItemState s) { return s == ItemState::Unlocked ? "saveeditor_state_unlocked" : s == ItemState::Locked ? "saveeditor_state_locked" : "saveeditor_state_missing"; }

bool CanToggle(const Services::SaveFile::ItemRow& row) { return row.state == ItemState::Unlocked ? row.canLock : row.canUnlock; }
std::wstring ActionText(const Services::SaveFile::ItemRow& row) {
    if (row.dlc) return Loc::Get("savefile_dlc_purchase");
    if (!CanToggle(row)) return row.state == ItemState::Unlocked ? Loc::Get("saveeditor_in_use") : row.detail;
    return Loc::Get(row.state == ItemState::Unlocked ? "saveeditor_click_lock" : row.state == ItemState::Locked ? "saveeditor_click_unlock" : "saveeditor_click_add");
}

void Ring(UI::Ui& ui, float cx, float cy, float radius, float width, float value, Color tone) {
    TabKit::Arc(ui.g, cx, cy, radius, 0.f, 360.f, M::P().foreground.Fade(0.08f), width);
    const float v = std::clamp(value, 0.f, 1.f);
    if (v > 0.001f) TabKit::Arc(ui.g, cx, cy, radius, 0.f, 360.f * v, tone, width);
}

namespace {
TextStyle ChipStyle() {
    TextStyle t = TabKit::Style(11.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false, HAlign::Center);
    t.valign = VAlign::Center;
    return t;
}
} // namespace

float ChipWidth(UI::Ui& ui, const std::wstring& text, bool dot) { return std::ceil(ui.g.Measure(text, ChipStyle()).w) + (dot ? 34.f : 22.f); }

float Chip(UI::Ui& ui, float x, float cy, const std::wstring& text, Color tone, bool filled, bool dot, float maxWidth) {
    const float w = maxWidth > 0.f ? std::min(ChipWidth(ui, text, dot), maxWidth) : ChipWidth(ui, text, dot);
    const Rect r{ x, cy - 12.f, w, 24.f };
    const Color fg = filled ? Colors::White : TabKit::Readable(tone);
    ui.g.FillRound(r, 12.f, filled ? tone : tone.Fade(0.14f));
    Rect text_r = r;
    if (dot) {
        ui.g.FillEllipse(r.x + 13.f, r.CY(), 3.5f, 3.5f, filled ? Colors::White : tone);
        text_r = { r.x + 12.f, r.y, r.w - 12.f, r.h };
    }
    TextStyle style = ChipStyle();
    style.ellipsis = true;
    ui.g.Text(text, text_r.Inset(6.f, 0.f), fg, style);
    return w;
}

long long Counter(UI::Ui& ui, UI::Id id, long long target, float seconds) {
    // Eased over `seconds` from the value shown when the target changed (Tween moves at a fixed
    // speed, too slow for big numbers)
    struct Count {
        long long from = 0, to = 0;
        double at = -1.0;
    };
    Count& c = ui.Data<Count>(id);
    if (c.at < 0.0 || c.to != target) {
        const float t0 = c.at < 0.0 ? 1.f : std::clamp(static_cast<float>((ui.Time() - c.at) / seconds), 0.f, 1.f);
        c.from = c.at < 0.0 ? 0 : c.from + static_cast<long long>(std::llround((c.to - c.from) * Anim::EaseOutCubic(t0)));
        c.to = target;
        c.at = ui.Time();
    }
    const float t = std::clamp(static_cast<float>((ui.Time() - c.at) / seconds), 0.f, 1.f);
    if (t < 1.f) ui.KeepAnimating();
    return c.from + std::llround((c.to - c.from) * Anim::EaseOutCubic(t));
}

void Card(UI::Ui& ui, const Rect& r, float radius, Color tone, float hover, float alpha) {
    const M::Palette& p = M::P();
    if (hover > 0.f) ui.g.GlowRound(r, radius, tone, 18.f, 0.28f * hover * alpha);
    ui.g.FillRound(r, radius, Lerp(p.foreground.Fade(0.045f), p.foreground.Fade(0.075f), hover).Fade(alpha));
    ui.g.StrokeRound(r.Inset(0.5f), radius - 0.5f, Lerp(p.foreground.Fade(0.07f), tone.Fade(0.55f), hover).Fade(alpha), 1.f);
}

float Bursts::Progress(const std::wstring& id, double now) const {
    const auto it = at.find(id);
    if (it == at.end()) return -1.f;
    const float t = static_cast<float>((now - it->second) / kBurstSeconds);
    return t >= 0.f && t < 1.f ? t : -1.f;
}

void BurstRing(UI::Ui& ui, float cx, float cy, float radius, float t, Color tone) {
    if (t < 0.f) return;
    ui.KeepAnimating();
    const float e = Anim::EaseOutCubic(t);
    const float r = radius * (0.6f + 0.9f * e);
    ui.g.StrokeEllipse(cx, cy, r, r, tone.Fade(1.f - t), 3.f * (1.f - t) + 0.5f);
    // A few sparks flying out
    for (int k = 0; k < 8; ++k) {
        const float a = 6.2831853f * k / 8.f + 0.4f;
        const float d = radius * (0.5f + 1.1f * e);
        ui.g.FillEllipse(cx + std::cos(a) * d, cy + std::sin(a) * d, 2.5f * (1.f - t), 2.5f * (1.f - t), tone.Fade(1.f - t));
    }
}

namespace {
const wchar_t* PictureKind(Services::SaveFile::ListId list) {
    using L = Services::SaveFile::ListId;
    switch (list) {
        case L::Drivers: return L"driver";
        case L::Machines: return L"machine";
        case L::FrontParts: return L"front";
        case L::RearParts: return L"rear";
        case L::TireParts: return L"tire";
        case L::Stickers: return L"sticker";
        case L::Horns: return L"horn";
        case L::Gadgets: return L"gadget";
        case L::ColorPresets: return L"color";
        case L::Auras: return L"aura";
        case L::HonorTitles: return L"title";
        case L::Albums: return L"album";
        default: return nullptr;
    }
}
std::wstring PictureDir(Services::SaveFile::ListId list) { return Paths::Asset(Files::Combine(L"GameImages", PictureKind(list))); }
} // namespace

bool HasPictures(Services::SaveFile::ListId list) { return PictureKind(list) && Files::DirectoryExists(PictureDir(list)); }

void ShopBadge(UI::Ui& ui, float x, float y) {
    const Color gold = Rgba(234, 179, 8);
    // The size of the state's badge on the other corner
    ui.g.FillEllipse(x, y, 11.f, 11.f, gold);
    ui.g.StrokeEllipse(x, y, 11.f, 11.f, M::P().background, 2.f);
    M::DrawIcon(ui.g, M::Icon::Storefront, x, y, 13.f, Colors::White);
}

Rect TitleArt(const std::wstring& id) {
    static std::unordered_map<std::wstring, Rect> cache;
    const std::wstring key = id + L"|" + Loc::CurrentLang();
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
    Rect art{ 0.f, 0.f, 1.f, 1.f };
    const std::wstring dir = PictureDir(Services::SaveFile::ListId::HonorTitles);
    for (const std::wstring& name : { id + L"_" + Loc::CurrentLang(), id + L"_en", id }) {
        unsigned w = 0, h = 0;
        std::vector<uint32_t> px;
        if (!Images::Decode(Files::Combine(dir, name + L".png"), 512, w, h, px) || !w || !h) continue;
        unsigned x0 = w, y0 = h, x1 = 0, y1 = 0;
        for (unsigned y = 0; y < h; ++y)
            for (unsigned x = 0; x < w; ++x)
                if ((px[y * w + x] >> 24) > 40) {   // (premultiplied BGRA: the alpha is the top byte)
                    x0 = std::min(x0, x); x1 = std::max(x1, x + 1);
                    y0 = std::min(y0, y); y1 = std::max(y1, y + 1);
                }
        if (x1 > x0 && y1 > y0)
            art = { static_cast<float>(x0) / w, static_cast<float>(y0) / h, static_cast<float>(x1 - x0) / w, static_cast<float>(y1 - y0) / h };
        break;
    }
    cache[key] = art;
    return art;
}

namespace {
// A gadget's picture name: "Run12_UltimateChargeAttack" -> run12, "Hidden08_..." -> hidden8 (no leading zero)
std::wstring GadgetKey(const std::wstring& id) {
    std::wstring key = Text::Lower(id.substr(0, id.find(L'_')));
    const size_t digits = key.find_first_of(L"0123456789");
    if (digits != std::wstring::npos) {
        const size_t first = key.find_first_not_of(L'0', digits);
        key = key.substr(0, digits) + (first == std::wstring::npos ? L"0" : key.substr(first));
    }
    return key;
}
} // namespace

ImagePtr GadgetPlatePicture(const std::wstring& id) {
    return Images::Load(Files::Combine(PictureDir(Services::SaveFile::ListId::Gadgets), Files::Combine(L"plate", GadgetKey(id) + L".png")), 512);
}

ImagePtr Picture(Services::SaveFile::ListId list, const std::wstring& id) {
    if (!PictureKind(list) || id.empty()) return nullptr;
    // Gadgets by their kind and number: "Run12_UltimateChargeAttack" → run12, "Hidden08_..." → hidden8 (the
    // pictures' numbers have no leading zero); the ones the game has no art for show its own unknown
    // gadget's (UI_ItemIcon_UnknownGadget, as gadget\unknown.png)
    if (list == Services::SaveFile::ListId::Gadgets) {
        const std::wstring dir = PictureDir(list);
        if (ImagePtr image = Images::Load(Files::Combine(dir, GadgetKey(id) + L".png"), 256)) return image;
        return Images::Load(Files::Combine(dir, L"unknown.png"), 256);
    }
    std::wstring file = id;
    // Joker's EDriverId says 11002, but the game's texts and icon use 11001
    if (file == L"Extnd11_Character11002") file = L"Extnd11_Character11001";
    // An honor title: its plate with its name on it, in the app's language (the bare plate otherwise)
    if (list == Services::SaveFile::ListId::HonorTitles) {
        const std::wstring dir = PictureDir(list);
        // (Images::Load remembers misses too: trying costs nothing after the first time)
        for (const std::wstring& name : { file + L"_" + Loc::CurrentLang(), file + L"_en", file })
            if (ImagePtr image = Images::Load(Files::Combine(dir, name + L".png"), 512)) return image;
        return nullptr;
    }
    return Images::Load(Files::Combine(PictureDir(list), file + L".png"), 256);
}

bool RingToken(UI::Ui& ui, UI::Id id, float cx, float cy, float size, bool on, float burst, bool enabled) {
    const Rect r{ cx - size * 0.5f, cy - size * 0.5f, size, size };
    const auto it = ui.Interact(id, r, enabled);
    const M::Motion::Feedback fb = M::Motion::Hover(ui, id, it.hovered, it.held);
    const float s = BurstScale(burst) * (1.f + 0.1f * fb.hover) * (1.f - 0.06f * fb.press);
    const float half = size * 0.5f * s;
    const Rect drawn{ cx - half, cy - half, half * 2.f, half * 2.f };
    if (on) ui.g.GlowRound(drawn, half, kRing, size * 0.25f, 0.35f + 0.2f * fb.hover);
    if (const ImagePtr image = GameImage(L"rsr", on ? L"on" : L"off", 128)) {
        ui.g.DrawImage(*image, drawn, on || !enabled ? 1.f : 0.75f + 0.25f * fb.hover, Stretch::Uniform);
    } else {
        // Without the game's pictures: a ring with a star
        ui.g.FillEllipse(cx, cy, half, half, on ? kRing : M::P().foreground.Fade(0.08f + 0.06f * fb.hover));
        if (!on) ui.g.StrokeEllipse(cx, cy, half - 1.f, half - 1.f, kRing.Fade(0.45f + 0.4f * fb.hover), 1.5f);
        M::DrawIcon(ui.g, M::Icon::StarFourPoints, cx, cy, half * 1.05f, on ? Colors::White : kRing.Fade(0.6f));
    }
    BurstRing(ui, cx, cy, size * 0.6f, burst, kRing);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    return it.clicked && enabled;
}

std::wstring CourseTime(int64_t time) {
    const int64_t ms = time / 6;
    wchar_t text[32];
    swprintf_s(text, L"%lld:%02lld.%03lld", ms / 60000, ms / 1000 % 60, ms % 1000);
    return text;
}

std::optional<int64_t> ParseCourseTime(const std::wstring& text) {
    const std::wstring t = Text::Trim(text);
    if (t.empty()) return std::nullopt;
    int64_t minutes = 0;
    std::wstring rest = t;
    if (const size_t colon = t.find(L':'); colon != std::wstring::npos) {
        const auto m = Text::ToInt(t.substr(0, colon));
        if (!m || *m < 0) return std::nullopt;
        minutes = *m;
        rest = t.substr(colon + 1);
    }
    // Seconds, and up to three decimals (a comma works too)
    const size_t dot = rest.find_first_of(L".,");
    const auto seconds = Text::ToInt(rest.substr(0, dot));
    if (!seconds || *seconds < 0) return std::nullopt;
    int64_t ms = 0;
    if (dot != std::wstring::npos) {
        std::wstring frac = rest.substr(dot + 1);
        if (frac.empty() || frac.size() > 3) return std::nullopt;
        while (frac.size() < 3) frac += L'0';
        const auto f = Text::ToInt(frac);
        if (!f || *f < 0) return std::nullopt;
        ms = *f;
    }
    const int64_t total = (minutes * 60 + *seconds) * 1000 + ms;
    if (total <= 0 || total > 60LL * 60 * 1000) return std::nullopt;
    return total * 6;
}

Color SpeedColor(int speed) {
    switch (speed) {
        case 0: return Rgba(34, 197, 94);
        case 1: return Rgba(59, 130, 246);
        case 2: return Rgba(99, 102, 241);
        case 3: return Rgba(234, 179, 8);
        default: return Rgba(168, 85, 247);
    }
}

const char* MachineTypeKey(const std::wstring& id) {
    const std::wstring type = id.substr(0, id.find(L'_'));
    if (type == L"SPD") return "saveeditor_machine_type_spd";
    if (type == L"ACL") return "saveeditor_machine_type_acl";
    if (type == L"HND") return "saveeditor_machine_type_hnd";
    if (type == L"POW") return "saveeditor_machine_type_pow";
    return "saveeditor_machine_type_dsh";
}

std::wstring BuildName(const Services::SaveFile::MachineBuild& b) {
    // (the Boost machines float: their tyres don't count)
    const bool boost = b.slot.starts_with(L"DSH");
    if (b.front == b.rear && (boost || b.rear == b.tire))
        if (const wchar_t* name = Data::SaveNames::Find("machine:" + Text::Narrow(b.front), Loc::CurrentLang())) return name;
    return Loc::Get("saveeditor_build_custom");
}

bool MixedBuild(const Services::SaveFile::MachineBuild& b) {
    const std::wstring type = b.slot.substr(0, b.slot.find(L'_') + 1);
    // (the Boost machines float: their tyres don't count)
    const bool boost = type == L"DSH_";
    return !b.front.starts_with(type) || !b.rear.starts_with(type) || (!boost && !b.tire.starts_with(type));
}

const std::wstring& BuildTypeLabel(const Services::SaveFile::MachineBuild& b) {
    return Loc::Get(MixedBuild(b) ? "saveeditor_machine_type_mixed" : MachineTypeKey(b.slot));
}

std::wstring HornSound(const std::wstring& id) {
    // Looked up once per horn
    static std::unordered_map<std::wstring, std::wstring> found;
    const auto it = found.find(id);
    if (it != found.end()) return it->second;
    const std::wstring path = Paths::Asset(Files::Combine(L"GameImages", L"horn", id + L".wav"));
    return found[id] = Files::Exists(path) ? path : std::wstring{};
}

void PlayWav(const std::wstring& path) { PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT); }

ImagePtr GameImage(const wchar_t* kind, const std::wstring& id, int size) {
    if (id.empty()) return nullptr;
    // (the course photos are JPEG)
    const wchar_t* extension = std::wstring_view(kind) == L"stage" ? L".jpg" : L".png";
    return Images::Load(Paths::Asset(Files::Combine(L"GameImages", kind, id + extension)), size);
}

float BurstScale(float t) {
    if (t < 0.f) return 1.f;
    // Squash a little, then overshoot back
    return 1.f + 0.12f * std::sin(t * 3.14159265f) * (1.f - t) - 0.04f * std::sin(t * 6.2831853f) * (1.f - t);
}

} // namespace SonicRacingSaveManager::Main::SaveEditorKit

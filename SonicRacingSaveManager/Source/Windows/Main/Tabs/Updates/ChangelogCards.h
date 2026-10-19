#pragma once
#include <string>
#include <vector>
#include "Graphics/Graphics.h"
#include "UI/Material/PackIcon.h"
#include "UI/Ui.h"

namespace SonicRacingSaveManager::Main {

// The update's "What's New": every version between the installed one and the new one as a folding
// card (the newest open, glowing, with a LATEST pill), each with chips counting its changes per
// section, the "=====Shot=====" sections as coloured tags and the changes as bulleted lines.
class ChangelogCards {
public:
    explicit ChangelogCards(std::string idKey) : key(std::move(idKey)) {}
    // `changelog` is the published changelog.txt ("3.0.1\n- ...\n\n3.0.0\n- ..."); only the versions
    // newer than `current` and up to `latest` are shown (or the latest one, when none match)
    void Draw(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest);
    // Every version (the Changelogs tab), the `installed` one marked INSTALLED
    void DrawHistory(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& installed);

    // Colour and icon of a section ("Additions", "Bug Fixes"..., "Shot"...), and that colour made
    // readable as text on the current cards
    static Color SectionColor(const std::wstring& title);
    // The section title in the current language (the changelog itself is written in English)
    static std::wstring SectionName(const std::wstring& title);
    static UI::Material::Icon SectionIcon(const std::wstring& title);
    static Color ReadableColor(Color c);

private:
    struct Line {
        std::wstring text;
        std::vector<TextSpan> spans;   // bold "Topic:" / hex id prefix
        float y = 0.f, h = 0.f;        // relative to the card body
    };
    struct Section {
        std::wstring title;            // empty: changes before any section line
        float y = 0.f;
        std::vector<Line> lines;
    };
    struct Card {
        std::wstring version, label, date;
        std::vector<Section> sections;
        int changes = 0;
        float bodyH = 0.f;
    };
    void DrawList(UI::Ui& ui, const Rect& r, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest);
    void Build(UI::Ui& ui, const std::wstring& changelog, const std::wstring& current, const std::wstring& latest, float width);
    float DrawCard(UI::Ui& ui, size_t index, float x, float y, float w);

    std::string key;          // id seed (one instance per screen)
    bool history = false;
    std::vector<Card> cards;
    std::vector<bool> open;
    std::wstring source, sourceCurrent, sourceLatest;
    float builtWidth = -1.f;
    unsigned builtRevision = ~0u;   // palette the section colours were taken from
    bool builtHistory = false;
    double shownAt = -10.0;
    float viewTop = 0.f, viewBottom = 0.f;   // the visible part of the list, in its coordinates
};

} // namespace SonicRacingSaveManager::Main

#pragma once
#include <string>
#include "Graphics/Images.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SonicRacingSaveManager::Main {

// <Popup Placement="Bottom" StaysOpen="False"> under a header button, redesigned: grows out of the
// button with a little bounce (an arrow on top pointing at it) while it unfolds, its rows coming in
// one by one; the first click anywhere outside closes it with a quick fade. WPF popups are separate
// windows that can spill past the main window; here they're kept inside it.
class AnchoredPopup : public Overlay {
public:
    using Overlay::Overlay;
    void Toggle(const Rect& anchor);
    void Close();
    bool IsOpen() const { return open; }
    bool Visible() const override;
    void Draw(UI::Ui& ui, const Rect& screen) final;

protected:
    // Content size and drawing (inside the 2 px primary border, radius 5, paper background)
    virtual float Width(UI::Ui& ui) = 0;
    virtual float Height(UI::Ui& ui, float width) = 0;
    virtual void DrawContent(UI::Ui& ui, const Rect& box) = 0;
    virtual void OnOpened() {}
    // Entrance (0..1) of content row `index` while the popup opens
    float RowReveal(UI::Ui& ui, int index) const;

private:
    bool open = false;
    double changedAt = -10.0;
    Rect anchor;
};

// SettingsPopup (MainWindow.xaml): loading screen / Discord activity checkboxes (new in the C++ port)
// and the season theme radios.
class SettingsPopup : public AnchoredPopup {
public:
    using AnchoredPopup::AnchoredPopup;

protected:
    float Width(UI::Ui& ui) override;
    float Height(UI::Ui& ui, float width) override;
    void DrawContent(UI::Ui& ui, const Rect& box) override;
    void OnOpened() override;

private:
    bool showLoadingScreen = true;
};

// LanguagePopup: one banner per language showing its whole flag rolled on a cylinder; a click
// switches the interface language (saved in language.cfg).
class LanguagePopup : public AnchoredPopup {
public:
    using AnchoredPopup::AnchoredPopup;

    struct Language {
        const wchar_t* code;   // Translations\<code>.txt
        const wchar_t* flag;   // Assets\Flags\<flag>
        const wchar_t* name;   // in its own language (tooltip)
    };
    static const Language kLanguages[4];
    static ImagePtr Flag(const std::wstring& lang);
    static void Banner(UI::Ui& ui, const Rect& r, const ImagePtr& flag, float hover, float glint);

protected:
    float Width(UI::Ui& ui) override;
    float Height(UI::Ui& ui, float width) override;
    void DrawContent(UI::Ui& ui, const Rect& box) override;
};

} // namespace SonicRacingSaveManager::Main

#pragma once
#include <string>
#include "Graphics/Images.h"
#include "UI/Material/PackIcon.h"
#include "UI/Material/Theme.h"
#include "UI/Ui.h"

// MaterialDesign buttons (MaterialDesignTheme.Button.xaml) as the manager's XAML styled them.
namespace SonicRacingSaveManager::UI::Material {

struct ButtonLook {
    Color background = Colors::Transparent;
    Color border = Colors::Transparent;
    Color foreground;
    Color ripple;                 // RippleAssist.Feedback
    float borderThickness = 0.f;
    float radius = 2.f;           // ButtonAssist.CornerRadius
    float elevation = 0.f;        // ElevationAssist.Elevation (dp)
    float hoverElevation = -1.f;  // App.xaml implicit Button: animates to 8 on hover (-1 = no animation)
    bool darkenOnHover = false;   // ShadowAssist.Darken trigger (raised / FAB)
    bool hoverOverlay = false;    // flat / outlined: Foreground at .16 behind the content on hover
    bool ellipse = false;         // FAB / icon button
    Color disabledForeground = Colors::Transparent;   // SeasonalAccentButton swaps the text color when disabled
    TextStyle text;               // TextBlock.FontSize 14, Medium
    Edges padding{ 16.f, 4.f };
};

// <Button> with no style: App.xaml's implicit MaterialDesignRaisedButton (elevation 2 -> 8 on hover)
ButtonLook Raised();
// Style="{StaticResource MaterialDesignRaisedButton}" (no hover elevation animation)
ButtonLook RaisedStatic();
// MaterialDesignRaisedSecondaryButton
ButtonLook RaisedSecondary();
// SeasonalAccentButton (MainWindowResources.xaml): black text, the body color when disabled
ButtonLook SeasonalAccent();
ButtonLook Flat();
ButtonLook Outlined();
// CustomOutlinedButton (App.xaml): outlined with MaterialDesignDivider as border
ButtonLook CustomOutlined();
// MaterialDesignFloatingActionButton (56) / MiniButton (40): ellipse, Dp6, white ripple
ButtonLook Fab(Color background);
// MaterialDesignIconButton (based on the flat button): round, transparent, Primary foreground,
// hover / ripple in that foreground
ButtonLook IconButtonLook();
ButtonLook IconButtonLook(Color foreground);

// A styled button frame. Draw the content between construction and destruction (it shares the
// disabled opacity); `content` is the padded area.
class Button {
public:
    Button(Ui& ui, Id id, const Rect& r, const ButtonLook& look, bool enabled = true);
    ~Button();
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    bool Clicked() const { return state.clicked; }
    Color Foreground() const { return foreground; }
    Interaction state;
    Rect bounds, content;

private:
    Ui& ui;
    bool layer = false;
    bool pressed = false;   // scaled down while held
    Color foreground;
};

float ButtonWidth(Ui& ui, const std::wstring& text, const ButtonLook& look, float iconSize = 0.f, float gap = 8.f);

bool TextButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, const ButtonLook& look, bool enabled = true);
// <StackPanel Orientation="Horizontal"><PackIcon Margin="0,0,8,0"/><TextBlock/></StackPanel> content, centered
bool IconTextButton(Ui& ui, Id id, const Rect& r, Icon icon, const std::wstring& text, const ButtonLook& look,
                    bool enabled = true, float iconSize = 18.f, float gap = 8.f);
// FAB / icon button with a PackIcon centered in it; `iconColor` = the PackIcon's own Foreground
// (transparent = the button's foreground)
bool IconButton(Ui& ui, Id id, const Rect& r, Icon icon, float iconSize, const ButtonLook& look, bool enabled = true,
                Color iconColor = Colors::Transparent);
// FAB with an image (the header logos): fitted in `imageSize`, clipped to a circle when `clip`
bool ImageFab(Ui& ui, Id id, const Rect& r, const ImagePtr& image, float imageSize, const ButtonLook& look, bool clip = false,
              Stretch stretch = Stretch::Uniform);

} // namespace SonicRacingSaveManager::UI::Material

#pragma once
#include <optional>
#include <string>
#include "Core/Async.h"
#include "Graphics/Color.h"
#include "Services/Discord/DiscordOAuthService.h"
#include "Windows/Main/Tabs/Tab.h"

namespace SonicRacingSaveManager::Main {

// Features\Reports\Views\ReportsTabControl, redesigned: an animated "Report a Bug" banner over two
// columns, the form on the left (title, description, a screenshot drop zone with its preview, the
// logs row (attach logs.zip / copy them) and the send button) and on the right the Discord identity
// card and a checklist that ticks itself as the report gets ready. The report goes to the
// bug-reports webhook.
class ReportsTab : public Tab {
public:
    using Tab::Tab;
    void Draw(UI::Ui& ui, const Rect& area) override;
    void OnSelected() override;   // ReportsTabControl_Loaded

private:
    enum class StatusKind { Warning, Success, Error };

    void Form(UI::Ui& ui, const Rect& r);
    void Screenshot(UI::Ui& ui, const Rect& r);
    void LogsRow(UI::Ui& ui, const Rect& r);
    void SendRow(UI::Ui& ui, const Rect& r);
    float DiscordCard(UI::Ui& ui, const Rect& r, bool draw);
    void Checklist(UI::Ui& ui, const Rect& r);
    void PickImage();
    void Connect();
    void ShowConnected(const std::wstring& username);
    void ShowDisconnected();
    void Send();
    void ShowStatus(const char* messageKey, StatusKind kind);

    double loadedAt = -1.0;   // entrance of the whole page
    // The three Discord states keep their own visibility, as the named elements did
    bool connectRowVisible = true, waitingRowVisible = false, connectedVisible = false;
    std::wstring username;
    std::optional<Services::DiscordOAuth::User> discordUser;
    std::optional<Async::Cancel> authCancel;

    std::wstring title, description;
    std::wstring imagePath;
    std::wstring imageFileName;
    bool includeLogs = true;            // logs.zip goes with the report
    double logsCopiedAt = -10.0;        // "Copied!" feedback of the Copy logs button
    const char* statusKey = "";         // translated on every frame
    StatusKind statusKind = StatusKind::Warning;
    bool statusVisible = false;
    double statusAt = -10.0, sentAt = -10.0;
    bool sendEnabled = true;
    bool sending = false;
};

} // namespace SonicRacingSaveManager::Main

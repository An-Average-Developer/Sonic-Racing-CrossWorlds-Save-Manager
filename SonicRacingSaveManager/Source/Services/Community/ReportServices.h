#pragma once
#include <string>
#include "Services/Discord/DiscordOAuthService.h"

// The bug-reports Discord webhook. Blocking: call from worker threads.
namespace SonicRacingSaveManager::Services::Reports {

// Reports tab (ReportsTabControl.xaml.cs): an embed with the reporter's Discord name / id and avatar, the
// title and description, the optional screenshot and the optional logs.zip (new in the C++ port) (`logsZip` = the zip's
// bytes, empty = none) attached (multipart). True on HTTP 2xx.
bool SendBugReport(const DiscordOAuth::User& user, const std::wstring& title, const std::wstring& description, const std::wstring& imagePath,
                   const std::string& logsZip = {});

} // namespace SonicRacingSaveManager::Services::Reports

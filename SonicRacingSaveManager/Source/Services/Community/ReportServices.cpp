#include "Services/Community/ReportServices.h"
#include <ctime>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/App/AppVersion.h"

namespace SonicRacingSaveManager::Services::Reports {

namespace {

// DateTime.UtcNow.ToString("o"): 2026-10-18T12:34:56.0000000Z
std::wstring UtcNowRoundTrip() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    gmtime_s(&tm, &now);
    wchar_t text[40];
    swprintf_s(text, L"%04d-%02d-%02dT%02d:%02d:%02d.0000000Z", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return text;
}

std::string ImageMimeType(const std::wstring& path) {
    const std::wstring ext = Text::Lower(Files::Extension(path));
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".png") return "image/png";
    if (ext == L".gif") return "image/gif";
    if (ext == L".webp") return "image/webp";
    if (ext == L".bmp") return "image/bmp";
    return "application/octet-stream";
}

} // namespace

bool SendBugReport(const std::wstring& discordId, const std::wstring& discordUsername, const std::wstring& title,
                   const std::wstring& description, const std::wstring& imagePath, const std::string& logsZip) {
    Json::Value embed = Json::Value::MakeObject();
    Json::Value author = Json::Value::MakeObject();
    author.Set("name", discordUsername + L"  \x00B7  ID: " + discordId);
    embed.Set("author", author);
    embed.Set("title", title);
    embed.Set("description", description);
    embed.Set("color", Config::Discord::ReportEmbedColor);

    const bool withImage = !imagePath.empty() && Files::Exists(imagePath);
    const std::wstring fileName = Files::FileName(imagePath);
    if (withImage) {
        Json::Value image = Json::Value::MakeObject();
        image.Set("url", L"attachment://" + fileName);
        embed.Set("image", image);
    }
    if (!logsZip.empty()) {
        Json::Value fields = Json::Value::MakeArray();
        Json::Value logs = Json::Value::MakeObject();
        logs.Set("name", "Logs");
        logs.Set("value", "Attached as logs.zip");
        logs.Set("inline", true);
        fields.Push(logs);
        embed.Set("fields", fields);
    }
    Json::Value footer = Json::Value::MakeObject();
    footer.Set("text", L"Sonic Racing CrossWorlds Save Manager  \x00B7  " + AppVersion::DisplayVersion());
    embed.Set("footer", footer);
    embed.Set("timestamp", UtcNowRoundTrip());
    Json::Value payload = Json::Value::MakeObject();
    Json::Value embeds = Json::Value::MakeArray();
    embeds.Push(embed);
    payload.Set("embeds", embeds);

    Http::Request request;
    request.method = L"POST";
    request.url = Config::Discord::BugReportWebhook;
    request.timeoutMs = 30000;
    if (withImage || !logsZip.empty()) {
        Http::Multipart form;
        form.AddField("payload_json", payload.Serialize());
        int index = 0;
        if (withImage) {
            const auto bytes = Files::ReadBytes(imagePath);
            if (!bytes) return false;
            form.AddFile("files[" + std::to_string(index++) + "]", Text::Narrow(fileName), ImageMimeType(imagePath), *bytes);
        }
        if (!logsZip.empty()) form.AddFile("files[" + std::to_string(index++) + "]", "logs.zip", "application/zip", logsZip);
        request.body = form.Body();
        request.contentType = form.ContentType();
    } else {
        request.body = payload.Serialize();
        request.contentType = L"application/json; charset=utf-8";
    }
    return Http::Send(request).Ok();
}

} // namespace SonicRacingSaveManager::Services::Reports

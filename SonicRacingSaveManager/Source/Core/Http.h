#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Blocking WinHTTP client. Call it from worker threads only (see Core/Async.h).
namespace SonicRacingSaveManager::Http {

struct Request {
    std::wstring method = L"GET";
    std::wstring url;
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    std::string body;                    // raw bytes
    std::wstring contentType;            // header added when not empty
    int timeoutMs = 30000;
    bool followRedirects = true;
};

struct Response {
    int status = 0;                      // 0 = network error
    std::string body;
    std::map<std::wstring, std::wstring> headers;  // lower-case names
    std::wstring error;

    bool Ok() const { return status >= 200 && status < 300; }
    std::wstring Header(const std::wstring& name) const;
};

// `progress(received, total)` — total is 0 when the server doesn't say. Return false to cancel.
using Progress = std::function<bool(uint64_t received, uint64_t total)>;

Response Send(const Request& request, const Progress& progress = nullptr);
Response Get(const std::wstring& url, const std::vector<std::pair<std::wstring, std::wstring>>& headers = {},
             int timeoutMs = 30000);
Response PostJson(const std::wstring& url, const std::string& json,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});
Response PostForm(const std::wstring& url, const std::vector<std::pair<std::string, std::string>>& fields,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});

// Streams the body into `path` (written to `path.part` first, then renamed).
bool Download(const std::wstring& url, const std::wstring& path, const Progress& progress = nullptr,
              std::wstring* error = nullptr, const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});

// multipart/form-data body builder (Discord webhooks with attachments, etc.)
class Multipart {
public:
    Multipart();
    void AddField(const std::string& name, const std::string& value);
    void AddFile(const std::string& name, const std::string& fileName, const std::string& contentType,
                 const std::string& data);
    std::string Body() const;
    std::wstring ContentType() const;

private:
    std::string boundary;
    std::string body;
};

std::wstring DefaultUserAgent();

} // namespace SonicRacingSaveManager::Http

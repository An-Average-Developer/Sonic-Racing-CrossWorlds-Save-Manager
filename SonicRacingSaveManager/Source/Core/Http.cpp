#include "Core/Http.h"
#include <windows.h>
#include <winhttp.h>
#include "Core/Files.h"
#include "Core/Text.h"
#include "Version.h"

#pragma comment(lib, "winhttp.lib")

namespace SonicRacingSaveManager::Http {

namespace {

struct Handle {
    HINTERNET h = nullptr;
    Handle() = default;
    explicit Handle(HINTERNET handle) : h(handle) {}
    ~Handle() { if (h) WinHttpCloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

std::wstring LastErrorText(const wchar_t* step) {
    return std::wstring(step) + L" failed (" + std::to_wstring(GetLastError()) + L")";
}

// Shared session: WinHTTP sessions are thread safe and keep connections alive.
HINTERNET Session() {
    static HINTERNET session = [] {
        HINTERNET s = WinHttpOpen(DefaultUserAgent().c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
        if (s) {
            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            WinHttpSetOption(s, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
            DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
            WinHttpSetOption(s, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
        }
        return s;
    }();
    return session;
}

// Runs the request; body bytes go to `sink` as they arrive.
Response Execute(const Request& request, const Progress& progress, const std::function<bool(const char*, size_t)>& sink) {
    Response response;
    URL_COMPONENTS parts{ sizeof(URL_COMPONENTS) };
    wchar_t host[512]{}, path[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    wchar_t extra[4096]{};
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));
    if (!WinHttpCrackUrl(request.url.c_str(), 0, 0, &parts)) {
        response.error = L"Invalid URL";
        return response;
    }
    std::wstring object = std::wstring(path) + extra;
    if (object.empty()) object = L"/";

    HINTERNET session = Session();
    if (!session) {
        response.error = LastErrorText(L"WinHttpOpen");
        return response;
    }
    Handle connect(WinHttpConnect(session, host, parts.nPort, 0));
    if (!connect) {
        response.error = LastErrorText(L"WinHttpConnect");
        return response;
    }
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle req(WinHttpOpenRequest(connect.h, request.method.c_str(), object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!req) {
        response.error = LastErrorText(L"WinHttpOpenRequest");
        return response;
    }
    WinHttpSetTimeouts(req.h, request.timeoutMs, request.timeoutMs, request.timeoutMs, request.timeoutMs);
    if (!request.followRedirects) {
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    }

    std::wstring headers;
    for (const auto& [name, value] : request.headers) headers += name + L": " + value + L"\r\n";
    if (!request.contentType.empty()) headers += L"Content-Type: " + request.contentType + L"\r\n";

    const BOOL sent = WinHttpSendRequest(req.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                                         headers.empty() ? 0 : static_cast<DWORD>(-1L),
                                         request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
                                         static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(req.h, nullptr)) {
        response.error = LastErrorText(L"Request");
        return response;
    }

    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
                        &size, WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status);

    DWORD rawSize = 0;
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &rawSize,
                        WINHTTP_NO_HEADER_INDEX);
    if (rawSize) {
        std::wstring raw(rawSize / sizeof(wchar_t), L'\0');
        if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &rawSize,
                                WINHTTP_NO_HEADER_INDEX)) {
            for (const auto& line : Text::SplitLines(raw)) {
                const size_t colon = line.find(L':');
                if (colon == std::wstring::npos) continue;
                response.headers[Text::Lower(Text::Trim(line.substr(0, colon)))] = Text::Trim(line.substr(colon + 1));
            }
        }
    }

    uint64_t total = 0;
    if (auto length = response.headers.find(L"content-length"); length != response.headers.end())
        total = Text::ToUInt64(length->second).value_or(0);

    uint64_t received = 0;
    std::vector<char> buffer(64 * 1024);
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.h, &available)) {
            response.error = LastErrorText(L"Read");
            response.status = 0;
            return response;
        }
        if (available == 0) break;
        while (available > 0) {
            DWORD read = 0;
            const DWORD chunk = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
            if (!WinHttpReadData(req.h, buffer.data(), chunk, &read) || read == 0) {
                available = 0;
                break;
            }
            available -= read;
            received += read;
            if (!sink(buffer.data(), read)) {
                response.error = L"Write failed";
                response.status = 0;
                return response;
            }
            if (progress && !progress(received, total)) {
                response.error = L"Cancelled";
                response.status = 0;
                return response;
            }
        }
    }
    return response;
}

} // namespace

std::wstring Response::Header(const std::wstring& name) const {
    auto it = headers.find(Text::Lower(name));
    return it == headers.end() ? std::wstring() : it->second;
}

std::wstring DefaultUserAgent() { return L"SonicRacingSaveManager/" + Text::Widen(APP_VERSION_STRING); }

Response Send(const Request& request, const Progress& progress) {
    std::string body;
    Response response = Execute(request, progress, [&](const char* data, size_t size) {
        body.append(data, size);
        return true;
    });
    response.body = std::move(body);
    return response;
}

Response Get(const std::wstring& url, const std::vector<std::pair<std::wstring, std::wstring>>& headers, int timeoutMs) {
    Request request;
    request.url = url;
    request.headers = headers;
    request.timeoutMs = timeoutMs;
    return Send(request);
}

Response PostJson(const std::wstring& url, const std::string& json,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    Request request;
    request.method = L"POST";
    request.url = url;
    request.headers = headers;
    request.body = json;
    request.contentType = L"application/json";
    return Send(request);
}

Response PostForm(const std::wstring& url, const std::vector<std::pair<std::string, std::string>>& fields,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    Request request;
    request.method = L"POST";
    request.url = url;
    request.headers = headers;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i) request.body += '&';
        request.body += Text::UrlEncode(fields[i].first) + "=" + Text::UrlEncode(fields[i].second);
    }
    request.contentType = L"application/x-www-form-urlencoded";
    return Send(request);
}

bool Download(const std::wstring& url, const std::wstring& path, const Progress& progress, std::wstring* error,
              const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    const std::wstring partial = path + L".part";
    Files::CreateDirectories(Files::DirectoryName(path));
    HANDLE file = CreateFileW(partial.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (error) *error = L"Cannot create " + partial;
        return false;
    }
    Request request;
    request.url = url;
    request.headers = headers;
    request.timeoutMs = 60000;
    Response response = Execute(request, progress, [&](const char* data, size_t size) {
        DWORD written = 0;
        return WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
    });
    CloseHandle(file);
    if (!response.Ok()) {
        DeleteFileW(partial.c_str());
        if (error) *error = response.error.empty() ? L"HTTP " + std::to_wstring(response.status) : response.error;
        return false;
    }
    if (!Files::Move(partial, path, true)) {
        DeleteFileW(partial.c_str());
        if (error) *error = L"Cannot write " + path;
        return false;
    }
    return true;
}

Multipart::Multipart() {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    boundary = "----SRCWBoundary" + std::to_string(counter.QuadPart);
}

void Multipart::AddField(const std::string& name, const std::string& value) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
    body += value + "\r\n";
}

void Multipart::AddFile(const std::string& name, const std::string& fileName, const std::string& contentType,
                        const std::string& data) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + name + "\"; filename=\"" + fileName + "\"\r\n";
    body += "Content-Type: " + contentType + "\r\n\r\n";
    body += data;
    body += "\r\n";
}

std::string Multipart::Body() const { return body + "--" + boundary + "--\r\n"; }

std::wstring Multipart::ContentType() const { return L"multipart/form-data; boundary=" + Text::Widen(boundary); }

} // namespace SonicRacingSaveManager::Http

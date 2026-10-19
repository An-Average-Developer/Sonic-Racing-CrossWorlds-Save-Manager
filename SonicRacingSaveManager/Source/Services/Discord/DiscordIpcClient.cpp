#include "Services/Discord/DiscordIpcClient.h"
#include <windows.h>
#include <chrono>
#include "Core/Text.h"

namespace SonicRacingSaveManager::Services {

namespace {

enum Opcode { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

} // namespace

DiscordIpcClient::DiscordIpcClient(std::string id) : clientId(std::move(id)) {
    worker = std::thread([this] { Run(); });
}

DiscordIpcClient::~DiscordIpcClient() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
        // Clear the presence on the way out
        pendingActivity = Json::Value();
        hasPending = true;
    }
    wake.notify_all();
    if (worker.joinable()) worker.join();
}

void DiscordIpcClient::SetActivity(const Json::Value& activity) {
    {
        std::lock_guard lock(mutex);
        pendingActivity = activity;
        hasPending = true;
    }
    wake.notify_all();
}

std::optional<DiscordIpcClient::User> DiscordIpcClient::ConnectedUser() const {
    std::lock_guard lock(mutex);
    return user;
}

bool DiscordIpcClient::Connect() {
    for (int i = 0; i < 10; ++i) {
        const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
        HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        pipe = h;
        Json::Value hello = Json::Value::MakeObject();
        hello.Set("v", 1);
        hello.Set("client_id", clientId);
        int op = 0;
        std::string reply;
        if (Write(Handshake, hello.Serialize()) && Read(op, reply, 5000) && op == Frame) {
            const Json::Value ready = Json::Parse(std::string_view(reply));
            if (ready["evt"].Str() == "READY") {
                std::lock_guard lock(mutex);
                user = User{ ready["data"]["user"]["id"].WStr(), ready["data"]["user"]["username"].WStr() };
                return true;
            }
        }
        Disconnect();
    }
    return false;
}

void DiscordIpcClient::Disconnect() {
    if (pipe) CloseHandle(static_cast<HANDLE>(pipe));
    pipe = nullptr;
    std::lock_guard lock(mutex);
    user.reset();
}

bool DiscordIpcClient::Write(int opcode, const std::string& json) {
    if (!pipe) return false;
    std::string frame(8, '\0');
    const int32_t op = opcode, len = static_cast<int32_t>(json.size());
    memcpy(frame.data(), &op, 4);
    memcpy(frame.data() + 4, &len, 4);
    frame += json;
    DWORD written = 0;
    return WriteFile(static_cast<HANDLE>(pipe), frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) &&
           written == frame.size();
}

bool DiscordIpcClient::Read(int& opcode, std::string& json, unsigned timeoutMs) {
    if (!pipe) return false;
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    // Wait for the 8-byte header without blocking forever
    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(static_cast<HANDLE>(pipe), nullptr, 0, nullptr, &available, nullptr)) return false;
        if (available >= 8) break;
        if (GetTickCount64() > deadline) return false;
        Sleep(20);
    }
    int32_t header[2]{};
    DWORD read = 0;
    if (!ReadFile(static_cast<HANDLE>(pipe), header, 8, &read, nullptr) || read != 8) return false;
    opcode = header[0];
    json.assign(static_cast<size_t>(std::max(0, header[1])), '\0');
    size_t done = 0;
    while (done < json.size()) {
        if (!ReadFile(static_cast<HANDLE>(pipe), json.data() + done, static_cast<DWORD>(json.size() - done), &read, nullptr) || read == 0)
            return false;
        done += read;
    }
    return true;
}

void DiscordIpcClient::Run() {
    bool connected = false;
    Json::Value lastSent;
    bool sentOnce = false;
    while (true) {
        if (!connected && !stopping) connected = Connect();
        if (connected) {
            // Answer pings / notice a closed pipe
            DWORD available = 0;
            if (!PeekNamedPipe(static_cast<HANDLE>(pipe), nullptr, 0, nullptr, &available, nullptr)) {
                Disconnect();
                connected = false;
                sentOnce = false;
            } else if (available >= 8) {
                int op = 0;
                std::string body;
                if (Read(op, body, 1000)) {
                    if (op == Ping) Write(Pong, body);
                    else if (op == Close) {
                        Disconnect();
                        connected = false;
                        sentOnce = false;
                    }
                }
            }
        }

        Json::Value activity;
        bool send = false;
        {
            std::unique_lock lock(mutex);
            if (hasPending) {
                activity = pendingActivity;
                lastSent = activity;
                hasPending = false;
                send = true;
            } else if (connected && !sentOnce) {
                activity = lastSent;   // re-send after a reconnect
                send = true;
            }
        }
        if (send && connected) {
            Json::Value args = Json::Value::MakeObject();
            args.Set("pid", static_cast<int>(GetCurrentProcessId()));
            args.Set("activity", activity);
            Json::Value cmd = Json::Value::MakeObject();
            cmd.Set("cmd", "SET_ACTIVITY");
            cmd.Set("args", args);
            cmd.Set("nonce", std::to_string(++nonce));
            if (Write(Frame, cmd.Serialize())) {
                sentOnce = true;
                int op = 0;
                std::string reply;
                Read(op, reply, 2000);   // acknowledgement (ignored)
            } else {
                Disconnect();
                connected = false;
                sentOnce = false;
            }
        }
        if (stopping) break;

        std::unique_lock lock(mutex);
        wake.wait_for(lock, std::chrono::milliseconds(connected ? 1000 : 5000), [this] { return hasPending || stopping.load(); });
    }
    Disconnect();
}

} // namespace SonicRacingSaveManager::Services

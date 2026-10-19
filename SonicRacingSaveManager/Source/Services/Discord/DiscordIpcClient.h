#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include "Core/Json.h"

namespace SonicRacingSaveManager::Services {

// Minimal Discord RPC client over the local IPC pipe (\\.\pipe\discord-ipc-N).
// A background thread keeps the connection alive (reconnecting every few seconds) and sends the
// latest activity; SetActivity only stores it, so callers never block.
class DiscordIpcClient {
public:
    struct User {
        std::wstring id, username;
    };

    explicit DiscordIpcClient(std::string clientId);
    ~DiscordIpcClient();
    DiscordIpcClient(const DiscordIpcClient&) = delete;
    DiscordIpcClient& operator=(const DiscordIpcClient&) = delete;

    // `activity` = the RPC activity object, or null to clear it.
    void SetActivity(const Json::Value& activity);
    std::optional<User> ConnectedUser() const;

private:
    void Run();
    bool Connect();
    void Disconnect();
    bool Write(int opcode, const std::string& json);
    bool Read(int& opcode, std::string& json, unsigned timeoutMs);

    std::string clientId;
    std::thread worker;
    std::atomic_bool stopping{ false };
    mutable std::mutex mutex;
    std::condition_variable wake;
    Json::Value pendingActivity;
    bool hasPending = false;
    std::optional<User> user;
    void* pipe = nullptr;   // HANDLE
    int nonce = 0;
};

} // namespace SonicRacingSaveManager::Services

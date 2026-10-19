#pragma once
#include <string>

// The manager's log: one file per session in %LOCALAPPDATA%\SonicRacingSaveManager\logs
// (manager.log, the previous session kept as manager-previous.log so a crash's log survives the
// restart). Thread-safe. Bug reports can attach it as logs.zip.
namespace SonicRacingSaveManager::Log {

enum class Level { Info, Warning, Error };

// Starts a new session: rotates the files and writes the header (version, Windows build...)
void Start();
void Write(Level level, const std::wstring& message);
inline void Info(const std::wstring& message) { Write(Level::Info, message); }
inline void Warning(const std::wstring& message) { Write(Level::Warning, message); }
inline void Error(const std::wstring& message) { Write(Level::Error, message); }

// This session's log followed by the previous one (for the clipboard)
std::wstring Text();
// logs.zip: manager.log, manager-previous.log, the last update's update.log and system.txt
std::string Zip();

} // namespace SonicRacingSaveManager::Log

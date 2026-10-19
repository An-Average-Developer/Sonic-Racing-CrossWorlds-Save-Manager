#pragma once

namespace SonicRacingSaveManager::App {

// Runs until PostQuitMessage: pumps window messages, runs posted callbacks / timers and renders
// every window that asked for a frame (paced to the display with DwmFlush).
int RunMessageLoop();

} // namespace SonicRacingSaveManager::App

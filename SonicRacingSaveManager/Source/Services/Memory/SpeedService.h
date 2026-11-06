#pragma once
#include <memory>
#include <vector>
#include "Services/Memory/ProcessMemory.h"

// Top speed in races, for the player only (racer 0). Found on 1.4.x (module offsets of that version
// in the comments; the manager finds every spot by an AOB, so it survives updates that move code):
//
// Every racer has an engine object (vtable module+7545908): +0x18 racer index (0 = the player, as in
// the ring blocks), +0xBC speed in m/s, +0xDC top speed in km/h, +0xE0 an override of the machine's
// base top speed (the CPU catch-up writes it every frame). Four code caves, each one checking that the
// engine is racer 0's and leaving the others alone:
//   A  module+466ECB5 (the top speed function, vtable slot +0x1E8): the override reads our limit
//      -> the boost and dash speeds go up to it
//   B  module+466E629 (GetTopSpeed, the same sum elsewhere): likewise
//   C  module+466B8EE (the cruise speed after a boost, 160/175/190/250 km/h, +0xF60 of the racer's
//      [+0xF0] object, which sets the speed outright): our limit
//   D  module+4627D42 (the land movement update, speed += push * dt): with the throttle on
//      (movement +0xD0 > 0.5) and the brake off (+0xD4 < 0.5), below the limit the push is at least
//      kMinPush (past the engine's 8000 rpm limiter in 4th gear, which caps it at ~240 km/h); at the
//      limit it's held between 0 and kHold, so it stays there without the limiter's hard braking
// Each cave keeps its own copy of the limit (km/h for A-C, m/s for D); SetLimit rewrites them.
// The engine is still looked for (by scanning the heap for its vtable on a worker thread) only to
// show the live speed.
namespace SonicRacingSaveManager::Memory {

class SpeedService {
public:
    static constexpr float kUnlimited = 999.f;

    explicit SpeedService(ProcessMemory& memory) : memory(memory) {}
    ~SpeedService() { ++*generation; }   // a scan finishing later is ignored
    SpeedService(const SpeedService&) = delete;
    SpeedService& operator=(const SpeedService&) = delete;

    bool IsOn() const { return on; }
    float Limit() const { return limit; }
    void SetLimit(float kmh);
    // Finds the four spots, builds the caves and patches them in; false when the game's code wasn't
    // found (nothing is patched then)
    bool Enable();
    // Writes the game's original instructions back
    void Disable();
    // Detach / the game closed: forget everything
    void Reset();
    // Every 100 ms on the UI thread: follows the player's engine for the live speed
    void Tick();
    float SpeedKmh() const { return speedKmh; }
    float TopKmh() const { return topKmh; }
    bool Searching() const { return on && inRace && engine == 0; }

private:
    struct Site {
        int64_t address = 0;
        Bytes original;
    };
    bool FindSites();
    bool BuildCaves();
    void WriteLimit();
    bool EngineValid(int64_t address) const;
    void StartScan();

    ProcessMemory& memory;
    Site sites[4];
    int64_t caves = 0;
    std::vector<int64_t> kmhLimits, msLimits;   // the limit's copies in the caves
    int64_t vtable = 0, raceGlobal = 0, engine = 0, engineRace = 0;
    bool on = false, inRace = false, scanning = false;
    float limit = 300.f, speedKmh = 0.f, topKmh = 0.f;
    double lastScan = -100.0;
    // Captured by the scan's continuation: changes once this service is gone or reset
    std::shared_ptr<unsigned> generation = std::make_shared<unsigned>(0);
};

} // namespace SonicRacingSaveManager::Memory

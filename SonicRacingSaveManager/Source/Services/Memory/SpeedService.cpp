#include "Services/Memory/SpeedService.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include "Core/Async.h"
#include "Services/Memory/RingService.h"

namespace SonicRacingSaveManager::Memory {

namespace {

// ── The game's code ─────────────────────────────────────────────────────────

// The engine's top speed function (module+466EC40); site A is at +0x75: movss xmm1,[rdi+0E0h]
constexpr const char* kTopSpeedPattern =
    "48 89 5C 24 18 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 83 EC 50 48 8B 01 4C 8D B1 F0 00 00 00 4C 8D A1 F4 00 00 00";
constexpr int64_t kTopSpeedSlot = 0x1E8, kSiteA = 0x75;
// GetTopSpeed (module+466E606); site B at +0x23: movss xmm0,[rbx+0E0h]
constexpr const char* kGetTopPattern = "48 8B 81 88 00 00 00 48 8B D9 48 8B 89 80 00 00 00 48 8B 50 58 8B 52 4C E8 ?? ?? ?? ?? 0F 28 D0 0F 57 C9";
constexpr int64_t kSiteB = 0x23;
// The cruise speed (module+466B8DE); site C at +0x10: movss [rbx+0F60h],xmm6
constexpr const char* kCruisePattern = "39 BB 58 0F 00 00 7D 08 F3 0F 10 35 ?? ?? ?? ??";
constexpr int64_t kSiteC = 0x10;
// The push of the land movement update (module+4627D32); site D at +0x10: mov rax,[rdi+30h]; mulss xmm0,xmm6
constexpr const char* kPushPattern = "48 8B 4F 50 0F 28 F0 48 8B 01 FF 90 C0 01 00 00 ?? ?? ?? ?? ?? ?? ?? ?? F3 0F 58 40 1C F3 0F 11 40 1C";
constexpr int64_t kSiteD = 0x10;

const Bytes kOriginalA = { 0xF3, 0x0F, 0x10, 0x8F, 0xE0, 0x00, 0x00, 0x00 };   // movss xmm1,[rdi+0E0h]
const Bytes kOriginalB = { 0xF3, 0x0F, 0x10, 0x83, 0xE0, 0x00, 0x00, 0x00 };   // movss xmm0,[rbx+0E0h]
const Bytes kOriginalC = { 0xF3, 0x0F, 0x11, 0xB3, 0x60, 0x0F, 0x00, 0x00 };   // movss [rbx+0F60h],xmm6
const Bytes kOriginalD = { 0x48, 0x8B, 0x47, 0x30, 0xF3, 0x0F, 0x59, 0xC6 };   // mov rax,[rdi+30h]; mulss xmm0,xmm6

constexpr float kMinPush = 20.f, kHold = -5.f;
constexpr int64_t kIndex = 0x18, kSpeed = 0xBC, kTop = 0xDC;
constexpr double kRescanDelay = 1.5, kScanTimeout = 15.0;
constexpr float kMsToKmh = 3.6f;
constexpr size_t kCaveSize = 0x1000;
constexpr int64_t kCaveStride = 0x80;

// ── A tiny assembler: bytes, labels, rel8 / rip-relative fixups and jumps back to the game ──

class Asm {
public:
    explicit Asm(int64_t base) : base(base) {}
    Asm& Raw(std::initializer_list<uint8_t> bytes) {
        out.insert(out.end(), bytes);
        return *this;
    }
    Asm& Raw(const Bytes& bytes) {
        out.insert(out.end(), bytes.begin(), bytes.end());
        return *this;
    }
    // Short jump to a label (0x75 jne, 0x73 jae, 0x72 jb, 0xEB jmp)
    Asm& Jump8(uint8_t opcode, const std::string& label) {
        out.push_back(opcode);
        fixups.push_back({ out.size(), label, 1 });
        out.push_back(0);
        return *this;
    }
    // An instruction ending in a rip-relative disp32 to a label (a float of this cave)
    Asm& Rip(std::initializer_list<uint8_t> opcode, const std::string& label) {
        out.insert(out.end(), opcode);
        fixups.push_back({ out.size(), label, 4 });
        out.insert(out.end(), 4, 0);
        return *this;
    }
    // jmp rel32 to an absolute address (back into the game)
    Asm& JumpTo(int64_t target) {
        out.push_back(0xE9);
        const int32_t rel = static_cast<int32_t>(target - (base + static_cast<int64_t>(out.size()) + 4));
        Append(&rel, 4);
        return *this;
    }
    Asm& Label(const std::string& name) {
        labels[name] = out.size();
        return *this;
    }
    Asm& Float(const std::string& name, float value) {
        while (out.size() % 4) out.push_back(0xCC);
        Label(name);
        Append(&value, 4);
        return *this;
    }
    int64_t Address(const std::string& label) const { return base + static_cast<int64_t>(labels.at(label)); }
    Bytes Link() {
        for (const Fixup& f : fixups) {
            const int64_t rel = static_cast<int64_t>(labels.at(f.label)) - static_cast<int64_t>(f.at + f.size);
            if (f.size == 1) out[f.at] = static_cast<uint8_t>(static_cast<int8_t>(rel));
            else {
                const int32_t rel32 = static_cast<int32_t>(rel);
                std::memcpy(&out[f.at], &rel32, 4);
            }
        }
        return out;
    }

private:
    struct Fixup {
        size_t at;
        std::string label;
        size_t size;
    };
    void Append(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    }
    int64_t base;
    Bytes out;
    std::map<std::string, size_t> labels;
    std::vector<Fixup> fixups;
};

// jmp rel32 to the cave + 3 nops: replaces the 8 bytes of the original instruction(s)
Bytes JumpPatch(int64_t from, int64_t to) {
    const int32_t rel = static_cast<int32_t>(to - (from + 5));
    Bytes b = { 0xE9 };
    const auto* r = reinterpret_cast<const uint8_t*>(&rel);
    b.insert(b.end(), r, r + 4);
    b.insert(b.end(), 3, 0x90);
    return b;
}

} // namespace

// ── Patching ────────────────────────────────────────────────────────────────

bool SpeedService::FindSites() {
    if (sites[0].address) return true;
    const struct {
        const char* pattern;
        int64_t offset;
        const Bytes* original;
    } spots[4] = { { kTopSpeedPattern, kSiteA, &kOriginalA }, { kGetTopPattern, kSiteB, &kOriginalB },
                   { kCruisePattern, kSiteC, &kOriginalC }, { kPushPattern, kSiteD, &kOriginalD } };
    Site found[4];
    for (int i = 0; i < 4; ++i) {
        const int64_t at = memory.FindPattern(spots[i].pattern);
        if (at == 0) return false;
        found[i] = { at + spots[i].offset, *spots[i].original };
        // The original instruction, or a jump left by an earlier session of the manager
        Bytes now(found[i].original.size());
        if (!memory.Read(found[i].address, now.data(), now.size())) return false;
        if (now != found[i].original && now[0] != 0xE9) return false;
    }
    for (int i = 0; i < 4; ++i) sites[i] = found[i];
    // The engine class (for the live speed) and the race object
    const int64_t function = sites[0].address - kSiteA;
    if (const int64_t slot = memory.FindQwordInModule(static_cast<uint64_t>(function))) vtable = slot - kTopSpeedSlot;
    raceGlobal = FindRaceGlobal(memory);
    return true;
}

bool SpeedService::BuildCaves() {
    if (caves) return true;
    const int64_t base = memory.AllocateNearModule(kCaveSize);
    if (base == 0) return false;
    kmhLimits.clear();
    msLimits.clear();
    const auto Back = [this](int i) { return sites[i].address + static_cast<int64_t>(sites[i].original.size()); };
    const auto Put = [this](Asm& a, int64_t at) { return memory.WriteCode(at, a.Link()); };

    // A: player ? movss xmm1,[limit] : movss xmm1,[rdi+0E0h]
    {
        const int64_t at = base;
        Asm a(at);
        a.Raw({ 0x83, 0x7F, 0x18, 0x00 }).Jump8(0x75, "orig");                    // cmp dword [rdi+18h],0 ; jne orig
        a.Rip({ 0xF3, 0x0F, 0x10, 0x0D }, "limit").JumpTo(Back(0));               // movss xmm1,[limit] ; jmp back
        a.Label("orig").Raw(sites[0].original).JumpTo(Back(0));
        a.Float("limit", limit);
        if (!Put(a, at)) return false;
        kmhLimits.push_back(a.Address("limit"));
    }
    // B: player ? movss xmm0,[limit] : movss xmm0,[rbx+0E0h]
    {
        const int64_t at = base + kCaveStride;
        Asm a(at);
        a.Raw({ 0x83, 0x7B, 0x18, 0x00 }).Jump8(0x75, "orig");                    // cmp dword [rbx+18h],0 ; jne orig
        a.Rip({ 0xF3, 0x0F, 0x10, 0x05 }, "limit").JumpTo(Back(1));               // movss xmm0,[limit] ; jmp back
        a.Label("orig").Raw(sites[1].original).JumpTo(Back(1));
        a.Float("limit", limit);
        if (!Put(a, at)) return false;
        kmhLimits.push_back(a.Address("limit"));
    }
    // C: if the engine [[rbx+50h]+70h] is the player's: xmm6 = limit; then movss [rbx+0F60h],xmm6
    {
        const int64_t at = base + 2 * kCaveStride;
        Asm a(at);
        a.Raw({ 0x48, 0x8B, 0x43, 0x50, 0x48, 0x8B, 0x40, 0x70 });                // mov rax,[rbx+50h] ; mov rax,[rax+70h]
        a.Raw({ 0x83, 0x78, 0x18, 0x00 }).Jump8(0x75, "store");                   // cmp dword [rax+18h],0 ; jne store
        a.Rip({ 0xF3, 0x0F, 0x10, 0x35 }, "limit");                               // movss xmm6,[limit]
        a.Label("store").Raw(sites[2].original).JumpTo(Back(2));
        a.Float("limit", limit);
        if (!Put(a, at)) return false;
        kmhLimits.push_back(a.Address("limit"));
    }
    // D: the player's push, with the throttle on and the brake off:
    //    below the limit at least minPush; at the limit between hold and 0
    {
        const int64_t at = base + 3 * kCaveStride;
        Asm a(at);
        a.Raw({ 0x48, 0x8B, 0x47, 0x50, 0x83, 0x78, 0x18, 0x00 }).Jump8(0x75, "skip");               // engine = [rdi+50h] ; racer 0?
        a.Raw({ 0xF3, 0x0F, 0x10, 0x8F, 0xD4, 0x00, 0x00, 0x00 }).Rip({ 0x0F, 0x2F, 0x0D }, "half");  // brake [rdi+0D4h] vs 0.5
        a.Jump8(0x73, "skip");
        a.Raw({ 0xF3, 0x0F, 0x10, 0x8F, 0xD0, 0x00, 0x00, 0x00 }).Rip({ 0x0F, 0x2F, 0x0D }, "half");  // throttle [rdi+0D0h] vs 0.5
        a.Jump8(0x72, "skip");
        a.Raw({ 0x48, 0x8B, 0x47, 0x30, 0xF3, 0x0F, 0x10, 0x48, 0x1C }).Rip({ 0x0F, 0x2F, 0x0D }, "limit");   // speed vs limit
        a.Jump8(0x73, "above");
        a.Rip({ 0xF3, 0x0F, 0x5F, 0x05 }, "minPush").Jump8(0xEB, "skip");         // maxss xmm0,[minPush]
        a.Label("above").Raw({ 0x0F, 0x57, 0xC9, 0xF3, 0x0F, 0x5D, 0xC1 });       // xorps xmm1,xmm1 ; minss xmm0,xmm1
        a.Rip({ 0xF3, 0x0F, 0x5F, 0x05 }, "hold");                                // maxss xmm0,[hold]
        a.Label("skip").Raw(sites[3].original).JumpTo(Back(3));
        a.Float("half", 0.5f).Float("limit", limit / kMsToKmh).Float("minPush", kMinPush).Float("hold", kHold);
        if (!Put(a, at)) return false;
        msLimits.push_back(a.Address("limit"));
    }
    caves = base;
    return true;
}

void SpeedService::WriteLimit() {
    if (!caves) return;
    for (const int64_t at : kmhLimits) memory.Write(at, &limit, sizeof(limit));
    const float ms = limit / kMsToKmh;
    for (const int64_t at : msLimits) memory.Write(at, &ms, sizeof(ms));
}

void SpeedService::SetLimit(float kmh) {
    limit = kmh;
    if (on) WriteLimit();
}

bool SpeedService::Enable() {
    if (!FindSites() || !BuildCaves()) return false;
    WriteLimit();
    // The caves are complete: now point the game at them
    for (int i = 0; i < 4; ++i) {
        if (!memory.WriteCode(sites[i].address, JumpPatch(sites[i].address, caves + i * kCaveStride))) {
            for (int j = 0; j < i; ++j) memory.WriteCode(sites[j].address, sites[j].original);
            return false;
        }
    }
    on = true;
    Tick();
    return true;
}

void SpeedService::Disable() {
    if (sites[0].address)
        for (const Site& s : sites) memory.WriteCode(s.address, s.original);
    on = false;
    engine = 0;
    speedKmh = topKmh = 0.f;
}

void SpeedService::Reset() {
    on = false;
    for (Site& s : sites) s = {};
    caves = 0;
    kmhLimits.clear();
    msLimits.clear();
    vtable = raceGlobal = engine = engineRace = 0;
    scanning = false;
    speedKmh = topKmh = 0.f;
    ++*generation;
}

// ── Live speed ──────────────────────────────────────────────────────────────

bool SpeedService::EngineValid(int64_t address) const {
    int64_t vt = 0;
    int32_t index = -1;
    return memory.Read(address, &vt, sizeof(vt)) && vt == vtable && memory.Read(address + kIndex, &index, sizeof(index)) && index == 0;
}

void SpeedService::StartScan() {
    scanning = true;
    lastScan = Async::Now();
    const uint32_t pid = memory.ProcessId();
    const int64_t target = vtable;
    std::shared_ptr<unsigned> token = generation;
    const unsigned at = *token;
    Async::Run(
        [pid, target] {
            std::vector<int64_t> engines;
            HANDLE h = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (!h) return engines;
            for (const int64_t address : ProcessMemory::ScanForPointer(pid, static_cast<uint64_t>(target))) {
                int32_t index = -1;
                SIZE_T read = 0;
                if (ReadProcessMemory(h, reinterpret_cast<LPCVOID>(address + kIndex), &index, sizeof(index), &read) && read == sizeof(index) && index == 0)
                    engines.push_back(address);
            }
            CloseHandle(h);
            return engines;
        },
        [this, token, at](std::vector<int64_t> found) {
            if (*token != at) return;   // reset (or detached) meanwhile
            scanning = false;
            // Freed engines of earlier races can still hold the vtable: the fastest one (or the only one)
            int64_t best = 0;
            float bestSpeed = -1.f;
            for (const int64_t address : found) {
                float speed = 0.f;
                if (!EngineValid(address) || !memory.Read(address + kSpeed, &speed, sizeof(speed))) continue;
                if (std::fabs(speed) > bestSpeed) {
                    best = address;
                    bestSpeed = std::fabs(speed);
                }
            }
            int64_t race = 0;
            if (best && memory.Read(raceGlobal, &race, sizeof(race)) && race != 0) {
                engine = best;
                engineRace = race;
            }
        });
}

void SpeedService::Tick() {
    if (!on || vtable == 0 || raceGlobal == 0) return;
    int64_t race = 0;
    inRace = memory.Read(raceGlobal, &race, sizeof(race)) && race != 0;
    if (!inRace) {
        engine = 0;
        speedKmh = topKmh = 0.f;
        return;
    }
    if (engine && (race != engineRace || !EngineValid(engine))) engine = 0;
    if (engine == 0) {
        speedKmh = topKmh = 0.f;
        if ((!scanning && Async::Now() - lastScan > kRescanDelay) || Async::Now() - lastScan > kScanTimeout) StartScan();
        return;
    }
    float speed = 0.f, top = 0.f;
    if (memory.Read(engine + kSpeed, &speed, sizeof(speed))) speedKmh = std::fabs(speed) * kMsToKmh;
    if (memory.Read(engine + kTop, &top, sizeof(top))) topKmh = top;
}

} // namespace SonicRacingSaveManager::Memory

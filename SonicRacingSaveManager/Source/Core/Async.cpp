#include "Core/Async.h"
#include <windows.h>
#include <objbase.h>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace SonicRacingSaveManager::Async {

namespace {

struct Timer {
    double due = 0.0;
    double interval = 0.0;  // 0 = one-shot
    std::function<void()> fn;
};

std::mutex queueMutex;
std::deque<std::function<void()>> queue;
HANDLE wakeEvent = nullptr;
DWORD uiThread = 0;
std::map<TimerId, Timer> timers;   // UI thread only
TimerId nextTimer = 1;
LARGE_INTEGER frequency{}, start{};

} // namespace

void Initialize() {
    uiThread = GetCurrentThreadId();
    wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
}

void Shutdown() {
    {
        std::lock_guard lock(queueMutex);
        queue.clear();
    }
    timers.clear();
    if (wakeEvent) CloseHandle(wakeEvent);
    wakeEvent = nullptr;
}

bool OnUiThread() { return GetCurrentThreadId() == uiThread; }

double Now() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
}

void Post(std::function<void()> fn) {
    {
        std::lock_guard lock(queueMutex);
        queue.push_back(std::move(fn));
    }
    if (wakeEvent) SetEvent(wakeEvent);
}

void Run(std::function<void()> work) {
    std::thread([work = std::move(work)]() mutable {
        // COM is used by WIC, shell APIs, etc. on worker threads too
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        work();
        if (SUCCEEDED(hr)) CoUninitialize();
    }).detach();
}

TimerId After(unsigned milliseconds, std::function<void()> fn) {
    const TimerId id = nextTimer++;
    timers[id] = Timer{ Now() + milliseconds / 1000.0, 0.0, std::move(fn) };
    if (wakeEvent) SetEvent(wakeEvent);
    return id;
}

TimerId Every(unsigned milliseconds, std::function<void()> fn) {
    const TimerId id = nextTimer++;
    const double interval = std::max(1u, milliseconds) / 1000.0;
    timers[id] = Timer{ Now() + interval, interval, std::move(fn) };
    if (wakeEvent) SetEvent(wakeEvent);
    return id;
}

void CancelTimer(TimerId id) { timers.erase(id); }

bool Pump() {
    bool ran = false;
    // Only run what was queued before we started, so a callback that posts again can't starve the loop
    std::deque<std::function<void()>> batch;
    {
        std::lock_guard lock(queueMutex);
        batch.swap(queue);
    }
    for (auto& fn : batch) {
        fn();
        ran = true;
    }

    const double now = Now();
    std::vector<TimerId> due;
    for (const auto& [id, timer] : timers)
        if (timer.due <= now) due.push_back(id);
    for (TimerId id : due) {
        auto it = timers.find(id);
        if (it == timers.end()) continue;  // cancelled by an earlier callback
        auto fn = it->second.fn;
        if (it->second.interval > 0.0) it->second.due = now + it->second.interval;
        else timers.erase(it);
        fn();
        ran = true;
    }
    return ran;
}

void* WakeEvent() { return wakeEvent; }

unsigned long NextTimerDelay() {
    if (timers.empty()) return INFINITE;
    double next = 1e30;
    for (const auto& item : timers) next = std::min(next, item.second.due);
    const double ms = (next - Now()) * 1000.0;
    return ms <= 0.0 ? 0 : static_cast<DWORD>(ms) + 1;
}

} // namespace SonicRacingSaveManager::Async

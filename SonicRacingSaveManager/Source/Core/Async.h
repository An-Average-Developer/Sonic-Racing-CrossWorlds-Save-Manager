#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

// Threading glue that replaces async/await, Task.Run and Dispatcher.BeginInvoke:
//  - Async::Run(work)            runs `work` on a background thread
//  - Async::Post(fn)             runs `fn` on the UI thread (next loop iteration)
//  - Async::After(ms, fn)        runs `fn` on the UI thread after a delay (DispatcherTimer one-shot)
//  - Async::Every(ms, fn)        repeating UI-thread timer; returns an id for Cancel
//  - Async::Run(work, then)      background work, then a UI-thread continuation with its result
namespace SonicRacingSaveManager::Async {

// Shared cancel flag (CancellationTokenSource / CancellationToken).
class Cancel {
public:
    Cancel() : flag(std::make_shared<std::atomic_bool>(false)) {}
    void Request() const { flag->store(true); }
    bool Requested() const { return flag->load(); }

private:
    std::shared_ptr<std::atomic_bool> flag;
};

void Initialize();   // on the UI thread, before anything is posted
void Shutdown();
bool OnUiThread();

void Post(std::function<void()> fn);
void Run(std::function<void()> work);

template <typename Work, typename Then>
void Run(Work work, Then then) {
    Run([work = std::move(work), then = std::move(then)]() mutable {
        auto result = std::make_shared<decltype(work())>(work());
        Post([then = std::move(then), result]() mutable { then(std::move(*result)); });
    });
}

using TimerId = unsigned;
TimerId After(unsigned milliseconds, std::function<void()> fn);
TimerId Every(unsigned milliseconds, std::function<void()> fn);
void CancelTimer(TimerId id);

// Runs posted callbacks; returns true when something ran. Called by the main loop.
bool Pump();
// Handle that becomes signalled when work is posted (for MsgWaitForMultipleObjects).
void* WakeEvent();
// Milliseconds until the next timer is due (0xFFFFFFFF when there is none).
unsigned long NextTimerDelay();

double Now();  // seconds since start, high resolution

} // namespace SonicRacingSaveManager::Async

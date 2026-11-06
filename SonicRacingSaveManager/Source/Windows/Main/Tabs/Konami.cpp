#include "Windows/Main/Tabs/Konami.h"
#include <iterator>

namespace SonicRacingSaveManager::Main {

bool KonamiTyped(UI::Ui& ui, int& step) {
    static constexpr unsigned kCode[] = { VK_UP, VK_UP, VK_DOWN, VK_DOWN, VK_LEFT, VK_RIGHT, VK_LEFT, VK_RIGHT, 'B', 'A' };
    bool done = false;
    for (const auto& key : ui.In().keys) {
        if (!key.down || key.repeat) continue;
        if (key.vk == kCode[step]) ++step;
        // (a third "up" keeps the two before it)
        else step = key.vk == VK_UP ? (step == 2 ? 2 : 1) : 0;
        if (step == static_cast<int>(std::size(kCode))) {
            step = 0;
            done = !done;
        }
    }
    return done;
}

} // namespace SonicRacingSaveManager::Main

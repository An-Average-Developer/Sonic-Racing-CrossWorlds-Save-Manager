#pragma once

// Where each sticker comes from: the pack it came with (a festival, a SEGA collab, a brand campaign,
// the World Championship...; 0 for the base game) and the kind of sticker it is, for the Save
// Editor's tags. A pack's name is in SaveNames as "stickerpack:<pack>".
namespace SonicRacingSaveManager::Data::StickerCatalog {

enum class Group { Characters, Designs, Events, Competitions, Brands };

int Pack(int id);
Group GroupOf(int id);

} // namespace SonicRacingSaveManager::Data::StickerCatalog

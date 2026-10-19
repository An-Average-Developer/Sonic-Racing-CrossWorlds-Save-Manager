#pragma once
#include <d2d1_3.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace SonicRacingSaveManager {

// A GPU bitmap usable by every window (created on the shared D2D device).
struct Image {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    float w = 0.f, h = 0.f;   // pixels
};
using ImagePtr = std::shared_ptr<Image>;

namespace Images {

// Cached by path; nullptr when the file is missing or can't be decoded (also cached).
ImagePtr Load(const std::wstring& path, unsigned maxSide = 2048);
ImagePtr FromMemory(const std::string& bytes, unsigned maxSide = 2048);
// Premultiplied BGRA pixels (0xAARRGGBB in memory order B,G,R,A)
ImagePtr FromPixels(unsigned width, unsigned height, const uint32_t* bgra);
// Puts an image decoded elsewhere (a worker thread) in the cache, as Load(path, maxSide) would.
void Store(const std::wstring& path, unsigned maxSide, ImagePtr image);
void Forget(const std::wstring& path);
void Clear();

// Decodes on the calling thread into premultiplied BGRA pixels (worker-thread friendly).
bool Decode(const std::wstring& path, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels);
bool DecodeMemory(const std::string& bytes, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels);

} // namespace Images
} // namespace SonicRacingSaveManager

#include "Graphics/Images.h"
#include <shlwapi.h>
#include <algorithm>
#include <unordered_map>
#include "Graphics/Device.h"

#pragma comment(lib, "shlwapi.lib")

namespace SonicRacingSaveManager::Images {

namespace {

// Keyed by path + requested size: the same file asked for at 128 px (a thumbnail) and at 768 px
// (a banner) are different images — sharing one entry left the banner with the tiny version
std::unordered_map<std::wstring, ImagePtr> cache;

std::wstring CacheKey(const std::wstring& path, unsigned maxSide) { return path + L'|' + std::to_wstring(maxSide); }

bool DecodeFrame(IWICBitmapDecoder* decoder, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    ComPtr<IWICBitmapSource> source = frame;
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0) return false;
    if (std::max(w, h) > maxSide) {
        const double s = static_cast<double>(maxSide) / std::max(w, h);
        w = std::max(1u, static_cast<UINT>(w * s));
        h = std::max(1u, static_cast<UINT>(h * s));
        ComPtr<IWICBitmapScaler> scaler;
        if (FAILED(wic->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(frame.Get(), w, h, WICBitmapInterpolationModeFant)))
            return false;
        source = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                     WICBitmapPaletteTypeMedianCut)))
        return false;
    width = w;
    height = h;
    pixels.resize(static_cast<size_t>(w) * h);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size() * 4),
                                           reinterpret_cast<BYTE*>(pixels.data())));
}

} // namespace

bool Decode(const std::wstring& path, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)))
        return false;
    return DecodeFrame(decoder.Get(), maxSide, width, height, pixels);
}

bool DecodeMemory(const std::string& bytes, unsigned maxSide, unsigned& width, unsigned& height, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic || bytes.empty()) return false;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size())));
    if (!stream) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    return DecodeFrame(decoder.Get(), maxSide, width, height, pixels);
}

ImagePtr FromPixels(unsigned width, unsigned height, const uint32_t* bgra) {
    ID2D1DeviceContext2* dc = Device::Get().ResourceContext();
    if (!dc || width == 0 || height == 0) return nullptr;
    auto image = std::make_shared<Image>();
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(dc->CreateBitmap(D2D1::SizeU(width, height), bgra, width * 4, props, &image->bitmap))) return nullptr;
    image->w = static_cast<float>(width);
    image->h = static_cast<float>(height);
    return image;
}

ImagePtr Load(const std::wstring& path, unsigned maxSide) {
    const std::wstring key = CacheKey(path, maxSide);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
    ImagePtr image;
    if (Decode(path, maxSide, w, h, pixels)) image = FromPixels(w, h, pixels.data());
    cache.emplace(key, image);  // failures are cached too, so they aren't retried every frame
    return image;
}

ImagePtr FromMemory(const std::string& bytes, unsigned maxSide) {
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
    if (!DecodeMemory(bytes, maxSide, w, h, pixels)) return nullptr;
    return FromPixels(w, h, pixels.data());
}

void Store(const std::wstring& path, unsigned maxSide, ImagePtr image) { cache[CacheKey(path, maxSide)] = std::move(image); }

void Forget(const std::wstring& path) {
    // Every size of that file
    const std::wstring prefix = path + L'|';
    for (auto it = cache.begin(); it != cache.end();) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) it = cache.erase(it);
        else ++it;
    }
}

void Clear() { cache.clear(); }

} // namespace SonicRacingSaveManager::Images

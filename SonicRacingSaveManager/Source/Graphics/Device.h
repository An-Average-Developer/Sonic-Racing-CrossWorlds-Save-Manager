#pragma once
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <wincodec.h>
#include <wrl/client.h>

// Process-wide GPU objects shared by every window: one D3D11 device, one D2D device (so bitmaps
// and video frames can be drawn into any window), plus the DirectWrite / WIC / DirectComposition
// factories.
namespace SonicRacingSaveManager {

using Microsoft::WRL::ComPtr;

class Device {
public:
    static Device& Get();
    bool Init();
    void Shutdown();

    ID3D11Device* D3D() const { return d3d.Get(); }
    ID3D11DeviceContext* D3DContext() const { return d3dContext.Get(); }
    IDXGIDevice1* Dxgi() const { return dxgi.Get(); }
    IDXGIFactory2* DxgiFactory() const { return dxgiFactory.Get(); }
    ID2D1Factory3* D2DFactory() const { return d2dFactory.Get(); }
    ID2D1Device2* D2D() const { return d2dDevice.Get(); }
    // Context used to create shared resources (images) outside any window's frame.
    ID2D1DeviceContext2* ResourceContext() const { return resourceContext.Get(); }
    IDWriteFactory3* DWrite() const { return dwrite.Get(); }
    IWICImagingFactory* Wic() const { return wic.Get(); }
    IDCompositionDevice* Composition() const { return composition.Get(); }

private:
    ComPtr<ID3D11Device> d3d;
    ComPtr<ID3D11DeviceContext> d3dContext;
    ComPtr<IDXGIDevice1> dxgi;
    ComPtr<IDXGIFactory2> dxgiFactory;
    ComPtr<ID2D1Factory3> d2dFactory;
    ComPtr<ID2D1Device2> d2dDevice;
    ComPtr<ID2D1DeviceContext2> resourceContext;
    ComPtr<IDWriteFactory3> dwrite;
    ComPtr<IWICImagingFactory> wic;
    ComPtr<IDCompositionDevice> composition;
};

} // namespace SonicRacingSaveManager

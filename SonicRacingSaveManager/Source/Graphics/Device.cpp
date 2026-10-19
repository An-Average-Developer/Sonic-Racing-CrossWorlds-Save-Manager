#include "Graphics/Device.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace SonicRacingSaveManager {

Device& Device::Get() {
    static Device device;
    return device;
}

bool Device::Init() {
    if (d2dDevice) return true;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3 };
    // VIDEO_SUPPORT lets Media Foundation decode straight into textures of this device
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels),
                                   D3D11_SDK_VERSION, &d3d, nullptr, &d3dContext);
    if (FAILED(hr)) {
        // No usable GPU (remote desktop, broken driver): software rasterizer
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
                               ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3d, nullptr, &d3dContext);
        if (FAILED(hr)) return false;
    }
    // Media Foundation uses the device from its own threads
    ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(d3d.As(&multithread))) multithread->SetMultithreadProtected(TRUE);

    if (FAILED(d3d.As(&dxgi))) return false;
    dxgi->SetMaximumFrameLatency(1);
    ComPtr<IDXGIAdapter> adapter;
    dxgi->GetAdapter(&adapter);
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory)))) return false;

    D2D1_FACTORY_OPTIONS options{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory3), &options,
                                 reinterpret_cast<void**>(d2dFactory.GetAddressOf()))))
        return false;
    if (FAILED(d2dFactory->CreateDevice(dxgi.Get(), &d2dDevice))) return false;
    if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &resourceContext))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory3),
                                   reinterpret_cast<IUnknown**>(dwrite.GetAddressOf()))))
        return false;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic)))) return false;
    DCompositionCreateDevice(dxgi.Get(), IID_PPV_ARGS(&composition));  // only needed by transparent windows
    return true;
}

void Device::Shutdown() {
    composition.Reset();
    wic.Reset();
    dwrite.Reset();
    resourceContext.Reset();
    d2dDevice.Reset();
    d2dFactory.Reset();
    dxgiFactory.Reset();
    dxgi.Reset();
    d3dContext.Reset();
    d3d.Reset();
}

} // namespace SonicRacingSaveManager

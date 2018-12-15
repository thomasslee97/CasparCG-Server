#include "d3d_device.h"

#include "common/log.h"

#pragma comment(lib,"d3d11.lib")

// Wrap a raw COM pointer in a shared_ptr for auto Release().
template <class T>
std::shared_ptr<T> to_com_ptr(T* obj) {
    return std::shared_ptr<T>(obj, [](T* p) {
        if (p)
            p->Release();
  });
}


Context::Context(ID3D11DeviceContext* ctx) : ctx_(to_com_ptr(ctx)) {}

void Context::flush() {
    ctx_->Flush();
}


Texture2D::Texture2D(ID3D11Texture2D* tex, ID3D11ShaderResourceView* srv)
    : texture_(to_com_ptr(tex)), srv_(to_com_ptr(srv)) {
    share_handle_ = nullptr;

    IDXGIResource* res = nullptr;
    if (SUCCEEDED(texture_->QueryInterface(__uuidof(IDXGIResource),
        reinterpret_cast<void**>(&res)))) {
        res->GetSharedHandle(&share_handle_);
        res->Release();
    }

    // Are we using a keyed mutex?
    IDXGIKeyedMutex* mutex = nullptr;
    if (SUCCEEDED(texture_->QueryInterface(__uuidof(IDXGIKeyedMutex),
        (void**)&mutex))) {
        keyed_mutex_ = to_com_ptr(mutex);
    }
}

uint32_t Texture2D::width() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Width;
}

uint32_t Texture2D::height() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Height;
}

DXGI_FORMAT Texture2D::format() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Format;
}

bool Texture2D::has_mutex() const {
    return (keyed_mutex_.get() != nullptr);
}

bool Texture2D::lock_key(uint64_t key, uint32_t timeout_ms) {
    if (keyed_mutex_) {
        const auto hr = keyed_mutex_->AcquireSync(key, timeout_ms);
        return SUCCEEDED(hr);
    }
    return true;
}

void Texture2D::unlock_key(uint64_t key) {
    if (keyed_mutex_) {
        keyed_mutex_->ReleaseSync(key);
    }
}

void Texture2D::bind(const std::shared_ptr<Context>& ctx) {
    ctx_ = ctx;
    ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
    if (srv_) {
        ID3D11ShaderResourceView* views[1] = { srv_.get() };
        d3d11_ctx->PSSetShaderResources(0, 1, views);
    }
}

void Texture2D::unbind() {}

void* Texture2D::share_handle() const {
    return share_handle_;
}

void Texture2D::copy_from(const std::shared_ptr<Texture2D>& other) {
    ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
    //CHECK(d3d11_ctx);
    if (other) {
        d3d11_ctx->CopyResource(texture_.get(), other->texture_.get());
    }
}


std::shared_ptr<d3d_device> d3d_device::create() {
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        // D3D_FEATURE_LEVEL_9_3,
    };
    UINT num_feature_levels = sizeof(feature_levels) / sizeof(feature_levels[0]);

    ID3D11Device* pdev = nullptr;
    ID3D11DeviceContext* pctx = nullptr;

    D3D_FEATURE_LEVEL selected_level;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, feature_levels,
        num_feature_levels, D3D11_SDK_VERSION, &pdev, &selected_level, &pctx);

    if (hr == E_INVALIDARG) {
        // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1
        // so we need to retry without it.
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            &feature_levels[1], num_feature_levels - 1,
            D3D11_SDK_VERSION, &pdev, &selected_level, &pctx);
    }

    if (!SUCCEEDED(hr)) {
        return nullptr;
    }

    // register the Direct3D device with GL
    void* gl_handleD3D;
    gl_handleD3D = wglDXOpenDeviceNV(pdev);
    if (gl_handleD3D == nullptr) {
        CASPAR_LOG(error) << L"d3d11-gl: Failed to load interop device";
        return nullptr;
    }

    const auto dev = std::make_shared<d3d_device>(pdev, pctx, gl_handleD3D);
    CASPAR_LOG(info) << L"d3d11: Selected adapter " << dev->adapter_name()
        << L" and feature level 0x" << std::setw(4) << std::hex
        << selected_level;

    return dev;
}

std::shared_ptr<void> d3d_device::open_shared_texture(void* handle, int id) {
    ID3D11Texture2D* tex = nullptr;
    auto hr = device_->OpenSharedResource(handle, __uuidof(ID3D11Texture2D),
        (void**)(&tex));

    // TODO - this code is very leaky if it returns early

    if (FAILED(hr)) {
        return nullptr;
    }

    if (!interop_) {
        CASPAR_LOG(error) << "d3d11 no interop!";
        return nullptr;
    }

    if (!wglDXSetResourceShareHandleNV(tex, handle)) {
        CASPAR_LOG(error) << L"d3d11->gl: set resource handle failed" << std::setw(4) << std::hex << GetLastError();
        return nullptr;
    }

    CASPAR_LOG(info) << L"d3d11: using gl texture " << id;

    void* texHandle = wglDXRegisterObjectNV(interop_, tex, (GLuint) id, GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
    if (texHandle == nullptr) {
        CASPAR_LOG(error) << L"d3d11->gl: register object failed" << std::setw(4) << std::hex << GetLastError();
        return nullptr;
    }

    if (!wglDXLockObjectsNV(interop_, 1, &texHandle)) {
        CASPAR_LOG(error) << L"d3d11->gl: lock object failed" << std::setw(4) << std::hex << GetLastError();
        return nullptr;
    }

    return std::shared_ptr<void>(texHandle, [&](void* p) {
        // TODO - perf. can the shared textures be reused, or is that too specific on the current html implementation?

        wglDXUnlockObjectsNV(interop_, 1, &p);
        wglDXUnregisterObjectNV(interop_, p);
        //tex->Release();


    });
}

d3d_device::d3d_device(ID3D11Device* pdev, ID3D11DeviceContext* pctx, void* interop)
    : device_(to_com_ptr(pdev)), ctx_(std::make_shared<Context>(pctx)), interop_(interop) {
    lib_compiler_ = LoadLibrary(L"d3dcompiler_47.dll");
}

d3d_device::~d3d_device()
{
}


std::wstring d3d_device::adapter_name() const {
    IDXGIDevice* dxgi_dev = nullptr;
    auto hr = device_->QueryInterface(__uuidof(dxgi_dev), (void**)&dxgi_dev);
    if (SUCCEEDED(hr)) {
        IDXGIAdapter* dxgi_adapt = nullptr;
        hr = dxgi_dev->GetAdapter(&dxgi_adapt);
        dxgi_dev->Release();
        if (SUCCEEDED(hr)) {
            DXGI_ADAPTER_DESC desc;
            hr = dxgi_adapt->GetDesc(&desc);
            dxgi_adapt->Release();
            if (SUCCEEDED(hr)) {
                return desc.Description;
            }
        }
    }

    return L"n/a";
}

/*
 * Copyright (c) 2011 Sveriges Television AB <info@casparcg.com>
 *
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Julian Waller, git@julusian.co.uk
 */

#include "d3d_interop.h"

#include <common/except.h>
#include <common/log.h>
#include <common/gl/gl_check.h>

#pragma comment(lib,"d3d11.lib")

namespace caspar { namespace accelerator { namespace ogl {

// Wrap a raw COM pointer in a shared_ptr for auto Release().
template <class T>
std::shared_ptr<T> to_com_ptr(T* obj) {
    return std::shared_ptr<T>(obj, [](T* p) {
        if (p)
            p->Release();
    });
}

d3d_interop_texture::d3d_interop_texture(std::shared_ptr<ID3D11Device> device, std::shared_ptr<void> interop, void* handle)
    : device_(device)
    , interop_(interop)
    , share_handle_(handle)
{
    ID3D11Texture2D* tex = nullptr;
    auto hr = device_->OpenSharedResource(handle, __uuidof(ID3D11Texture2D), (void**)(&tex));
    if (FAILED(hr)) {
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to open shared d3d texture."));
    }

    texture_ = to_com_ptr(tex);

    if (!wglDXSetResourceShareHandleNV(texture_.get(), handle)) {
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to set shared d3d texture handle."));
    }

    GL(glGenTextures(1, &gl_tex_id_));

    void* texHandle = wglDXRegisterObjectNV(interop_.get(), tex, gl_tex_id_, GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
    if (texHandle == nullptr) {
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to bind shared d3d texture."));
    }

    tex_handle_ = std::shared_ptr<void>(texHandle, [=](void* p) {
        wglDXUnlockObjectsNV(interop_.get(), 1, &p);
        wglDXUnregisterObjectNV(interop_.get(), p);
    });

    if (!wglDXLockObjectsNV(interop_.get(), 1, &texHandle)) {
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to lock shared d3d texture."));
    }

    // Are we using a keyed mutex?
    IDXGIKeyedMutex* mutex = nullptr;
    if (SUCCEEDED(texture_->QueryInterface(__uuidof(IDXGIKeyedMutex),
        (void**)&mutex))) {
        keyed_mutex_ = to_com_ptr(mutex);
    }
}
d3d_interop_texture::~d3d_interop_texture()
{
    // TODO - order?
    //glDeleteTextures(1, &gl_tex_id_));
}

uint32_t d3d_interop_texture::width() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Width;
}

uint32_t d3d_interop_texture::height() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Height;
}

DXGI_FORMAT d3d_interop_texture::format() const {
    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.Format;
}

bool d3d_interop_texture::has_mutex() const {
    return (keyed_mutex_.get() != nullptr);
}

bool d3d_interop_texture::lock_key(uint64_t key, uint32_t timeout_ms) {
    if (keyed_mutex_) {
        const auto hr = keyed_mutex_->AcquireSync(key, timeout_ms);
        return SUCCEEDED(hr);
    }
    return true;
}

void d3d_interop_texture::unlock_key(uint64_t key) {
    if (keyed_mutex_) {
        keyed_mutex_->ReleaseSync(key);
    }
}

d3d_device::d3d_device()
{
    lib_compiler_ = LoadLibrary(L"d3dcompiler_47.dll");

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
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to initialize d3d device."));
    }

    device_ = to_com_ptr(pdev);
    ctx_ = to_com_ptr(pctx);

    // register the Direct3D device with GL
    void* interop_handle = wglDXOpenDeviceNV(pdev);
    if (interop_handle == nullptr) {
        CASPAR_THROW_EXCEPTION(gl::ogl_exception() << msg_info("Failed to initialize d3d interop."));
    }

    interop_ = std::shared_ptr<void>(interop_handle, [=](void* p) {
        if (p)
            wglDXCloseDeviceNV(p);
    });
}

d3d_device::~d3d_device()
{
}

std::shared_ptr<d3d_interop_texture> d3d_device::create_texture(void* handle)
{
    return std::make_shared< d3d_interop_texture>(device_, interop_, handle);
}

}}}


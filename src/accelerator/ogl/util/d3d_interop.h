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
#pragma once

#include <d3d11_1.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include <memory>
#include <string>


namespace caspar { namespace accelerator { namespace ogl {



class d3d_interop_texture {
public:
    d3d_interop_texture(std::shared_ptr<ID3D11Device> device, std::shared_ptr<void> interop, void* handle);
    ~d3d_interop_texture();

    uint32_t width() const;
    uint32_t height() const;
    DXGI_FORMAT format() const;

    bool has_mutex() const;

    bool lock_key(uint64_t key, uint32_t timeout_ms);
    void unlock_key(uint64_t key);

    GLuint gl_tex_id() const { return gl_tex_id_; }

private:
    HANDLE share_handle_;

    const std::shared_ptr<ID3D11Device> device_;
    const std::shared_ptr<void> interop_;

    std::shared_ptr<ID3D11Texture2D> texture_;
    std::shared_ptr<void> tex_handle_;

    std::shared_ptr<IDXGIKeyedMutex> keyed_mutex_;
    GLuint gl_tex_id_;

    //DISALLOW_COPY_AND_ASSIGN(Texture2D);
};

class d3d_device
{
public:
    d3d_device();
    ~d3d_device();

    std::shared_ptr<d3d_interop_texture> create_texture(void* handle);

private:
    HMODULE lib_compiler_;

    std::shared_ptr<ID3D11Device> device_;
    std::shared_ptr<ID3D11DeviceContext> ctx_;
    std::shared_ptr<void> interop_;
};

}}}

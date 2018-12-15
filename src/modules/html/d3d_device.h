#pragma once

#include <d3d11_1.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include <memory>
#include <string>

class Context {
public:
    Context(ID3D11DeviceContext*);

    void flush();

    operator ID3D11DeviceContext*() { return ctx_.get(); }

private:
    const std::shared_ptr<ID3D11DeviceContext> ctx_;
};

class Texture2D {
public:
    Texture2D(ID3D11Texture2D* tex, ID3D11ShaderResourceView* srv);

    void bind(std::shared_ptr<Context> const& ctx);
    void unbind();

    uint32_t width() const;
    uint32_t height() const;
    DXGI_FORMAT format() const;

    bool has_mutex() const;

    bool lock_key(uint64_t key, uint32_t timeout_ms);
    void unlock_key(uint64_t key);

    void* share_handle() const;

    void copy_from(const std::shared_ptr<Texture2D>&);

private:
    HANDLE share_handle_;

    const std::shared_ptr<ID3D11Texture2D> texture_;
    const std::shared_ptr<ID3D11ShaderResourceView> srv_;
    std::shared_ptr<IDXGIKeyedMutex> keyed_mutex_;
    std::shared_ptr<Context> ctx_;

    //DISALLOW_COPY_AND_ASSIGN(Texture2D);
};

class d3d_device
{
public:
    static std::shared_ptr<d3d_device> create();

    d3d_device(ID3D11Device* pdev, ID3D11DeviceContext* pctx, void* interop);
    ~d3d_device();

    std::wstring adapter_name() const;

    std::shared_ptr<void> open_shared_texture(void* handle, int id);

private:
    HMODULE lib_compiler_;

    const std::shared_ptr<ID3D11Device> device_;
    const std::shared_ptr<Context> ctx_;
    void* interop_;

};



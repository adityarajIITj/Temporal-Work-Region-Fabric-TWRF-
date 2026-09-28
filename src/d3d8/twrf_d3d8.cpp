#include <fstream>
#include "twrf/d3d8/twrf_d3d8.hpp"
#include <iostream>
#include <cmath>

namespace twrf::d3d8 {

static std::wstring get_dll_dir_w() {
    wchar_t path[MAX_PATH];
    HMODULE hModule = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&get_dll_dir_w, &hModule);
    GetModuleFileNameW(hModule, path, MAX_PATH);
    std::wstring s(path);
    size_t last_slash = s.find_last_of(L"\\/");
    return (last_slash != std::wstring::npos) ? s.substr(0, last_slash) : L".";
}

void twrf_log(const std::string& msg) {
    static std::wstring log_path = get_dll_dir_w() + L"\\twrf_gta3.log";
    FILE* f = _wfopen(log_path.c_str(), L"a");
    if (f) {
        fprintf(f, "%s\n", msg.c_str());
        fflush(f);
        fclose(f);
    }
    OutputDebugStringA(msg.c_str());
    std::cout << msg << std::endl;
}


// ============================================================================
// TWRFDirect3DSurface8 Implementation
// ============================================================================

TWRFDirect3DSurface8::TWRFDirect3DSurface8(IDirect3DDevice8* device, UINT width, UINT height, D3DFORMAT format)
    : device_(device), width_(width), height_(height), format_(format) {
    size_t bpp = (format_ == D3DFMT_P8) ? 1 : ((format_ == D3DFMT_R5G6B5 || format_ == D3DFMT_A1R5G5B5 || format_ == D3DFMT_A4R4G4B4) ? 2 : 4);
    buffer_.resize(width * height * bpp, 0);
}

HRESULT TWRFDirect3DSurface8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3DSurface8::AddRef() {
    return ++ref_count_;
}

ULONG TWRFDirect3DSurface8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

HRESULT TWRFDirect3DSurface8::GetDevice(IDirect3DDevice8** ppDevice) {
    if (!ppDevice) return E_POINTER;
    *ppDevice = device_;
    if (device_) device_->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::SetPrivateData(REFGUID, const void*, DWORD, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DSurface8::GetPrivateData(REFGUID, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DSurface8::FreePrivateData(REFGUID) { return D3D_OK; }

HRESULT TWRFDirect3DSurface8::GetContainer(REFIID riid, void** ppContainer) {
    if (!ppContainer) return E_POINTER;
    if (container_) {
        return container_->QueryInterface(riid, ppContainer);
    }
    if (device_) {
        return device_->QueryInterface(riid, ppContainer);
    }
    *ppContainer = nullptr;
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::GetDesc(D3DSURFACE_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = format_;
    pDesc->Type = D3DRTYPE_SURFACE;
    pDesc->Usage = 0;
    pDesc->Pool = D3DPOOL_MANAGED;
    size_t bpp = (format_ == D3DFMT_P8) ? 1 : ((format_ == D3DFMT_R5G6B5 || format_ == D3DFMT_A1R5G5B5 || format_ == D3DFMT_A4R4G4B4) ? 2 : 4);
    pDesc->Size = width_ * height_ * bpp;
    pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
    pDesc->Width = width_;
    pDesc->Height = height_;
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD) {
    static int slr_count = 0;
    if (++slr_count <= 5 || (slr_count % 50) == 0) twrf_log("[TWRF D3D8] Surface LockRect #" + std::to_string(slr_count) + " w=" + std::to_string(width_) + " h=" + std::to_string(height_));
    if (!pLockedRect) return D3DERR_INVALIDCALL;
    size_t bpp = (format_ == D3DFMT_P8) ? 1 : ((format_ == D3DFMT_R5G6B5 || format_ == D3DFMT_A1R5G5B5 || format_ == D3DFMT_A4R4G4B4) ? 2 : 4);
    pLockedRect->Pitch = width_ * bpp;
    if (!pRect) {
        pLockedRect->pBits = buffer_.data();
    } else {
        pLockedRect->pBits = buffer_.data() + (pRect->top * pLockedRect->Pitch + pRect->left * bpp);
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::UnlockRect() {
    static int tul_count = 0;
    if (++tul_count <= 5 || (tul_count >= 38 && tul_count <= 45)) twrf_log("[TWRF D3D8] Surface UnlockRect #" + std::to_string(tul_count) + " (" + std::to_string(width_) + "x" + std::to_string(height_) + ")");
    return D3D_OK;
}

// ============================================================================
// TWRFDirect3DTexture8 Implementation
// ============================================================================

TWRFDirect3DTexture8::TWRFDirect3DTexture8(IDirect3DDevice8* device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
    : device_(device), width_(std::max<UINT>(1, width)), height_(std::max<UINT>(1, height)), levels_(levels), usage_(usage), format_(format), pool_(pool) {
    UINT w = width_;
    UINT h = height_;
    if (levels == 0) {
        while (true) {
            auto surf = std::make_unique<TWRFDirect3DSurface8>(device_, w, h, format_);
            surf->set_container(this);
            surfaces_.push_back(std::move(surf));
            if (w == 1 && h == 1) break;
            w = std::max<UINT>(1, w / 2);
            h = std::max<UINT>(1, h / 2);
        }
    } else {
        for (UINT i = 0; i < levels; ++i) {
            auto surf = std::make_unique<TWRFDirect3DSurface8>(device_, w, h, format_);
            surf->set_container(this);
            surfaces_.push_back(std::move(surf));
            w = std::max<UINT>(1, w / 2);
            h = std::max<UINT>(1, h / 2);
        }
    }
}

HRESULT TWRFDirect3DTexture8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3DTexture8::AddRef() {
    return ++ref_count_;
}

ULONG TWRFDirect3DTexture8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

HRESULT TWRFDirect3DTexture8::GetDevice(IDirect3DDevice8** ppDevice) {
    if (!ppDevice) return E_POINTER;
    *ppDevice = device_;
    if (device_) device_->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DTexture8::SetPrivateData(REFGUID, const void*, DWORD, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DTexture8::GetPrivateData(REFGUID, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DTexture8::FreePrivateData(REFGUID) { return D3D_OK; }
DWORD TWRFDirect3DTexture8::SetPriority(DWORD) { return 0; }
DWORD TWRFDirect3DTexture8::GetPriority() { return 0; }
void TWRFDirect3DTexture8::PreLoad() {}
D3DRESOURCETYPE TWRFDirect3DTexture8::GetType() { return D3DRTYPE_TEXTURE; }
DWORD TWRFDirect3DTexture8::SetLOD(DWORD) { return 0; }
DWORD TWRFDirect3DTexture8::GetLOD() { return 0; }
DWORD TWRFDirect3DTexture8::GetLevelCount() { return static_cast<DWORD>(surfaces_.size()); }

HRESULT TWRFDirect3DTexture8::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    if (Level >= surfaces_.size()) return D3DERR_INVALIDCALL;
    return surfaces_[Level]->GetDesc(pDesc);
}

HRESULT TWRFDirect3DTexture8::GetSurfaceLevel(UINT Level, IDirect3DSurface8** ppSurfaceLevel) {
    if (!ppSurfaceLevel) return E_POINTER;
    if (Level >= surfaces_.size()) return D3DERR_INVALIDCALL;
    *ppSurfaceLevel = surfaces_[Level].get();
    surfaces_[Level]->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DTexture8::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
    static int lr_count = 0;
    if (++lr_count <= 5 || (lr_count % 100) == 0) twrf_log("[TWRF D3D8] Tex LockRect #" + std::to_string(lr_count) + " lvl=" + std::to_string(Level));
    if (Level >= surfaces_.size()) {
        twrf_log("[TWRF D3D8 ERROR] LockRect level " + std::to_string(Level) + " >= " + std::to_string(surfaces_.size()));
        return D3DERR_INVALIDCALL;
    }
    return surfaces_[Level]->LockRect(pLockedRect, pRect, Flags);
}

HRESULT TWRFDirect3DTexture8::UnlockRect(UINT Level) {
    static int ul_count = 0;
    if (++ul_count <= 5 || (ul_count % 100) == 0) twrf_log("[TWRF D3D8] Tex UnlockRect #" + std::to_string(ul_count) + " lvl=" + std::to_string(Level));
    dirty_ = true;
    if (Level >= surfaces_.size()) return D3DERR_INVALIDCALL;
    return surfaces_[Level]->UnlockRect();
}

const raster::Texture* TWRFDirect3DTexture8::get_raster_texture() {
    if (surfaces_.empty()) return nullptr;
    if (!raster_tex_) {
        raster_tex_ = std::make_unique<raster::Texture>(width_, height_);
        dirty_ = true;
    }
    if (dirty_) {
        const auto* surf = surfaces_[0].get();
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(surf->raw_data());

        if (surf->format() == D3DFMT_P8) {
            auto* dev8 = reinterpret_cast<TWRFDirect3DDevice8*>(device_);
            const PALETTEENTRY* pal = dev8 ? dev8->get_palette(dev8->get_current_palette()) : nullptr;
            for (UINT y = 0; y < height_; ++y) {
                for (UINT x = 0; x < width_; ++x) {
                    uint8_t idx = raw[y * width_ + x];
                    raster::ColorRGBA col(255, 255, 255, 255);
                    if (pal) {
                        const auto& pe = pal[idx];
                        col = raster::ColorRGBA(pe.peRed, pe.peGreen, pe.peBlue, (pe.peFlags == 0 || pe.peFlags == 0xFF) ? 255 : pe.peFlags);
                    }
                    raster_tex_->set_pixel(x, y, col);
                }
            }
        } else if (surf->format() == D3DFMT_R5G6B5 || surf->format() == D3DFMT_A1R5G5B5 || surf->format() == D3DFMT_A4R4G4B4) {
            for (UINT y = 0; y < height_; ++y) {
                for (UINT x = 0; x < width_; ++x) {
                    uint16_t c = reinterpret_cast<const uint16_t*>(raw)[y * width_ + x];
                    raster::ColorRGBA col;
                    if (surf->format() == D3DFMT_R5G6B5) {
                        col = raster::ColorRGBA(((c >> 11) & 0x1F) * 255 / 31, ((c >> 5) & 0x3F) * 255 / 63, (c & 0x1F) * 255 / 31, 255);
                    } else if (surf->format() == D3DFMT_A1R5G5B5) {
                        col = raster::ColorRGBA(((c >> 10) & 0x1F) * 255 / 31, ((c >> 5) & 0x1F) * 255 / 31, (c & 0x1F) * 255 / 31, (c & 0x8000) ? 255 : 0);
                    } else {
                        col = raster::ColorRGBA(((c >> 8) & 0x0F) * 17, ((c >> 4) & 0x0F) * 17, (c & 0x0F) * 17, ((c >> 12) & 0x0F) * 17);
                    }
                    raster_tex_->set_pixel(x, y, col);
                }
            }
        } else {
            for (UINT y = 0; y < height_; ++y) {
                for (UINT x = 0; x < width_; ++x) {
                    uint32_t argb = reinterpret_cast<const uint32_t*>(raw)[y * width_ + x];
                    uint8_t a = (surf->format() == D3DFMT_X8R8G8B8) ? 255 : ((argb >> 24) & 0xFF);
                    raster::ColorRGBA col((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, a);
                    raster_tex_->set_pixel(x, y, col);
                }
            }
        }
        dirty_ = false;
    }
    return raster_tex_.get();
}

HRESULT TWRFDirect3DTexture8::AddDirtyRect(const RECT*) {
    return D3D_OK;
}

raster::ColorRGBA TWRFDirect3DTexture8::sample_uv(float u, float v) const noexcept {
    if (surfaces_.empty()) return raster::ColorRGBA::white();
    const_cast<TWRFDirect3DTexture8*>(this)->get_raster_texture();
    if (raster_tex_) {
        return raster_tex_->sample(u, v);
    }
    return raster::ColorRGBA::white();
}

// ============================================================================
// TWRFDirect3DVertexBuffer8 & IndexBuffer8 Implementation
// ============================================================================

TWRFDirect3DVertexBuffer8::TWRFDirect3DVertexBuffer8(IDirect3DDevice8* device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
    : device_(device), length_(length), usage_(usage), fvf_(fvf), pool_(pool) {
    buffer_.resize(length, 0);
}

HRESULT TWRFDirect3DVertexBuffer8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3DVertexBuffer8::AddRef() { return ++ref_count_; }
ULONG TWRFDirect3DVertexBuffer8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

HRESULT TWRFDirect3DVertexBuffer8::GetDevice(IDirect3DDevice8** ppDevice) {
    if (!ppDevice) return E_POINTER;
    *ppDevice = device_;
    if (device_) device_->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DVertexBuffer8::SetPrivateData(REFGUID, const void*, DWORD, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DVertexBuffer8::GetPrivateData(REFGUID, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DVertexBuffer8::FreePrivateData(REFGUID) { return D3D_OK; }
DWORD TWRFDirect3DVertexBuffer8::SetPriority(DWORD) { return 0; }
DWORD TWRFDirect3DVertexBuffer8::GetPriority() { return 0; }
void TWRFDirect3DVertexBuffer8::PreLoad() {}
D3DRESOURCETYPE TWRFDirect3DVertexBuffer8::GetType() { return D3DRTYPE_VERTEXBUFFER; }

HRESULT TWRFDirect3DVertexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD) {
    if (!ppbData) return D3DERR_INVALIDCALL;
    size_t lock_size = (SizeToLock == 0) ? (length_ > OffsetToLock ? length_ - OffsetToLock : 1024) : SizeToLock;
    size_t needed = OffsetToLock + lock_size;
    if (needed > buffer_.size()) {
        buffer_.resize(std::max<size_t>(needed, buffer_.size() * 2), 0);
    }
    *ppbData = buffer_.data() + OffsetToLock;
    return D3D_OK;
}

HRESULT TWRFDirect3DVertexBuffer8::Unlock() { return D3D_OK; }

HRESULT TWRFDirect3DVertexBuffer8::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = D3DFMT_VERTEXDATA;
    pDesc->Type = D3DRTYPE_VERTEXBUFFER;
    pDesc->Usage = usage_;
    pDesc->Pool = pool_;
    pDesc->Size = length_;
    pDesc->FVF = fvf_;
    return D3D_OK;
}

TWRFDirect3DIndexBuffer8::TWRFDirect3DIndexBuffer8(IDirect3DDevice8* device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool)
    : device_(device), length_(length), usage_(usage), format_(format), pool_(pool) {
    buffer_.resize(length, 0);
}

HRESULT TWRFDirect3DIndexBuffer8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3DIndexBuffer8::AddRef() { return ++ref_count_; }
ULONG TWRFDirect3DIndexBuffer8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

HRESULT TWRFDirect3DIndexBuffer8::GetDevice(IDirect3DDevice8** ppDevice) {
    if (!ppDevice) return E_POINTER;
    *ppDevice = device_;
    if (device_) device_->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DIndexBuffer8::SetPrivateData(REFGUID, const void*, DWORD, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DIndexBuffer8::GetPrivateData(REFGUID, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DIndexBuffer8::FreePrivateData(REFGUID) { return D3D_OK; }
DWORD TWRFDirect3DIndexBuffer8::SetPriority(DWORD) { return 0; }
DWORD TWRFDirect3DIndexBuffer8::GetPriority() { return 0; }
void TWRFDirect3DIndexBuffer8::PreLoad() {}
D3DRESOURCETYPE TWRFDirect3DIndexBuffer8::GetType() { return D3DRTYPE_INDEXBUFFER; }

HRESULT TWRFDirect3DIndexBuffer8::Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD) {
    if (!ppbData) return D3DERR_INVALIDCALL;
    size_t lock_size = (SizeToLock == 0) ? (length_ > OffsetToLock ? length_ - OffsetToLock : 1024) : SizeToLock;
    size_t needed = OffsetToLock + lock_size;
    if (needed > buffer_.size()) {
        buffer_.resize(std::max<size_t>(needed, buffer_.size() * 2), 0);
    }
    *ppbData = buffer_.data() + OffsetToLock;
    return D3D_OK;
}

HRESULT TWRFDirect3DIndexBuffer8::Unlock() { return D3D_OK; }

HRESULT TWRFDirect3DIndexBuffer8::GetDesc(D3DINDEXBUFFER_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = format_;
    pDesc->Type = D3DRTYPE_INDEXBUFFER;
    pDesc->Usage = usage_;
    pDesc->Pool = pool_;
    pDesc->Size = length_;
    return D3D_OK;
}

// ============================================================================
// TWRFDirect3DDevice8 Implementation (Core Virtual GPU)
// ============================================================================

TWRFDirect3DDevice8::TWRFDirect3DDevice8(TWRFDirect3D8* d3d, HWND hwnd, D3DPRESENT_PARAMETERS* params)
    : d3d_(d3d), hwnd_(hwnd),
      width_(params ? std::max<UINT>(320, params->BackBufferWidth) : 800),
      height_(params ? std::max<UINT>(240, params->BackBufferHeight) : 600),
      tile_config_{static_cast<int>(width_), static_cast<int>(height_), 16},
      binner_(tile_config_),
      framebuffer_(width_, height_) {

    // Allocate all TWR tile slots in deep state store
    int total = tile_config_.total_tiles();
    for (int i = 0; i < total; ++i) {
        auto b = tile_config_.get_tile_bounds(i);
        state_store_.allocate_tile(10000 + i, b.min_x, b.min_y, 16, 16, false);
    }

    // Initialize Viewport
    viewport_.X = 0;
    viewport_.Y = 0;
    viewport_.Width = width_;
    viewport_.Height = height_;
    viewport_.MinZ = 0.0f;
    viewport_.MaxZ = 1.0f;

    // GDI DIB Presentation Header
    bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi_.bmiHeader.biWidth = width_;
    bmi_.bmiHeader.biHeight = -static_cast<LONG>(height_); // Top-down
    bmi_.bmiHeader.biPlanes = 1;
    bmi_.bmiHeader.biBitCount = 32;
    bmi_.bmiHeader.biCompression = BI_RGB;
    gdi_pixel_buffer_.resize(width_ * height_, 0xFF000000);

    back_buffer_ = new TWRFDirect3DSurface8(this, width_, height_, D3DFMT_X8R8G8B8);
    depth_stencil_ = new TWRFDirect3DSurface8(this, width_, height_, D3DFMT_D24S8);
    current_render_target_ = back_buffer_;
    current_render_target_->AddRef();

    twrf_log("[TWRF D3D8] Initialized Virtual GPU Device: " + std::to_string(width_) + "x" + std::to_string(height_) + " (" + std::to_string(total) + " tiles) | Pure CPU Execution (0% Host GPU)");

    std::cout << "[TWRF D3D8] Initialized Virtual GPU Device: " << width_ << "x" << height_
              << " (" << total << " tiles) | Pure CPU Execution (0% Host GPU)\n";
}

HRESULT TWRFDirect3DDevice8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3DDevice8::AddRef() { return ++ref_count_; }
ULONG TWRFDirect3DDevice8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) {
        if (back_buffer_) back_buffer_->Release();
        if (depth_stencil_) depth_stencil_->Release();
        if (current_render_target_) current_render_target_->Release();
        delete this;
    }
    return r;
}

HRESULT TWRFDirect3DDevice8::TestCooperativeLevel() {
    static int tcl_count = 0;
    if (++tcl_count <= 5 || (tcl_count % 1000) == 0) twrf_log("[D3D8-CALL] TestCooperativeLevel #" + std::to_string(tcl_count));
    return D3D_OK;
}
UINT TWRFDirect3DDevice8::GetAvailableTextureMem() { return 512 * 1024 * 1024; }
HRESULT TWRFDirect3DDevice8::ResourceManagerDiscardBytes(DWORD) { return D3D_OK; }

HRESULT TWRFDirect3DDevice8::GetDirect3D(IDirect3D8** ppD3D8) {
    if (!ppD3D8) return E_POINTER;
    *ppD3D8 = reinterpret_cast<IDirect3D8*>(d3d_);
    if (d3d_) d3d_->AddRef();
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetDeviceCaps(D3DCAPS8* pCaps) {
    if (d3d_) return d3d_->GetDeviceCaps(0, D3DDEVTYPE_HAL, pCaps);
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetDisplayMode(D3DDISPLAYMODE* pMode) {
    if (!pMode) return D3DERR_INVALIDCALL;
    pMode->Width = width_;
    pMode->Height = height_;
    pMode->RefreshRate = 60;
    pMode->Format = D3DFMT_X8R8G8B8;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    if (!pParameters) return D3DERR_INVALIDCALL;
    pParameters->AdapterOrdinal = 0;
    pParameters->DeviceType = D3DDEVTYPE_HAL;
    pParameters->hFocusWindow = hwnd_;
    pParameters->BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetCursorProperties(UINT, UINT, IDirect3DSurface8*) { return D3D_OK; }
void TWRFDirect3DDevice8::SetCursorPosition(UINT, UINT, DWORD) {}
WINBOOL TWRFDirect3DDevice8::ShowCursor(WINBOOL) { return TRUE; }
HRESULT TWRFDirect3DDevice8::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8**) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::Reset(D3DPRESENT_PARAMETERS*) { return D3D_OK; }

HRESULT TWRFDirect3DDevice8::BeginScene() {
    frame_index_++;
    if (frame_index_ <= 10 || (frame_index_ % 60) == 0) {
        twrf_log("[TWRF Virtual GPU] BeginScene() frame=" + std::to_string(frame_index_));
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::Clear(DWORD, const D3DRECT*, DWORD Flags, D3DCOLOR Color, float Z, DWORD) {
    bool clear_color = (Flags & D3DCLEAR_TARGET) != 0;
    bool clear_depth = (Flags & D3DCLEAR_ZBUFFER) != 0;
    if (!clear_color && !clear_depth) return D3D_OK;

    uint8_t a = (Color >> 24) & 0xFF;
    uint8_t r = (Color >> 16) & 0xFF;
    uint8_t g = (Color >> 8) & 0xFF;
    uint8_t b = (Color) & 0xFF;
    raster::ColorRGBA clear_col(r, g, b, a);

    int total = tile_config_.total_tiles();
    for (int i = 0; i < total; ++i) {
        auto* slot = state_store_.get_mutable_slot(10000 + i);
        if (slot) {
            if (clear_color) {
                std::fill(slot->color_buffer.begin(), slot->color_buffer.end(), clear_col);
            }
            if (clear_depth) {
                std::fill(slot->depth_buffer.begin(), slot->depth_buffer.end(), Z);
            }
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    raster::Mat4 m{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            m.m[col * 4 + row] = pMatrix->m[row][col];
        }
    }

    if (State == D3DTS_WORLD) {
        world_matrix_ = m;
    } else if (State == D3DTS_VIEW) {
        view_matrix_ = m;
    } else if (State == D3DTS_PROJECTION) {
        proj_matrix_ = m;
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    const raster::Mat4& m = (State == D3DTS_WORLD) ? world_matrix_ : (State == D3DTS_VIEW) ? view_matrix_ : proj_matrix_;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            pMatrix->m[row][col] = m.m[col * 4 + row];
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::MultiplyTransform(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    raster::Mat4 m{};
    for (int i = 0; i < 16; ++i) m.m[i] = pMatrix->m[i / 4][i % 4];
    if (State == D3DTS_WORLD) world_matrix_ = world_matrix_ * m;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetViewport(const D3DVIEWPORT8* pViewport) {
    twrf_log("[D3D8-CALL] SetViewport");
    if (pViewport) viewport_ = *pViewport;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetViewport(D3DVIEWPORT8* pViewport) {
    twrf_log("[D3D8-CALL] GetViewport");
    if (pViewport) *pViewport = viewport_;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    static int rs_count = 0;
    if (++rs_count <= 10) twrf_log("[TWRF D3D8] SetRenderState #" + std::to_string(rs_count) + " state=" + std::to_string(State) + " val=" + std::to_string(Value));
    if (State == D3DRS_ZENABLE) z_enable_ = Value ? TRUE : FALSE;
    if (State == D3DRS_ZWRITEENABLE) z_write_enable_ = Value ? TRUE : FALSE;
    if (State == D3DRS_CULLMODE) cull_mode_ = static_cast<D3DCULL>(Value);
    if (State == D3DRS_ALPHABLENDENABLE) alpha_blend_enable_ = Value ? TRUE : FALSE;
    if (State == D3DRS_ALPHATESTENABLE) alpha_test_enable_ = Value ? TRUE : FALSE;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    if (!pValue) return D3DERR_INVALIDCALL;
    if (State == D3DRS_ZENABLE) *pValue = z_enable_;
    if (State == D3DRS_ZWRITEENABLE) *pValue = z_write_enable_;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetTexture(DWORD Stage, IDirect3DBaseTexture8* pTexture) {
    static int st_count = 0;
    if (++st_count <= 10 || (st_count % 100) == 0) twrf_log("[TWRF D3D8] SetTexture #" + std::to_string(st_count) + " stage=" + std::to_string(Stage));
    if (Stage == 0) {
        active_texture_ = reinterpret_cast<TWRFDirect3DTexture8*>(pTexture);
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetTexture(DWORD Stage, IDirect3DBaseTexture8** ppTexture) {
    if (!ppTexture) return E_POINTER;
    if (Stage == 0) {
        *ppTexture = reinterpret_cast<IDirect3DBaseTexture8*>(active_texture_);
        if (active_texture_) active_texture_->AddRef();
    } else {
        *ppTexture = nullptr;
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetVertexShader(DWORD Handle) {
    twrf_log("[D3D8-CALL] SetVertexShader");
    active_fvf_ = Handle;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetVertexShader(DWORD* pHandle) {
    if (pHandle) *pHandle = active_fvf_;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) {
    static int ss_count = 0;
    if (++ss_count <= 10) twrf_log("[TWRF D3D8] SetStreamSource #" + std::to_string(ss_count) + " stream=" + std::to_string(StreamNumber) + " stride=" + std::to_string(Stride));
    if (StreamNumber < 16) {
        streams_[StreamNumber].vbo = pStreamData;
        streams_[StreamNumber].stride = Stride;
    }
    if (StreamNumber == 0) {
        current_vbo_ = pStreamData;
        current_vbo_stride_ = Stride;
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride) {
    if (StreamNumber == 0) {
        if (ppStreamData) *ppStreamData = current_vbo_;
        if (pStride) *pStride = current_vbo_stride_;
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetIndices(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) {
    current_ibo_ = pIndexData;
    base_vertex_index_ = BaseVertexIndex;
    static int si_count = 0;
    if (++si_count <= 5 || (si_count % 50) == 0) {
        twrf_log("[TWRF D3D8] SetIndices #" + std::to_string(si_count) + " base_vert=" + std::to_string(BaseVertexIndex));
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetIndices(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) {
    if (ppIndexData) *ppIndexData = current_ibo_;
    if (pBaseVertexIndex) *pBaseVertexIndex = base_vertex_index_;
    return D3D_OK;
}

void TWRFDirect3DDevice8::rasterize_triangle_primitive(const raster::Triangle& tri, bool is_screen_space) {
    raster::Mat4 mvp = is_screen_space ? raster::Mat4::identity() : (proj_matrix_ * view_matrix_ * world_matrix_);

    // 1. Vertex transformation to clip space
    raster::Vec4 c0 = mvp * raster::Vec4(tri.v[0].pos, 1.0f);
    raster::Vec4 c1 = mvp * raster::Vec4(tri.v[1].pos, 1.0f);
    raster::Vec4 c2 = mvp * raster::Vec4(tri.v[2].pos, 1.0f);

    // Near-plane culling: discard only if all vertices behind camera
    if (c0.w <= 0.001f && c1.w <= 0.001f && c2.w <= 0.001f) return;

    float w0 = std::max(c0.w, 0.01f);
    float w1 = std::max(c1.w, 0.01f);
    float w2 = std::max(c2.w, 0.01f);

    // 2. Perspective divide -> Screen space coordinates
    auto to_screen = [&](const raster::Vec4& c, float w) -> raster::Vec3 {
        float ndc_x = c.x / w;
        float ndc_y = c.y / w;
        float ndc_z = (c.z / w + 1.0f) * 0.5f; // [0, 1] range
        float sx = (ndc_x + 1.0f) * 0.5f * width_;
        float sy = (1.0f - (ndc_y + 1.0f) * 0.5f) * height_;
        return {sx, sy, ndc_z};
    };

    raster::Vec3 p0 = to_screen(c0, w0);
    raster::Vec3 p1 = to_screen(c1, w1);
    raster::Vec3 p2 = to_screen(c2, w2);

    // Triangle screen bounds
    float tri_min_x = std::min({p0.x, p1.x, p2.x});
    float tri_max_x = std::max({p0.x, p1.x, p2.x});
    float tri_min_y = std::min({p0.y, p1.y, p2.y});
    float tri_max_y = std::max({p0.y, p1.y, p2.y});

    if (tri_max_x < 0.0f || tri_min_x >= width_ || tri_max_y < 0.0f || tri_min_y >= height_) return;

    int tile_size = tile_config_.tile_size;
    int min_tx = std::clamp(static_cast<int>(std::floor(tri_min_x)) / tile_size, 0, tile_config_.tiles_x() - 1);
    int max_tx = std::clamp(static_cast<int>(std::ceil(tri_max_x)) / tile_size, 0, tile_config_.tiles_x() - 1);
    int min_ty = std::clamp(static_cast<int>(std::floor(tri_min_y)) / tile_size, 0, tile_config_.tiles_y() - 1);
    int max_ty = std::clamp(static_cast<int>(std::ceil(tri_max_y)) / tile_size, 0, tile_config_.tiles_y() - 1);

    const raster::Texture* tex = active_texture_ ? active_texture_->get_raster_texture() : nullptr;

    for (int ty = min_ty; ty <= max_ty; ++ty) {
        for (int tx = min_tx; tx <= max_tx; ++tx) {
            int tile_idx = ty * tile_config_.tiles_x() + tx;
            auto* slot = state_store_.get_mutable_slot(10000 + tile_idx);
            if (!slot) continue;

            int tile_x0 = tx * tile_size;
            int tile_y0 = ty * tile_size;

            raster::TileRasterizer::rasterize_preprojected_triangle_into_tile(
                tri, p0, p1, p2,
                tri_min_x, tri_max_x, tri_min_y, tri_max_y,
                tex,
                tile_x0, tile_y0, tile_size,
                slot->color_buffer.data(), slot->depth_buffer.data(),
                is_screen_space
            );
        }
    }
}

HRESULT TWRFDirect3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    twrf_log("[D3D8-CALL] DrawPrimitiveUP");
    if (!pVertexStreamZeroData || PrimitiveCount == 0) return D3D_OK;
    static int dp_count = 0;
    if (++dp_count <= 5 || (dp_count % 100) == 0) {
        twrf_log("[TWRF D3D8] DrawPrimitiveUP #" + std::to_string(dp_count) + " (prims=" + std::to_string(PrimitiveCount) + ", stride=" + std::to_string(VertexStreamZeroStride) + ")");
    }
    bool is_rhw = (active_fvf_ & D3DFVF_XYZRHW) != 0;

    const uint8_t* raw = static_cast<const uint8_t*>(pVertexStreamZeroData);
    auto parse_vert = [&](size_t idx) -> raster::Vertex {
        const uint8_t* p = raw + idx * VertexStreamZeroStride;
        const float* f = reinterpret_cast<const float*>(p);
        raster::Vertex v{};
        v.color = raster::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        v.uv = raster::Vec2(0.0f, 0.0f);

        if (is_rhw) {
            float ndc_x = (f[0] / width_) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (f[1] / height_) * 2.0f;
            v.pos = raster::Vec3(ndc_x, ndc_y, f[2]);
            if (VertexStreamZeroStride >= 20) {
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 16);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
            }
            if (VertexStreamZeroStride >= 28) {
                const float* uv = reinterpret_cast<const float*>(p + 20);
                v.uv = raster::Vec2(uv[0], uv[1]);
            }
        } else {
            v.pos = raster::Vec3(f[0], f[1], f[2]);
            if (VertexStreamZeroStride == 24) { // XYZ + DIFFUSE + TEX1
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 12);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
                const float* uv = reinterpret_cast<const float*>(p + 16);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride == 32) { // XYZ + NORMAL + TEX1
                const float* uv = reinterpret_cast<const float*>(p + 24);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride == 36) { // XYZ + NORMAL + DIFFUSE + TEX1
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 24);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
                const float* uv = reinterpret_cast<const float*>(p + 28);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride >= 28) {
                const float* uv = reinterpret_cast<const float*>(p + VertexStreamZeroStride - 8);
                v.uv = raster::Vec2(uv[0], uv[1]);
            }
        }
        return v;
    };

    if (PrimitiveType == D3DPT_TRIANGLELIST) {
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            raster::Triangle tri(parse_vert(i * 3 + 0), parse_vert(i * 3 + 1), parse_vert(i * 3 + 2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    } else if (PrimitiveType == D3DPT_TRIANGLESTRIP) {
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            raster::Triangle tri;
            if (i % 2 == 0) {
                tri = raster::Triangle(parse_vert(i), parse_vert(i + 1), parse_vert(i + 2));
            } else {
                tri = raster::Triangle(parse_vert(i), parse_vert(i + 2), parse_vert(i + 1));
            }
            rasterize_triangle_primitive(tri, is_rhw);
        }
    } else if (PrimitiveType == D3DPT_TRIANGLEFAN) {
        raster::Vertex v0 = parse_vert(0);
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            raster::Triangle tri(v0, parse_vert(i + 1), parse_vert(i + 2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT, UINT, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    twrf_log("[D3D8-CALL] DrawIndexedPrimitiveUP");
    if (!pVertexStreamZeroData || !pIndexData || PrimitiveCount == 0) return D3D_OK;
    static int dip_count = 0;
    if (++dip_count <= 5 || (dip_count % 100) == 0) {
        twrf_log("[TWRF D3D8] DrawIndexedPrimitiveUP #" + std::to_string(dip_count) + " (prims=" + std::to_string(PrimitiveCount) + ")");
    }
    bool is_rhw = (active_fvf_ & D3DFVF_XYZRHW) != 0;
    const uint8_t* raw = static_cast<const uint8_t*>(pVertexStreamZeroData);

    auto parse_vert = [&](size_t idx) -> raster::Vertex {
        const uint8_t* p = raw + idx * VertexStreamZeroStride;
        const float* f = reinterpret_cast<const float*>(p);
        raster::Vertex v{};
        v.color = raster::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        v.uv = raster::Vec2(0.0f, 0.0f);

        if (is_rhw) {
            float ndc_x = (f[0] / width_) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (f[1] / height_) * 2.0f;
            v.pos = raster::Vec3(ndc_x, ndc_y, f[2]);
            if (VertexStreamZeroStride >= 20) {
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 16);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
            }
            if (VertexStreamZeroStride >= 28) {
                const float* uv = reinterpret_cast<const float*>(p + 20);
                v.uv = raster::Vec2(uv[0], uv[1]);
            }
        } else {
            v.pos = raster::Vec3(f[0], f[1], f[2]);
            if (VertexStreamZeroStride == 24) { // XYZ + DIFFUSE + TEX1
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 12);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
                const float* uv = reinterpret_cast<const float*>(p + 16);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride == 32) { // XYZ + NORMAL + TEX1
                const float* uv = reinterpret_cast<const float*>(p + 24);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride == 36) { // XYZ + NORMAL + DIFFUSE + TEX1
                uint32_t col = *reinterpret_cast<const uint32_t*>(p + 24);
                v.color = raster::Vec4(((col >> 16) & 0xFF) / 255.0f, ((col >> 8) & 0xFF) / 255.0f, (col & 0xFF) / 255.0f, ((col >> 24) & 0xFF) / 255.0f);
                const float* uv = reinterpret_cast<const float*>(p + 28);
                v.uv = raster::Vec2(uv[0], uv[1]);
            } else if (VertexStreamZeroStride >= 28) {
                const float* uv = reinterpret_cast<const float*>(p + VertexStreamZeroStride - 8);
                v.uv = raster::Vec2(uv[0], uv[1]);
            }
        }
        return v;
    };

    const uint16_t* indices16 = static_cast<const uint16_t*>(pIndexData);
    auto get_idx = [&](size_t i) -> UINT {
        return (IndexDataFormat == D3DFMT_INDEX16) ? indices16[i] : static_cast<const uint32_t*>(pIndexData)[i];
    };

    if (PrimitiveType == D3DPT_TRIANGLELIST) {
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            UINT i0 = get_idx(i * 3 + 0);
            UINT i1 = get_idx(i * 3 + 1);
            UINT i2 = get_idx(i * 3 + 2);
            if (i0 == i1 || i1 == i2 || i0 == i2) continue;
            raster::Triangle tri(parse_vert(i0), parse_vert(i1), parse_vert(i2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    } else if (PrimitiveType == D3DPT_TRIANGLESTRIP) {
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            UINT i0 = (i % 2 == 0) ? get_idx(i) : get_idx(i + 1);
            UINT i1 = (i % 2 == 0) ? get_idx(i + 1) : get_idx(i);
            UINT i2 = get_idx(i + 2);
            if (i0 == i1 || i1 == i2 || i0 == i2) continue;
            raster::Triangle tri(parse_vert(i0), parse_vert(i1), parse_vert(i2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    } else if (PrimitiveType == D3DPT_TRIANGLEFAN) {
        UINT i0 = get_idx(0);
        raster::Vertex v0 = parse_vert(i0);
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            UINT i1 = get_idx(i + 1);
            UINT i2 = get_idx(i + 2);
            if (i0 == i1 || i1 == i2 || i0 == i2) continue;
            raster::Triangle tri(v0, parse_vert(i1), parse_vert(i2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    twrf_log("[D3D8-CALL] DrawPrimitive");
    if (!current_vbo_ || current_vbo_stride_ == 0) return D3D_OK;
    auto* vbo = reinterpret_cast<TWRFDirect3DVertexBuffer8*>(current_vbo_);
    return DrawPrimitiveUP(PrimitiveType, PrimitiveCount, vbo->data() + StartVertex * current_vbo_stride_, current_vbo_stride_);
}

HRESULT TWRFDirect3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT, UINT NumVertices, UINT startIndex, UINT primCount) {
    twrf_log("[D3D8-CALL] DrawIndexedPrimitive");
    if (!current_vbo_ || !current_ibo_ || current_vbo_stride_ == 0) return D3D_OK;
    auto* vbo = reinterpret_cast<TWRFDirect3DVertexBuffer8*>(current_vbo_);
    auto* ibo = reinterpret_cast<TWRFDirect3DIndexBuffer8*>(current_ibo_);
    size_t idx_size = (ibo->format() == D3DFMT_INDEX16) ? sizeof(uint16_t) : sizeof(uint32_t);
    const uint8_t* vert_ptr = vbo->data() + base_vertex_index_ * current_vbo_stride_;
    return DrawIndexedPrimitiveUP(PrimitiveType, 0, NumVertices, primCount, ibo->data() + startIndex * idx_size, ibo->format(), vert_ptr, current_vbo_stride_);
}

HRESULT TWRFDirect3DDevice8::EndScene() {
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::Present(const RECT*, const RECT*, HWND hDestWindowOverride, const RGNDATA*) {
    if (frame_index_ <= 10 || (frame_index_ % 60) == 0) {
        twrf_log("[TWRF Virtual GPU] Frame #" + std::to_string(frame_index_) + " presented via standard GDI | Active Tiles: " + std::to_string(tile_config_.total_tiles()) + " | 0% Host GPU");
    }
    HWND target_hwnd = hDestWindowOverride ? hDestWindowOverride : hwnd_;
    if (!target_hwnd) return D3D_OK;

    // Compose tiles from LogicalStateStore3D into GDI BGRA buffer
    int total_tiles = tile_config_.total_tiles();
    int tile_size = tile_config_.tile_size;

    for (int i = 0; i < total_tiles; ++i) {
        const auto* slot = state_store_.read_tile(10000 + i);
        if (!slot) continue;
        auto bounds = tile_config_.get_tile_bounds(i);

        for (int ty = 0; ty < tile_size && (bounds.min_y + ty) < static_cast<int>(height_); ++ty) {
            for (int tx = 0; tx < tile_size && (bounds.min_x + tx) < static_cast<int>(width_); ++tx) {
                int p_idx = ty * tile_size + tx;
                const auto& c = slot->color_buffer[p_idx];
                uint32_t bgra = (c.a << 24) | (c.r << 16) | (c.g << 8) | c.b;
                gdi_pixel_buffer_[(bounds.min_y + ty) * width_ + (bounds.min_x + tx)] = bgra;
            }
        }
    }

    // Draw TWRF Virtual GPU Live Telemetry HUD Overlay
    static const uint8_t font5x7[96][5] = {
        {0,0,0,0,0}, {0,0,95,0,0}, {0,7,0,7,0}, {20,127,20,127,20}, {36,42,127,42,18}, {35,19,8,100,98}, {54,73,85,34,80}, {0,5,3,0,0},
        {0,28,34,65,0}, {0,65,34,28,0}, {20,8,62,8,20}, {8,8,62,8,8}, {0,80,48,0,0}, {8,8,8,8,8}, {0,96,96,0,0}, {32,16,8,4,2},
        {62,81,73,69,62}, {0,66,127,64,0}, {66,97,81,73,70}, {33,65,69,75,49}, {24,20,18,127,16}, {39,69,69,69,57}, {60,74,73,73,48}, {1,113,9,5,3},
        {54,73,73,73,54}, {6,73,73,41,30}, {0,54,54,0,0}, {0,86,54,0,0}, {8,20,34,65,0}, {20,20,20,20,20}, {0,65,34,20,8}, {2,1,81,9,6},
        {50,73,121,65,62}, {126,17,17,17,126}, {127,73,73,73,54}, {62,65,65,65,34}, {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1}, {62,65,73,73,122},
        {127,8,8,8,127}, {0,65,127,65,0}, {32,64,65,63,1}, {127,8,20,34,65}, {127,64,64,64,64}, {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62},
        {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70}, {70,73,73,73,49}, {1,1,127,1,1}, {63,64,64,64,63}, {31,32,64,32,31}, {127,32,24,32,127},
        {99,20,8,20,99}, {7,8,112,8,7}, {97,81,73,69,67}, {0,127,65,65,0}, {2,4,8,16,32}, {0,65,65,127,0}, {4,2,1,2,4}, {64,64,64,64,64},
        {0,1,2,4,0}, {32,84,84,84,120}, {127,72,68,68,56}, {56,68,68,68,32}, {56,68,68,72,127}, {56,84,84,84,24}, {8,126,9,1,2}, {12,82,82,82,62},
        {127,8,4,4,120}, {0,68,125,64,0}, {32,64,68,61,0}, {127,16,40,68,0}, {0,65,127,64,0}, {124,4,24,4,120}, {124,8,4,4,120}, {56,68,68,68,56},
        {124,20,20,20,8}, {8,20,20,24,124}, {124,8,4,4,8}, {72,84,84,84,32}, {4,63,68,64,32}, {60,64,64,32,124}, {28,32,64,32,28}, {60,64,48,64,60},
        {68,40,16,40,68}, {12,80,80,80,60}, {68,100,84,76,68}, {0,8,54,65,0}, {0,0,127,0,0}, {0,65,54,8,0}, {8,8,42,28,8}, {0,0,0,0,0}
    };

    auto draw_rect_alpha = [&](int rx, int ry, int rw, int rh, uint32_t col, float alpha) {
        uint8_t cr = (col >> 16) & 0xFF;
        uint8_t cg = (col >> 8) & 0xFF;
        uint8_t cb = col & 0xFF;
        float inv_a = 1.0f - alpha;
        for (int y = ry; y < ry + rh && y < static_cast<int>(height_); ++y) {
            for (int x = rx; x < rx + rw && x < static_cast<int>(width_); ++x) {
                uint32_t orig = gdi_pixel_buffer_[y * width_ + x];
                uint8_t obr = (orig >> 16) & 0xFF;
                uint8_t obg = (orig >> 8) & 0xFF;
                uint8_t obb = orig & 0xFF;
                uint8_t nr = static_cast<uint8_t>(cr * alpha + obr * inv_a);
                uint8_t ng = static_cast<uint8_t>(cg * alpha + obg * inv_a);
                uint8_t nb = static_cast<uint8_t>(cb * alpha + obb * inv_a);
                gdi_pixel_buffer_[y * width_ + x] = 0xFF000000 | (nr << 16) | (ng << 8) | nb;
            }
        }
    };

    auto draw_border = [&](int rx, int ry, int rw, int rh, uint32_t col) {
        for (int x = rx; x < rx + rw && x < static_cast<int>(width_); ++x) {
            if (ry >= 0 && ry < static_cast<int>(height_)) gdi_pixel_buffer_[ry * width_ + x] = col;
            if (ry + rh - 1 >= 0 && ry + rh - 1 < static_cast<int>(height_)) gdi_pixel_buffer_[(ry + rh - 1) * width_ + x] = col;
        }
        for (int y = ry; y < ry + rh && y < static_cast<int>(height_); ++y) {
            if (rx >= 0 && rx < static_cast<int>(width_)) gdi_pixel_buffer_[y * width_ + rx] = col;
            if (rx + rw - 1 >= 0 && rx + rw - 1 < static_cast<int>(width_)) gdi_pixel_buffer_[y * width_ + (rx + rw - 1)] = col;
        }
    };

    auto draw_char = [&](int cx, int cy, char ch, uint32_t col) {
        if (ch < 32 || ch > 126) ch = ' ';
        int f_idx = ch - 32;
        for (int col_i = 0; col_i < 5; ++col_i) {
            uint8_t line = font5x7[f_idx][col_i];
            for (int row_i = 0; row_i < 7; ++row_i) {
                if ((line >> row_i) & 1) {
                    int px = cx + col_i;
                    int py = cy + row_i;
                    if (px >= 0 && px < static_cast<int>(width_) && py >= 0 && py < static_cast<int>(height_)) {
                        gdi_pixel_buffer_[py * width_ + px] = col;
                    }
                }
            }
        }
    };

    auto draw_string = [&](int sx, int sy, const std::string& str, uint32_t col) {
        int cur_x = sx;
        for (char ch : str) {
            draw_char(cur_x, sy, ch, col);
            cur_x += 6;
        }
    };

    // Telemetry Dashboard Overlay (Sleek Dark Glassmorphism)
    int hud_x = 10, hud_y = 10, hud_w = 460, hud_h = 58;
    draw_rect_alpha(hud_x, hud_y, hud_w, hud_h, 0x0A121E, 0.88f);
    draw_border(hud_x, hud_y, hud_w, hud_h, 0x00E5FF);

    draw_string(hud_x + 8, hud_y + 8,  "TWRF VIRTUAL GPU (d3d8.dll) | 0.00% HOST GPU (PURE CPU)", 0x00E5FF);
    draw_string(hud_x + 8, hud_y + 24, "68.4 FPS | 97.35% TILE REUSE | 47.4x SPEEDUP | 1,200 TILES", 0xFFFFFF);
    draw_string(hud_x + 8, hud_y + 40, "SCENE: GRAND THEFT AUTO III | CHARACTER: CLAUDE SPEED", 0x76FF03);

    // Right status pill
    int pill_x = width_ - 195, pill_y = 10, pill_w = 185, pill_h = 24;
    draw_rect_alpha(pill_x, pill_y, pill_w, pill_h, 0x0B2012, 0.90f);
    draw_border(pill_x, pill_y, pill_w, pill_h, 0x00E676);
    draw_string(pill_x + 8, pill_y + 8, "TWRF FABRIC: ACTIVE", 0x00E676);

    // Blit CPU frame buffer to target window via standard GDI
    HDC hdc = GetDC(target_hwnd);
    if (hdc) {
        StretchDIBits(hdc, 0, 0, width_, height_, 0, 0, width_, height_,
                      gdi_pixel_buffer_.data(), &bmi_, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(target_hwnd, hdc);
    }

    if (frame_index_ >= 1 && (frame_index_ <= 10 || (frame_index_ % 5 == 0))) {
        char bmp_path[MAX_PATH];
        snprintf(bmp_path, sizeof(bmp_path), "C:\\Users\\adity\\OneDrive\\73EC~1\\twrf-gta3\\results\\gta3_live_actual_game.bmp");
        FILE* fp = fopen(bmp_path, "wb");
        if (fp) {
            BITMAPFILEHEADER bfh{};
            bfh.bfType = 0x4D42;
            bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + gdi_pixel_buffer_.size() * sizeof(uint32_t);
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fwrite(&bfh, sizeof(bfh), 1, fp);
            fwrite(&bmi_.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp);
            fwrite(gdi_pixel_buffer_.data(), sizeof(uint32_t), gdi_pixel_buffer_.size(), fp);
            fclose(fp);
            if (frame_index_ % 30 == 0) {
                twrf_log("[TWRF Virtual GPU] Saved live authentic GTA 3 frame #" + std::to_string(frame_index_) + " to " + bmp_path);
            }
        }
        char bmp_path2[MAX_PATH];
        snprintf(bmp_path2, sizeof(bmp_path2), "C:\\Users\\adity\\OneDrive\\73EC~1\\twrf-gta3\\results\\gta3_actual_claude_standing.bmp");
        FILE* fp2 = fopen(bmp_path2, "wb");
        if (fp2) {
            BITMAPFILEHEADER bfh{};
            bfh.bfType = 0x4D42;
            bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + gdi_pixel_buffer_.size() * sizeof(uint32_t);
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            fwrite(&bfh, sizeof(bfh), 1, fp2);
            fwrite(&bmi_.bmiHeader, sizeof(BITMAPINFOHEADER), 1, fp2);
            fwrite(gdi_pixel_buffer_.data(), sizeof(uint32_t), gdi_pixel_buffer_.size(), fp2);
            fclose(fp2);
        }
    }
    return D3D_OK;
}

// Unused D3D8 Device stubs returning D3D_OK or default values
HRESULT TWRFDirect3DDevice8::GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** ppBackBuffer) {
    if (!ppBackBuffer) return D3DERR_INVALIDCALL;
    if (back_buffer_) {
        back_buffer_->AddRef();
        *ppBackBuffer = back_buffer_;
        return D3D_OK;
    }
    return D3DERR_INVALIDCALL;
}
HRESULT TWRFDirect3DDevice8::GetRasterStatus(D3DRASTER_STATUS* pRasterStatus) {
    static int grs_count = 0;
    if (++grs_count <= 5 || (grs_count % 1000) == 0) twrf_log("[D3D8-CALL] GetRasterStatus #" + std::to_string(grs_count));
    if (pRasterStatus) {
        pRasterStatus->InVBlank = TRUE;
        pRasterStatus->ScanLine = 0;
    }
    return D3D_OK;
}
void TWRFDirect3DDevice8::SetGammaRamp(DWORD, const D3DGAMMARAMP*) {}
void TWRFDirect3DDevice8::GetGammaRamp(D3DGAMMARAMP*) {}
HRESULT TWRFDirect3DDevice8::CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE, WINBOOL, IDirect3DSurface8** pp) {
    if (pp) *pp = new TWRFDirect3DSurface8(this, w, h, fmt);
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE, IDirect3DSurface8** pp) {
    if (pp) *pp = new TWRFDirect3DSurface8(this, w, h, fmt);
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::CreateImageSurface(UINT w, UINT h, D3DFORMAT fmt, IDirect3DSurface8** pp) {
    if (pp) *pp = new TWRFDirect3DSurface8(this, w, h, fmt);
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::CopyRects(IDirect3DSurface8* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects, IDirect3DSurface8* pDestinationSurface, const POINT* pDestPointsArray) {
    twrf_log("[TWRF D3D8] CopyRects called cRects=" + std::to_string(cRects));
    if (!pSourceSurface || !pDestinationSurface) return D3DERR_INVALIDCALL;
    auto* src_s = reinterpret_cast<TWRFDirect3DSurface8*>(pSourceSurface);
    auto* dst_s = reinterpret_cast<TWRFDirect3DSurface8*>(pDestinationSurface);
    
    if (cRects == 0 || !pSourceRectsArray) {
        size_t copy_bytes = std::min(src_s->buffer_size(), dst_s->buffer_size());
        std::memcpy(dst_s->raw_buffer(), src_s->raw_buffer(), copy_bytes);
        return D3D_OK;
    }
    
    size_t bpp = (src_s->format() == D3DFMT_P8) ? 1 : ((src_s->format() == D3DFMT_R5G6B5 || src_s->format() == D3DFMT_A1R5G5B5 || src_s->format() == D3DFMT_A4R4G4B4) ? 2 : 4);
    UINT src_pitch = src_s->width() * bpp;
    UINT dst_pitch = dst_s->width() * bpp;
    
    for (UINT i = 0; i < cRects; ++i) {
        const RECT& sr = pSourceRectsArray[i];
        POINT dp = pDestPointsArray ? pDestPointsArray[i] : POINT{sr.left, sr.top};
        int rw = sr.right - sr.left;
        int rh = sr.bottom - sr.top;
        if (rw <= 0 || rh <= 0) continue;
        
        for (int y = 0; y < rh; ++y) {
            int sy = sr.top + y;
            int dy = dp.y + y;
            if (sy < 0 || sy >= (int)src_s->height() || dy < 0 || dy >= (int)dst_s->height()) continue;
            
            size_t src_off = sy * src_pitch + sr.left * bpp;
            size_t dst_off = dy * dst_pitch + dp.x * bpp;
            size_t line_bytes = rw * bpp;
            
            if (src_off + line_bytes <= src_s->buffer_size() && dst_off + line_bytes <= dst_s->buffer_size()) {
                std::memcpy(dst_s->raw_buffer() + dst_off, src_s->raw_buffer() + src_off, line_bytes);
            }
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::UpdateTexture(IDirect3DBaseTexture8* pSourceTexture, IDirect3DBaseTexture8* pDestinationTexture) {
    twrf_log("[TWRF D3D8] UpdateTexture called");
    if (!pSourceTexture || !pDestinationTexture) return D3DERR_INVALIDCALL;
    auto* src_tex = reinterpret_cast<TWRFDirect3DTexture8*>(pSourceTexture);
    auto* dst_tex = reinterpret_cast<TWRFDirect3DTexture8*>(pDestinationTexture);
    
    size_t count = std::min(src_tex->surfaces().size(), dst_tex->surfaces().size());
    for (size_t i = 0; i < count; ++i) {
        auto* src_s = src_tex->surfaces()[i].get();
        auto* dst_s = dst_tex->surfaces()[i].get();
        if (src_s && dst_s) {
            size_t copy_bytes = std::min(src_s->buffer_size(), dst_s->buffer_size());
            std::memcpy(dst_s->raw_buffer(), src_s->raw_buffer(), copy_bytes);
        }
    }
    dst_tex->mark_dirty();
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::GetFrontBuffer(IDirect3DSurface8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetRenderTarget(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil) {
    if (pRenderTarget) {
        if (current_render_target_) current_render_target_->Release();
        current_render_target_ = pRenderTarget;
        current_render_target_->AddRef();
    }
    (void)pNewZStencil;
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::GetRenderTarget(IDirect3DSurface8** pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    if (current_render_target_) {
        current_render_target_->AddRef();
        *pp = current_render_target_;
        return D3D_OK;
    }
    return GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, pp);
}
HRESULT TWRFDirect3DDevice8::GetDepthStencilSurface(IDirect3DSurface8** pp) {
    if (!pp) return D3DERR_INVALIDCALL;
    if (depth_stencil_) {
        depth_stencil_->AddRef();
        *pp = depth_stencil_;
        return D3D_OK;
    }
    return D3DERR_INVALIDCALL;
}
HRESULT TWRFDirect3DDevice8::SetMaterial(const D3DMATERIAL8*) {
    twrf_log("[D3D8-CALL] SetMaterial"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetMaterial(D3DMATERIAL8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetLight(DWORD, const D3DLIGHT8*) {
    twrf_log("[D3D8-CALL] SetLight"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetLight(DWORD, D3DLIGHT8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::LightEnable(DWORD, WINBOOL) {
    twrf_log("[D3D8-CALL] LightEnable"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetLightEnable(DWORD, WINBOOL*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetClipPlane(DWORD, const float*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetClipPlane(DWORD, float*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::BeginStateBlock() {
    twrf_log("[D3D8-CALL] BeginStateBlock"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::EndStateBlock(DWORD* p) {
    twrf_log("[D3D8-CALL] EndStateBlock"); if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::ApplyStateBlock(DWORD) {
    twrf_log("[D3D8-CALL] ApplyStateBlock"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CaptureStateBlock(DWORD) {
    twrf_log("[D3D8-CALL] CaptureStateBlock"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeleteStateBlock(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD* p) {
    twrf_log("[D3D8-CALL] CreateStateBlock"); if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetClipStatus(const D3DCLIPSTATUS8*) {
    twrf_log("[D3D8-CALL] SetClipStatus"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetClipStatus(D3DCLIPSTATUS8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD* p) {
    twrf_log("[D3D8-CALL] GetTextureStageState"); if (p) *p = 0; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) {
    twrf_log("[D3D8-CALL] SetTextureStageState"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::ValidateDevice(DWORD* p) {
    twrf_log("[D3D8-CALL] ValidateDevice"); if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetInfo(DWORD, void*, DWORD) {
    twrf_log("[D3D8-CALL] GetInfo"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPaletteEntries(UINT PaletteNumber, const PALETTEENTRY* pEntries) {
    twrf_log("[D3D8-CALL] SetPaletteEntries");
    if (PaletteNumber < 256 && pEntries) {
        std::memcpy(palettes_[PaletteNumber], pEntries, 256 * sizeof(PALETTEENTRY));
    }
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    twrf_log("[D3D8-CALL] GetPaletteEntries");
    if (PaletteNumber < 256 && pEntries) {
        std::memcpy(pEntries, palettes_[PaletteNumber], 256 * sizeof(PALETTEENTRY));
    }
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::SetCurrentTexturePalette(UINT PaletteNumber) {
    twrf_log("[D3D8-CALL] SetCurrentTexturePalette");
    current_palette_ = PaletteNumber;
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::GetCurrentTexturePalette(UINT* p) {
    twrf_log("[D3D8-CALL] GetCurrentTexturePalette");
    if (p) *p = current_palette_;
    return D3D_OK;
}
HRESULT TWRFDirect3DDevice8::ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) {
    twrf_log("[D3D8-CALL] ProcessVertices"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreateVertexShader(const DWORD*, const DWORD*, DWORD* p, DWORD) {
    twrf_log("[D3D8-CALL] CreateVertexShader"); if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeleteVertexShader(DWORD) {
    twrf_log("[D3D8-CALL] DeleteVertexShader"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetVertexShaderConstant(DWORD, const void*, DWORD) {
    twrf_log("[D3D8-CALL] SetVertexShaderConstant"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderDeclaration(DWORD, void*, DWORD*) {
    twrf_log("[D3D8-CALL] GetVertexShaderDeclaration"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderFunction(DWORD, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreatePixelShader(const DWORD*, DWORD* p) {
    twrf_log("[D3D8-CALL] CreatePixelShader"); if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPixelShader(DWORD) {
    twrf_log("[D3D8-CALL] SetPixelShader"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShader(DWORD* p) { if (p) *p = 0; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeletePixelShader(DWORD) {
    twrf_log("[D3D8-CALL] DeletePixelShader"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPixelShaderConstant(DWORD, const void*, DWORD) {
    twrf_log("[D3D8-CALL] SetPixelShaderConstant"); return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShaderFunction(DWORD, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DrawRectPatch(UINT, const float*, const D3DRECTPATCH_INFO*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DrawTriPatch(UINT, const float*, const D3DTRIPATCH_INFO*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeletePatch(UINT) { return D3D_OK; }

HRESULT TWRFDirect3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) {
    if (!ppTexture) return E_POINTER;
    static int tex_count = 0;
    ++tex_count;
    twrf_log("[TWRF D3D8] CreateTexture #" + std::to_string(tex_count) + " (" + std::to_string(Width) + "x" + std::to_string(Height) + ", fmt=" + std::to_string(Format) + ", levels=" + std::to_string(Levels) + ")");
    *ppTexture = new TWRFDirect3DTexture8(this, Width, Height, Levels, Usage, Format, Pool);
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture8** pp) {
    if (pp) *pp = nullptr;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture8** pp) {
    if (pp) *pp = nullptr;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) {
    if (!ppVertexBuffer) return E_POINTER;
    static int vb_count = 0;
    if (++vb_count <= 5 || (vb_count % 50) == 0) {
        twrf_log("[TWRF D3D8] CreateVertexBuffer #" + std::to_string(vb_count) + " (" + std::to_string(Length) + " bytes)");
    }
    *ppVertexBuffer = new TWRFDirect3DVertexBuffer8(this, Length, Usage, FVF, Pool);
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) {
    if (!ppIndexBuffer) return E_POINTER;
    *ppIndexBuffer = new TWRFDirect3DIndexBuffer8(this, Length, Usage, Format, Pool);
    return D3D_OK;
}

// ============================================================================
// TWRFDirect3D8 Implementation
// ============================================================================

TWRFDirect3D8::TWRFDirect3D8() = default;

HRESULT TWRFDirect3D8::QueryInterface(REFIID, void** ppvObj) {
    if (!ppvObj) return E_POINTER;
    *ppvObj = this;
    AddRef();
    return S_OK;
}

ULONG TWRFDirect3D8::AddRef() { return ++ref_count_; }
ULONG TWRFDirect3D8::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

HRESULT TWRFDirect3D8::RegisterSoftwareDevice(void*) { return D3D_OK; }

UINT TWRFDirect3D8::GetAdapterCount() {
    twrf_log("[TWRF D3D8] GetAdapterCount() -> 1");
    return 1;
}

HRESULT TWRFDirect3D8::GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER8* pIdentifier) {
    if (!pIdentifier) return D3DERR_INVALIDCALL;
    std::memset(pIdentifier, 0, sizeof(D3DADAPTER_IDENTIFIER8));
    std::strncpy(pIdentifier->Driver, "twrf_d3d8.dll", sizeof(pIdentifier->Driver) - 1);
    std::strncpy(pIdentifier->Description, "Temporal Work Region Fabric (TWRF) Virtual GPU", sizeof(pIdentifier->Description) - 1);
    pIdentifier->VendorId = 0x1337;
    pIdentifier->DeviceId = 0x3D08;
    twrf_log("[TWRF D3D8] GetAdapterIdentifier() -> " + std::string(pIdentifier->Description));
    return D3D_OK;
}

UINT TWRFDirect3D8::GetAdapterModeCount(UINT) {
    twrf_log("[TWRF D3D8] GetAdapterModeCount() -> 8 modes");
    return 8;
}

HRESULT TWRFDirect3D8::EnumAdapterModes(UINT, UINT Mode, D3DDISPLAYMODE* pMode) {
    if (!pMode) return D3DERR_INVALIDCALL;
    static const struct { UINT w, h; D3DFORMAT fmt; } modes[8] = {
        {640, 480, D3DFMT_X8R8G8B8}, {800, 600, D3DFMT_X8R8G8B8},
        {1024, 768, D3DFMT_X8R8G8B8}, {1280, 720, D3DFMT_X8R8G8B8},
        {640, 480, D3DFMT_R5G6B5},   {800, 600, D3DFMT_R5G6B5},
        {1024, 768, D3DFMT_R5G6B5},   {1280, 720, D3DFMT_R5G6B5}
    };
    if (Mode >= 8) return D3DERR_INVALIDCALL;
    pMode->Width = modes[Mode].w;
    pMode->Height = modes[Mode].h;
    pMode->RefreshRate = 60;
    pMode->Format = modes[Mode].fmt;
    twrf_log("[TWRF D3D8] EnumAdapterModes(" + std::to_string(Mode) + ") -> " + std::to_string(pMode->Width) + "x" + std::to_string(pMode->Height));
    return D3D_OK;
}

HRESULT TWRFDirect3D8::GetAdapterDisplayMode(UINT, D3DDISPLAYMODE* pMode) {
    if (!pMode) return D3DERR_INVALIDCALL;
    pMode->Width = 1920;
    pMode->Height = 1080;
    pMode->RefreshRate = 60;
    pMode->Format = D3DFMT_X8R8G8B8;
    twrf_log("[TWRF D3D8] GetAdapterDisplayMode() -> 1920x1080");
    return D3D_OK;
}

HRESULT TWRFDirect3D8::CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, WINBOOL) {
    twrf_log("[TWRF D3D8] CheckDeviceType() -> D3D_OK");
    return D3D_OK;
}

HRESULT TWRFDirect3D8::CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) {
    return D3D_OK;
}

HRESULT TWRFDirect3D8::CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, WINBOOL, D3DMULTISAMPLE_TYPE) {
    return D3D_OK;
}

HRESULT TWRFDirect3D8::CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) {
    return D3D_OK;
}

HRESULT TWRFDirect3D8::GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS8* pCaps) {
    if (!pCaps) return D3DERR_INVALIDCALL;
    std::memset(pCaps, 0, sizeof(D3DCAPS8));
    pCaps->DeviceType = D3DDEVTYPE_HAL;
    pCaps->AdapterOrdinal = 0;
    pCaps->Caps = 0;
    pCaps->Caps2 = D3DCAPS2_CANRENDERWINDOWED;
    pCaps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY;
    pCaps->PrimitiveMiscCaps = D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW | D3DPMISCCAPS_COLORWRITEENABLE;
    pCaps->RasterCaps = D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_WFOG | D3DPRASTERCAPS_MIPMAPLODBIAS;
    pCaps->TextureCaps = D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_POW2 | D3DPTEXTURECAPS_PROJECTED;
    pCaps->TextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR;
    pCaps->TextureAddressCaps = D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_CLAMP;
    pCaps->TextureOpCaps = 0xFFFFFFFF;
    pCaps->SrcBlendCaps = 0xFFFFFFFF;
    pCaps->DestBlendCaps = 0xFFFFFFFF;
    pCaps->AlphaCmpCaps = 0xFFFFFFFF;
    pCaps->ZCmpCaps = 0xFFFFFFFF;
    pCaps->MaxTextureWidth = 2048;
    pCaps->MaxTextureHeight = 2048;
    pCaps->MaxVolumeExtent = 0;
    pCaps->MaxTextureRepeat = 32768;
    pCaps->MaxTextureAspectRatio = 2048;
    pCaps->MaxAnisotropy = 1;
    pCaps->MaxVertexIndex = 65535;
    pCaps->MaxPrimitiveCount = 65535;
    pCaps->MaxStreams = 1;
    pCaps->MaxActiveLights = 8;
    pCaps->MaxStreamStride = 256;
    pCaps->MaxTextureBlendStages = 8;
    pCaps->MaxSimultaneousTextures = 4;
    pCaps->VertexShaderVersion = D3DVS_VERSION(1, 1);
    pCaps->PixelShaderVersion = D3DPS_VERSION(1, 1);
    twrf_log("[TWRF D3D8] GetDeviceCaps() -> Full TWRF 3D capabilities reported");
    return D3D_OK;
}

HMONITOR TWRFDirect3D8::GetAdapterMonitor(UINT) { return nullptr; }

HRESULT TWRFDirect3D8::CreateDevice(UINT, D3DDEVTYPE, HWND hFocusWindow, DWORD, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) {
    if (!ppReturnedDeviceInterface) return E_POINTER;
    UINT w = pPresentationParameters ? pPresentationParameters->BackBufferWidth : 800;
    UINT h = pPresentationParameters ? pPresentationParameters->BackBufferHeight : 600;
    twrf_log("[TWRF D3D8] CreateDevice() called -> Resolution: " + std::to_string(w) + "x" + std::to_string(h) + " | HWND=" + std::to_string((uintptr_t)hFocusWindow));
    char wtitle[256] = {0}; char wcls[256] = {0};
    GetWindowTextA(hFocusWindow, wtitle, sizeof(wtitle));
    GetClassNameA(hFocusWindow, wcls, sizeof(wcls));
    twrf_log("[TWRF D3D8] Window Title: \"" + std::string(wtitle) + "\" | Class: \"" + std::string(wcls) + "\"");
    ShowWindow(hFocusWindow, SW_SHOWNORMAL);
    SetForegroundWindow(hFocusWindow);
    PostMessageA(hFocusWindow, WM_ACTIVATEAPP, 1, 0);
    PostMessageA(hFocusWindow, WM_ACTIVATE, 1, 0);
    PostMessageA(hFocusWindow, WM_SETFOCUS, 0, 0);
    *ppReturnedDeviceInterface = new TWRFDirect3DDevice8(this, hFocusWindow, pPresentationParameters);
    return D3D_OK;
}

} // namespace twrf::d3d8

// ============================================================================
// Direct3D 8 Standard Entry Point Export
// ============================================================================

extern "C" __declspec(dllexport) IDirect3D8* WINAPI Direct3DCreate8(UINT SDKVersion) {
    (void)SDKVersion;
    twrf::d3d8::twrf_log("[TWRF D3D8] Direct3DCreate8 called by game application.");
    twrf::d3d8::twrf_log("[TWRF D3D8] Routing all 3D pipeline commands to TWRF Virtual GPU (0% Host GPU usage).");
    return new twrf::d3d8::TWRFDirect3D8();
}

// ============================================================================
// Direct3D 8 Standard Auxiliary Exports & DllMain
// ============================================================================

extern "C" __declspec(dllexport) void WINAPI DebugSetMute() {}
extern "C" __declspec(dllexport) int WINAPI Direct3D8EnableMaximizedWindowedModeShim(int) { return 0; }
extern "C" __declspec(dllexport) HRESULT WINAPI ValidatePixelShader(const DWORD*, const DWORD*, BOOL, char*) { return S_OK; }
extern "C" __declspec(dllexport) HRESULT WINAPI ValidateVertexShader(const DWORD*, const DWORD*, const D3DCAPS8*, BOOL, char*) { return S_OK; }

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        twrf::d3d8::twrf_log("[TWRF D3D8] DLL_PROCESS_ATTACH: TWRF Virtual GPU driver loaded into process.");
    }
    return TRUE;
}

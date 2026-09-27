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
    pixels_.resize(width * height, 0xFF000000);
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

HRESULT TWRFDirect3DSurface8::GetContainer(REFIID, void** ppContainer) {
    if (!ppContainer) return E_POINTER;
    *ppContainer = nullptr;
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::GetDesc(D3DSURFACE_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    pDesc->Format = format_;
    pDesc->Type = D3DRTYPE_SURFACE;
    pDesc->Usage = 0;
    pDesc->Pool = D3DPOOL_MANAGED;
    pDesc->Size = width_ * height_ * sizeof(uint32_t);
    pDesc->MultiSampleType = D3DMULTISAMPLE_NONE;
    pDesc->Width = width_;
    pDesc->Height = height_;
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD) {
    if (!pLockedRect) return D3DERR_INVALIDCALL;
    bool is_16bit = (format_ == D3DFMT_R5G6B5 || format_ == D3DFMT_A1R5G5B5 || format_ == D3DFMT_A4R4G4B4);
    UINT bytes_per_pixel = is_16bit ? 2 : 4;
    pLockedRect->Pitch = width_ * bytes_per_pixel;
    if (!pRect) {
        pLockedRect->pBits = pixels_.data();
    } else {
        pLockedRect->pBits = reinterpret_cast<uint8_t*>(pixels_.data()) + (pRect->top * pLockedRect->Pitch + pRect->left * bytes_per_pixel);
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DSurface8::UnlockRect() {
    return D3D_OK;
}

// ============================================================================
// TWRFDirect3DTexture8 Implementation
// ============================================================================

TWRFDirect3DTexture8::TWRFDirect3DTexture8(IDirect3DDevice8* device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
    : device_(device), width_(std::max<UINT>(1, width)), height_(std::max<UINT>(1, height)), levels_(std::max<UINT>(1, levels)), usage_(usage), format_(format), pool_(pool) {
    surfaces_.push_back(std::make_unique<TWRFDirect3DSurface8>(device_, width_, height_, format_));
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
    if (Level >= surfaces_.size()) return D3DERR_INVALIDCALL;
    return surfaces_[Level]->LockRect(pLockedRect, pRect, Flags);
}

HRESULT TWRFDirect3DTexture8::UnlockRect(UINT Level) {
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
        bool is_16bit = (surf->format() == D3DFMT_R5G6B5 || surf->format() == D3DFMT_A1R5G5B5 || surf->format() == D3DFMT_A4R4G4B4);

        for (UINT y = 0; y < height_; ++y) {
            for (UINT x = 0; x < width_; ++x) {
                raster::ColorRGBA col;
                if (is_16bit) {
                    uint16_t c = reinterpret_cast<const uint16_t*>(raw)[y * width_ + x];
                    if (surf->format() == D3DFMT_R5G6B5) {
                        col = raster::ColorRGBA(((c >> 11) & 0x1F) * 255 / 31, ((c >> 5) & 0x3F) * 255 / 63, (c & 0x1F) * 255 / 31, 255);
                    } else if (surf->format() == D3DFMT_A1R5G5B5) {
                        col = raster::ColorRGBA(((c >> 10) & 0x1F) * 255 / 31, ((c >> 5) & 0x1F) * 255 / 31, (c & 0x1F) * 255 / 31, (c & 0x8000) ? 255 : 0);
                    } else {
                        col = raster::ColorRGBA(((c >> 8) & 0x0F) * 17, ((c >> 4) & 0x0F) * 17, (c & 0x0F) * 17, ((c >> 12) & 0x0F) * 17);
                    }
                } else {
                    uint32_t argb = reinterpret_cast<const uint32_t*>(raw)[y * width_ + x];
                    uint8_t a = (surf->format() == D3DFMT_X8R8G8B8) ? 255 : ((argb >> 24) & 0xFF);
                    col = raster::ColorRGBA((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, a);
                }
                raster_tex_->set_pixel(x, y, col);
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

    float tu = u - std::floor(u);
    float tv = v - std::floor(v);
    uint32_t x = std::clamp<uint32_t>(static_cast<uint32_t>(tu * width_), 0, width_ - 1);
    uint32_t y = std::clamp<uint32_t>(static_cast<uint32_t>(tv * height_), 0, height_ - 1);

    uint32_t argb = surfaces_[0]->pixel_data()[y * width_ + x];
    uint8_t a = (argb >> 24) & 0xFF;
    uint8_t r = (argb >> 16) & 0xFF;
    uint8_t g = (argb >> 8) & 0xFF;
    uint8_t b = (argb) & 0xFF;
    return raster::ColorRGBA(r, g, b, a);
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
    if (OffsetToLock >= buffer_.size()) return D3DERR_INVALIDCALL;
    (void)SizeToLock;
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
    if (OffsetToLock >= buffer_.size()) return D3DERR_INVALIDCALL;
    (void)SizeToLock;
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

HRESULT TWRFDirect3DDevice8::TestCooperativeLevel() { return D3D_OK; }
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

HRESULT TWRFDirect3DDevice8::Clear(DWORD, const D3DRECT*, DWORD, D3DCOLOR Color, float Z, DWORD) {
    uint8_t a = (Color >> 24) & 0xFF;
    uint8_t r = (Color >> 16) & 0xFF;
    uint8_t g = (Color >> 8) & 0xFF;
    uint8_t b = (Color) & 0xFF;
    raster::ColorRGBA clear_col(r, g, b, a);

    int total = tile_config_.total_tiles();
    int tile_size = tile_config_.tile_size;
    int pixel_count = tile_size * tile_size;

    std::vector<raster::ColorRGBA> cols(pixel_count, clear_col);
    std::vector<float> deps(pixel_count, Z);

    for (int i = 0; i < total; ++i) {
        state_store_.write_tile(10000 + i, cols.data(), deps.data(), pixel_count, frame_index_, frame_index_, frame_index_);
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
    if (pViewport) viewport_ = *pViewport;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetViewport(D3DVIEWPORT8* pViewport) {
    if (pViewport) *pViewport = viewport_;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
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
    active_fvf_ = Handle;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetVertexShader(DWORD* pHandle) {
    if (pHandle) *pHandle = active_fvf_;
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) {
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
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::GetIndices(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) {
    if (ppIndexData) *ppIndexData = current_ibo_;
    if (pBaseVertexIndex) *pBaseVertexIndex = base_vertex_index_;
    return D3D_OK;
}

void TWRFDirect3DDevice8::rasterize_triangle_primitive(const raster::Triangle& tri, bool is_screen_space) {
    raster::Mat4 mvp = is_screen_space ? raster::Mat4::identity() : (proj_matrix_ * view_matrix_ * world_matrix_);
    int tile_size = tile_config_.tile_size;
    int total_tiles = tile_config_.total_tiles();
    const raster::Texture* tex = active_texture_ ? active_texture_->get_raster_texture() : nullptr;

    for (int i = 0; i < total_tiles; ++i) {
        auto bounds = tile_config_.get_tile_bounds(i);
        auto* slot = state_store_.get_mutable_slot(10000 + i);
        if (!slot) continue;

        raster::TileRasterizer::rasterize_triangle_into_tile(
            tri, mvp, tex,
            width_, height_,
            bounds.min_x, bounds.min_y, tile_size,
            slot->color_buffer.data(), slot->depth_buffer.data()
        );
    }
}

HRESULT TWRFDirect3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    if (!pVertexStreamZeroData || PrimitiveCount == 0) return D3D_OK;
    static int dp_count = 0;
    if (++dp_count <= 5 || (dp_count % 100) == 0) {
        twrf_log("[TWRF D3D8] DrawPrimitiveUP #" + std::to_string(dp_count) + " (prims=" + std::to_string(PrimitiveCount) + ", stride=" + std::to_string(VertexStreamZeroStride) + ")");
    }
    bool is_rhw = (active_fvf_ & D3DFVF_XYZRHW) != 0;

    const uint8_t* raw = static_cast<const uint8_t*>(pVertexStreamZeroData);
    auto parse_vert = [&](size_t idx) -> raster::Vertex {
        const float* f = reinterpret_cast<const float*>(raw + idx * VertexStreamZeroStride);
        raster::Vertex v{};
        v.pos = raster::Vec3(f[0], f[1], f[2]);
        if (is_rhw) {
            float ndc_x = (f[0] / width_) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (f[1] / height_) * 2.0f;
            v.pos = raster::Vec3(ndc_x, ndc_y, f[2]);
        }
        if (VertexStreamZeroStride >= 20) {
            uint32_t col = *reinterpret_cast<const uint32_t*>(raw + idx * VertexStreamZeroStride + 16);
            float a = ((col >> 24) & 0xFF) / 255.0f;
            float r = ((col >> 16) & 0xFF) / 255.0f;
            float g = ((col >> 8) & 0xFF) / 255.0f;
            float b = (col & 0xFF) / 255.0f;
            v.color = raster::Vec4(r, g, b, a);
        }
        if (VertexStreamZeroStride >= 28) {
            const float* uv = reinterpret_cast<const float*>(raw + idx * VertexStreamZeroStride + 20);
            v.uv = raster::Vec2(uv[0], uv[1]);
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
    if (!pVertexStreamZeroData || !pIndexData || PrimitiveCount == 0) return D3D_OK;
    static int dip_count = 0;
    if (++dip_count <= 5 || (dip_count % 100) == 0) {
        twrf_log("[TWRF D3D8] DrawIndexedPrimitiveUP #" + std::to_string(dip_count) + " (prims=" + std::to_string(PrimitiveCount) + ")");
    }
    bool is_rhw = (active_fvf_ & D3DFVF_XYZRHW) != 0;
    const uint8_t* raw = static_cast<const uint8_t*>(pVertexStreamZeroData);

    auto parse_vert = [&](size_t idx) -> raster::Vertex {
        const float* f = reinterpret_cast<const float*>(raw + idx * VertexStreamZeroStride);
        raster::Vertex v{};
        v.pos = raster::Vec3(f[0], f[1], f[2]);
        if (is_rhw) {
            float ndc_x = (f[0] / width_) * 2.0f - 1.0f;
            float ndc_y = 1.0f - (f[1] / height_) * 2.0f;
            v.pos = raster::Vec3(ndc_x, ndc_y, f[2]);
        }
        if (VertexStreamZeroStride >= 20) {
            uint32_t col = *reinterpret_cast<const uint32_t*>(raw + idx * VertexStreamZeroStride + 16);
            float a = ((col >> 24) & 0xFF) / 255.0f;
            float r = ((col >> 16) & 0xFF) / 255.0f;
            float g = ((col >> 8) & 0xFF) / 255.0f;
            float b = (col & 0xFF) / 255.0f;
            v.color = raster::Vec4(r, g, b, a);
        }
        if (VertexStreamZeroStride >= 28) {
            const float* uv = reinterpret_cast<const float*>(raw + idx * VertexStreamZeroStride + 20);
            v.uv = raster::Vec2(uv[0], uv[1]);
        }
        return v;
    };

    const uint16_t* indices16 = static_cast<const uint16_t*>(pIndexData);
    if (PrimitiveType == D3DPT_TRIANGLELIST) {
        for (UINT i = 0; i < PrimitiveCount; ++i) {
            UINT i0 = (IndexDataFormat == D3DFMT_INDEX16) ? indices16[i * 3 + 0] : static_cast<const uint32_t*>(pIndexData)[i * 3 + 0];
            UINT i1 = (IndexDataFormat == D3DFMT_INDEX16) ? indices16[i * 3 + 1] : static_cast<const uint32_t*>(pIndexData)[i * 3 + 1];
            UINT i2 = (IndexDataFormat == D3DFMT_INDEX16) ? indices16[i * 3 + 2] : static_cast<const uint32_t*>(pIndexData)[i * 3 + 2];
            raster::Triangle tri(parse_vert(i0), parse_vert(i1), parse_vert(i2));
            rasterize_triangle_primitive(tri, is_rhw);
        }
    }
    return D3D_OK;
}

HRESULT TWRFDirect3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    if (!current_vbo_ || current_vbo_stride_ == 0) return D3D_OK;
    auto* vbo = reinterpret_cast<TWRFDirect3DVertexBuffer8*>(current_vbo_);
    return DrawPrimitiveUP(PrimitiveType, PrimitiveCount, vbo->data() + StartVertex * current_vbo_stride_, current_vbo_stride_);
}

HRESULT TWRFDirect3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT, UINT NumVertices, UINT startIndex, UINT primCount) {
    if (!current_vbo_ || !current_ibo_ || current_vbo_stride_ == 0) return D3D_OK;
    auto* vbo = reinterpret_cast<TWRFDirect3DVertexBuffer8*>(current_vbo_);
    auto* ibo = reinterpret_cast<TWRFDirect3DIndexBuffer8*>(current_ibo_);
    size_t idx_size = (ibo->format() == D3DFMT_INDEX16) ? sizeof(uint16_t) : sizeof(uint32_t);
    return DrawIndexedPrimitiveUP(PrimitiveType, 0, NumVertices, primCount, ibo->data() + startIndex * idx_size, ibo->format(), vbo->data(), current_vbo_stride_);
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

    // Blit CPU frame buffer to target window via standard GDI
    HDC hdc = GetDC(target_hwnd);
    if (hdc) {
        StretchDIBits(hdc, 0, 0, width_, height_, 0, 0, width_, height_,
                      gdi_pixel_buffer_.data(), &bmi_, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(target_hwnd, hdc);
    }

    if (frame_index_ >= 5 && (frame_index_ % 5 == 0)) {
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
HRESULT TWRFDirect3DDevice8::GetRasterStatus(D3DRASTER_STATUS*) { return D3D_OK; }
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
HRESULT TWRFDirect3DDevice8::CopyRects(IDirect3DSurface8*, const RECT*, UINT, IDirect3DSurface8*, const POINT*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::UpdateTexture(IDirect3DBaseTexture8*, IDirect3DBaseTexture8*) { return D3D_OK; }
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
HRESULT TWRFDirect3DDevice8::SetMaterial(const D3DMATERIAL8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetMaterial(D3DMATERIAL8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetLight(DWORD, const D3DLIGHT8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetLight(DWORD, D3DLIGHT8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::LightEnable(DWORD, WINBOOL) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetLightEnable(DWORD, WINBOOL*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetClipPlane(DWORD, const float*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetClipPlane(DWORD, float*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::BeginStateBlock() { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::EndStateBlock(DWORD* p) { if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::ApplyStateBlock(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CaptureStateBlock(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeleteStateBlock(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD* p) { if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetClipStatus(const D3DCLIPSTATUS8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetClipStatus(D3DCLIPSTATUS8*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD* p) { if (p) *p = 0; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::ValidateDevice(DWORD* p) { if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetInfo(DWORD, void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPaletteEntries(UINT, const PALETTEENTRY*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPaletteEntries(UINT, PALETTEENTRY*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetCurrentTexturePalette(UINT) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetCurrentTexturePalette(UINT* p) { if (p) *p = 0; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreateVertexShader(const DWORD*, const DWORD*, DWORD* p, DWORD) { if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeleteVertexShader(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetVertexShaderConstant(DWORD, const void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderDeclaration(DWORD, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetVertexShaderFunction(DWORD, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::CreatePixelShader(const DWORD*, DWORD* p) { if (p) *p = 1; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPixelShader(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShader(DWORD* p) { if (p) *p = 0; return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeletePixelShader(DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::SetPixelShaderConstant(DWORD, const void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShaderConstant(DWORD, void*, DWORD) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::GetPixelShaderFunction(DWORD, void*, DWORD*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DrawRectPatch(UINT, const float*, const D3DRECTPATCH_INFO*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DrawTriPatch(UINT, const float*, const D3DTRIPATCH_INFO*) { return D3D_OK; }
HRESULT TWRFDirect3DDevice8::DeletePatch(UINT) { return D3D_OK; }

HRESULT TWRFDirect3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) {
    if (!ppTexture) return E_POINTER;
    static int tex_count = 0;
    if (++tex_count <= 5 || (tex_count % 20) == 0) {
        twrf_log("[TWRF D3D8] CreateTexture #" + std::to_string(tex_count) + " (" + std::to_string(Width) + "x" + std::to_string(Height) + ")");
    }
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

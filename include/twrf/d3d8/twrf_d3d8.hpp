#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <windows.h>
#include <d3d8.h>

#include "twrf/core/types.hpp"
#include "twrf/core/state_store_3d.hpp"
#include "twrf/raster/math.hpp"
#include "twrf/raster/geometry.hpp"
#include "twrf/raster/texture.hpp"
#include "twrf/raster/tile_rasterizer.hpp"
#include "twrf/raster/spatial_binner_3d.hpp"
#include "twrf/raster/dual_layer_compositor.hpp"
#include "twrf/raster/framebuffer.hpp"

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace twrf::d3d8 {

class TWRFDirect3D8;
class TWRFDirect3DDevice8;
class TWRFDirect3DTexture8;
class TWRFDirect3DSurface8;
class TWRFDirect3DVertexBuffer8;
class TWRFDirect3DIndexBuffer8;

// Texture Surface Implementation
class TWRFDirect3DSurface8 : public IDirect3DSurface8 {
public:
    TWRFDirect3DSurface8(IDirect3DDevice8* device, UINT width, UINT height, D3DFORMAT format);
    virtual ~TWRFDirect3DSurface8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3DResource8
    STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override;
    STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override;
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(FreePrivateData)(REFGUID refguid) override;

    // IDirect3DSurface8
    STDMETHOD(GetContainer)(REFIID riid, void** ppContainer) override;
    STDMETHOD(GetDesc)(D3DSURFACE_DESC* pDesc) override;
    STDMETHOD(LockRect)(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) override;
    STDMETHOD(UnlockRect)() override;

    [[nodiscard]] UINT width() const noexcept { return width_; }
    [[nodiscard]] UINT height() const noexcept { return height_; }
    [[nodiscard]] D3DFORMAT format() const noexcept { return format_; }
    [[nodiscard]] const uint32_t* pixel_data() const noexcept { return pixels_.data(); }
    [[nodiscard]] uint32_t* pixel_data() noexcept { return pixels_.data(); }

private:
    ULONG ref_count_{1};
    IDirect3DDevice8* device_{nullptr};
    UINT width_{0};
    UINT height_{0};
    D3DFORMAT format_{D3DFMT_A8R8G8B8};
    std::vector<uint32_t> pixels_;
};

// Texture Implementation
class TWRFDirect3DTexture8 : public IDirect3DTexture8 {
public:
    TWRFDirect3DTexture8(IDirect3DDevice8* device, UINT width, UINT height, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~TWRFDirect3DTexture8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3DResource8
    STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override;
    STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override;
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(FreePrivateData)(REFGUID refguid) override;
    STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew) override;
    STDMETHOD_(DWORD, GetPriority)() override;
    STDMETHOD_(void, PreLoad)() override;
    STDMETHOD_(D3DRESOURCETYPE, GetType)() override;

    // IDirect3DBaseTexture8
    STDMETHOD_(DWORD, SetLOD)(DWORD LODNew) override;
    STDMETHOD_(DWORD, GetLOD)() override;
    STDMETHOD_(DWORD, GetLevelCount)() override;

    // IDirect3DTexture8
    STDMETHOD(GetLevelDesc)(UINT Level, D3DSURFACE_DESC* pDesc) override;
    STDMETHOD(GetSurfaceLevel)(UINT Level, IDirect3DSurface8** ppSurfaceLevel) override;
    STDMETHOD(LockRect)(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) override;
    STDMETHOD(UnlockRect)(UINT Level) override;
    STDMETHOD(AddDirtyRect)(const RECT* pDirtyRect) override;

    [[nodiscard]] raster::ColorRGBA sample_uv(float u, float v) const noexcept;
    [[nodiscard]] UINT width() const noexcept { return width_; }
    [[nodiscard]] UINT height() const noexcept { return height_; }

private:
    ULONG ref_count_{1};
    IDirect3DDevice8* device_{nullptr};
    UINT width_{0};
    UINT height_{0};
    UINT levels_{1};
    DWORD usage_{0};
    D3DFORMAT format_{D3DFMT_A8R8G8B8};
    D3DPOOL pool_{D3DPOOL_MANAGED};
    std::vector<std::unique_ptr<TWRFDirect3DSurface8>> surfaces_;
};

// Vertex Buffer Implementation
class TWRFDirect3DVertexBuffer8 : public IDirect3DVertexBuffer8 {
public:
    TWRFDirect3DVertexBuffer8(IDirect3DDevice8* device, UINT length, DWORD usage, DWORD fvf, D3DPOOL pool);
    virtual ~TWRFDirect3DVertexBuffer8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3DResource8
    STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override;
    STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override;
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(FreePrivateData)(REFGUID refguid) override;
    STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew) override;
    STDMETHOD_(DWORD, GetPriority)() override;
    STDMETHOD_(void, PreLoad)() override;
    STDMETHOD_(D3DRESOURCETYPE, GetType)() override;

    // IDirect3DVertexBuffer8
    STDMETHOD(Lock)(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) override;
    STDMETHOD(Unlock)() override;
    STDMETHOD(GetDesc)(D3DVERTEXBUFFER_DESC* pDesc) override;

    [[nodiscard]] const uint8_t* data() const noexcept { return buffer_.data(); }
    [[nodiscard]] UINT length() const noexcept { return length_; }
    [[nodiscard]] DWORD fvf() const noexcept { return fvf_; }

private:
    ULONG ref_count_{1};
    IDirect3DDevice8* device_{nullptr};
    UINT length_{0};
    DWORD usage_{0};
    DWORD fvf_{0};
    D3DPOOL pool_{D3DPOOL_MANAGED};
    std::vector<uint8_t> buffer_;
};

// Index Buffer Implementation
class TWRFDirect3DIndexBuffer8 : public IDirect3DIndexBuffer8 {
public:
    TWRFDirect3DIndexBuffer8(IDirect3DDevice8* device, UINT length, DWORD usage, D3DFORMAT format, D3DPOOL pool);
    virtual ~TWRFDirect3DIndexBuffer8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3DResource8
    STDMETHOD(GetDevice)(IDirect3DDevice8** ppDevice) override;
    STDMETHOD(SetPrivateData)(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override;
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(FreePrivateData)(REFGUID refguid) override;
    STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew) override;
    STDMETHOD_(DWORD, GetPriority)() override;
    STDMETHOD_(void, PreLoad)() override;
    STDMETHOD_(D3DRESOURCETYPE, GetType)() override;

    // IDirect3DIndexBuffer8
    STDMETHOD(Lock)(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD Flags) override;
    STDMETHOD(Unlock)() override;
    STDMETHOD(GetDesc)(D3DINDEXBUFFER_DESC* pDesc) override;

    [[nodiscard]] const uint8_t* data() const noexcept { return buffer_.data(); }
    [[nodiscard]] UINT length() const noexcept { return length_; }
    [[nodiscard]] D3DFORMAT format() const noexcept { return format_; }

private:
    ULONG ref_count_{1};
    IDirect3DDevice8* device_{nullptr};
    UINT length_{0};
    DWORD usage_{0};
    D3DFORMAT format_{D3DFMT_INDEX16};
    D3DPOOL pool_{D3DPOOL_MANAGED};
    std::vector<uint8_t> buffer_;
};

// Main TWRF Virtual GPU Direct3D 8 Device
class TWRFDirect3DDevice8 : public IDirect3DDevice8 {
public:
    TWRFDirect3DDevice8(TWRFDirect3D8* d3d, HWND hwnd, D3DPRESENT_PARAMETERS* params);
    virtual ~TWRFDirect3DDevice8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3DDevice8
    STDMETHOD(TestCooperativeLevel)() override;
    STDMETHOD_(UINT, GetAvailableTextureMem)() override;
    STDMETHOD(ResourceManagerDiscardBytes)(DWORD Bytes) override;
    STDMETHOD(GetDirect3D)(IDirect3D8** ppD3D8) override;
    STDMETHOD(GetDeviceCaps)(D3DCAPS8* pCaps) override;
    STDMETHOD(GetDisplayMode)(D3DDISPLAYMODE* pMode) override;
    STDMETHOD(GetCreationParameters)(D3DDEVICE_CREATION_PARAMETERS* pParameters) override;
    STDMETHOD(SetCursorProperties)(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface8* pCursorBitmap) override;
    STDMETHOD_(void, SetCursorPosition)(UINT XScreenSpace, UINT YScreenSpace, DWORD Flags) override;
    STDMETHOD_(WINBOOL, ShowCursor)(WINBOOL bShow) override;
    STDMETHOD(CreateAdditionalSwapChain)(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain8** pSwapChain) override;
    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) override;
    STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) override;
    STDMETHOD(GetBackBuffer)(UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface8** ppBackBuffer) override;
    STDMETHOD(GetRasterStatus)(D3DRASTER_STATUS* pRasterStatus) override;
    STDMETHOD_(void, SetGammaRamp)(DWORD Flags, const D3DGAMMARAMP* pRamp) override;
    STDMETHOD_(void, GetGammaRamp)(D3DGAMMARAMP* pRamp) override;
    STDMETHOD(CreateRenderTarget)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, WINBOOL Lockable, IDirect3DSurface8** ppSurface) override;
    STDMETHOD(CreateDepthStencilSurface)(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, IDirect3DSurface8** ppSurface) override;
    STDMETHOD(CreateImageSurface)(UINT Width, UINT Height, D3DFORMAT Format, IDirect3DSurface8** ppSurface) override;
    STDMETHOD(CopyRects)(IDirect3DSurface8* pSourceSurface, const RECT* pSourceRectsArray, UINT cRects, IDirect3DSurface8* pDestinationSurface, const POINT* pDestPointsArray) override;
    STDMETHOD(UpdateTexture)(IDirect3DBaseTexture8* pSourceTexture, IDirect3DBaseTexture8* pDestinationTexture) override;
    STDMETHOD(GetFrontBuffer)(IDirect3DSurface8* pDestSurface) override;
    STDMETHOD(SetRenderTarget)(IDirect3DSurface8* pRenderTarget, IDirect3DSurface8* pNewZStencil) override;
    STDMETHOD(GetRenderTarget)(IDirect3DSurface8** ppRenderTarget) override;
    STDMETHOD(GetDepthStencilSurface)(IDirect3DSurface8** ppZStencilSurface) override;
    STDMETHOD(BeginScene)() override;
    STDMETHOD(EndScene)() override;
    STDMETHOD(Clear)(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    STDMETHOD(SetTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override;
    STDMETHOD(GetTransform)(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override;
    STDMETHOD(MultiplyTransform)(D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix) override;
    STDMETHOD(SetViewport)(const D3DVIEWPORT8* pViewport) override;
    STDMETHOD(GetViewport)(D3DVIEWPORT8* pViewport) override;
    STDMETHOD(SetMaterial)(const D3DMATERIAL8* pMaterial) override;
    STDMETHOD(GetMaterial)(D3DMATERIAL8* pMaterial) override;
    STDMETHOD(SetLight)(DWORD Index, const D3DLIGHT8* pLight) override;
    STDMETHOD(GetLight)(DWORD Index, D3DLIGHT8* pLight) override;
    STDMETHOD(LightEnable)(DWORD Index, WINBOOL Enable) override;
    STDMETHOD(GetLightEnable)(DWORD Index, WINBOOL* pEnable) override;
    STDMETHOD(SetClipPlane)(DWORD Index, const float* pPlane) override;
    STDMETHOD(GetClipPlane)(DWORD Index, float* pPlane) override;
    STDMETHOD(SetRenderState)(D3DRENDERSTATETYPE State, DWORD Value) override;
    STDMETHOD(GetRenderState)(D3DRENDERSTATETYPE State, DWORD* pValue) override;
    STDMETHOD(BeginStateBlock)() override;
    STDMETHOD(EndStateBlock)(DWORD* pToken) override;
    STDMETHOD(ApplyStateBlock)(DWORD Token) override;
    STDMETHOD(CaptureStateBlock)(DWORD Token) override;
    STDMETHOD(DeleteStateBlock)(DWORD Token) override;
    STDMETHOD(CreateStateBlock)(D3DSTATEBLOCKTYPE Type, DWORD* pToken) override;
    STDMETHOD(SetClipStatus)(const D3DCLIPSTATUS8* pClipStatus) override;
    STDMETHOD(GetClipStatus)(D3DCLIPSTATUS8* pClipStatus) override;
    STDMETHOD(GetTexture)(DWORD Stage, IDirect3DBaseTexture8** ppTexture) override;
    STDMETHOD(SetTexture)(DWORD Stage, IDirect3DBaseTexture8* pTexture) override;
    STDMETHOD(GetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override;
    STDMETHOD(SetTextureStageState)(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
    STDMETHOD(ValidateDevice)(DWORD* pNumPasses) override;
    STDMETHOD(GetInfo)(DWORD DevInfoID, void* pDevInfoStruct, DWORD DevInfoStructSize) override;
    STDMETHOD(SetPaletteEntries)(UINT PaletteNumber, const PALETTEENTRY* pEntries) override;
    STDMETHOD(GetPaletteEntries)(UINT PaletteNumber, PALETTEENTRY* pEntries) override;
    STDMETHOD(SetCurrentTexturePalette)(UINT PaletteNumber) override;
    STDMETHOD(GetCurrentTexturePalette)(UINT* PaletteNumber) override;
    STDMETHOD(DrawPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
    STDMETHOD(DrawIndexedPrimitive)(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount) override;
    STDMETHOD(DrawPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    STDMETHOD(DrawIndexedPrimitiveUP)(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    STDMETHOD(ProcessVertices)(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer8* pDestBuffer, DWORD Flags) override;
    STDMETHOD(CreateVertexShader)(const DWORD* pDeclaration, const DWORD* pFunction, DWORD* pHandle, DWORD Usage) override;
    STDMETHOD(SetVertexShader)(DWORD Handle) override;
    STDMETHOD(GetVertexShader)(DWORD* pHandle) override;
    STDMETHOD(DeleteVertexShader)(DWORD Handle) override;
    STDMETHOD(SetVertexShaderConstant)(DWORD Register, const void* pConstantData, DWORD ConstantCount) override;
    STDMETHOD(GetVertexShaderConstant)(DWORD Register, void* pConstantData, DWORD ConstantCount) override;
    STDMETHOD(GetVertexShaderDeclaration)(DWORD Handle, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(GetVertexShaderFunction)(DWORD Handle, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(SetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer8* pStreamData, UINT Stride) override;
    STDMETHOD(GetStreamSource)(UINT StreamNumber, IDirect3DVertexBuffer8** ppStreamData, UINT* pStride) override;
    STDMETHOD(SetIndices)(IDirect3DIndexBuffer8* pIndexData, UINT BaseVertexIndex) override;
    STDMETHOD(GetIndices)(IDirect3DIndexBuffer8** ppIndexData, UINT* pBaseVertexIndex) override;
    STDMETHOD(CreatePixelShader)(const DWORD* pFunction, DWORD* pHandle) override;
    STDMETHOD(SetPixelShader)(DWORD Handle) override;
    STDMETHOD(GetPixelShader)(DWORD* pHandle) override;
    STDMETHOD(DeletePixelShader)(DWORD Handle) override;
    STDMETHOD(SetPixelShaderConstant)(DWORD Register, const void* pConstantData, DWORD ConstantCount) override;
    STDMETHOD(GetPixelShaderConstant)(DWORD Register, void* pConstantData, DWORD ConstantCount) override;
    STDMETHOD(GetPixelShaderFunction)(DWORD Handle, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(DrawRectPatch)(UINT Handle, const float* pNumSegs, const D3DRECTPATCH_INFO* pRectPatchInfo) override;
    STDMETHOD(DrawTriPatch)(UINT Handle, const float* pNumSegs, const D3DTRIPATCH_INFO* pTriPatchInfo) override;
    STDMETHOD(DeletePatch)(UINT Handle) override;
    STDMETHOD(CreateTexture)(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture8** ppTexture) override;
    STDMETHOD(CreateVolumeTexture)(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture8** ppVolumeTexture) override;
    STDMETHOD(CreateCubeTexture)(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture8** ppCubeTexture) override;
    STDMETHOD(CreateVertexBuffer)(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer8** ppVertexBuffer) override;
    STDMETHOD(CreateIndexBuffer)(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer8** ppIndexBuffer) override;

private:
    ULONG ref_count_{1};
    TWRFDirect3D8* d3d_{nullptr};
    HWND hwnd_{nullptr};
    UINT width_{800};
    UINT height_{600};

    // TWRF Virtual GPU Core Components
    raster::TileConfig tile_config_;
    LogicalStateStore3D state_store_;
    raster::SpatialBinner3D binner_;
    raster::FrameBuffer framebuffer_;

    // Transform Matrices
    raster::Mat4 world_matrix_{raster::Mat4::identity()};
    raster::Mat4 view_matrix_{raster::Mat4::identity()};
    raster::Mat4 proj_matrix_{raster::Mat4::identity()};
    D3DVIEWPORT8 viewport_{};

    // Render States
    DWORD active_fvf_{0};
    WINBOOL z_enable_{TRUE};
    WINBOOL z_write_enable_{TRUE};
    D3DCULL cull_mode_{D3DCULL_CCW};
    WINBOOL alpha_blend_enable_{FALSE};
    WINBOOL alpha_test_enable_{FALSE};

    // Active Texture
    TWRFDirect3DTexture8* active_texture_{nullptr};

    // Active Vertex & Index Streams
    IDirect3DVertexBuffer8* current_vbo_{nullptr};
    UINT current_vbo_stride_{0};
    IDirect3DIndexBuffer8* current_ibo_{nullptr};
    TWRFDirect3DSurface8* back_buffer_{nullptr};
    TWRFDirect3DSurface8* depth_stencil_{nullptr};
    IDirect3DSurface8* current_render_target_{nullptr};
    UINT base_vertex_index_{0};

    // GDI Presentation Bitmap
    BITMAPINFO bmi_{};
    std::vector<uint32_t> gdi_pixel_buffer_;
    uint32_t frame_index_{0};

    void rasterize_triangle_primitive(const raster::Triangle& tri, bool is_screen_space);
};

// Root TWRF Direct3D 8 Interface
class TWRFDirect3D8 : public IDirect3D8 {
public:
    TWRFDirect3D8();
    virtual ~TWRFDirect3D8() = default;

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;

    // IDirect3D8
    STDMETHOD(RegisterSoftwareDevice)(void* pInitializeFunction) override;
    STDMETHOD_(UINT, GetAdapterCount)() override;
    STDMETHOD(GetAdapterIdentifier)(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8* pIdentifier) override;
    STDMETHOD_(UINT, GetAdapterModeCount)(UINT Adapter) override;
    STDMETHOD(EnumAdapterModes)(UINT Adapter, UINT Mode, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(GetAdapterDisplayMode)(UINT Adapter, D3DDISPLAYMODE* pMode) override;
    STDMETHOD(CheckDeviceType)(UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat, D3DFORMAT BackBufferFormat, WINBOOL Windowed) override;
    STDMETHOD(CheckDeviceFormat)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;
    STDMETHOD(CheckDeviceMultiSampleType)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, WINBOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType) override;
    STDMETHOD(CheckDepthStencilMatch)(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override;
    STDMETHOD(GetDeviceCaps)(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8* pCaps) override;
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(UINT Adapter) override;
    STDMETHOD(CreateDevice)(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice8** ppReturnedDeviceInterface) override;

private:
    ULONG ref_count_{1};
};

} // namespace twrf::d3d8

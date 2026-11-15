/* FORK-082: HDR Output's presenter on Windows.
 *
 * OpenGL on Windows cannot present an HDR frame itself, so while HDR Output is
 * on, a DXGI flip-model swap chain (R16G16B16A16_FLOAT, scRGB: linear Rec.709,
 * 1.0 = 80 nits) takes over presentation of the game window. The GL renderer
 * draws its final frame into a D3D11 texture shared through
 * WGL_NV_DX_interop2 (gfx_hdr.inl), which is copied to the back buffer and
 * presented; SDL_GL_SwapWindow is skipped meanwhile. Turning it off releases
 * the swap chain and GL presents again.
 *
 * Everything runs on the render thread (the thread that owns the GL context).
 * d3d11.dll and dxgi.dll are loaded on first use, so the executable keeps
 * starting where they are missing; any failure logs once and HDR Output then
 * reports itself unavailable for the session. */

#include "gfx_hdr_dxgi.h"

#if defined(_WIN32) && !defined(GE_XBOX)

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <string.h>

#include "system.h"

/* WGL_NV_DX_interop2: no wglext.h in the MinGW tree, so the entry points by hand */
#define GFX_WGL_ACCESS_WRITE_DISCARD_NV 0x0002
typedef HANDLE(WINAPI* PfnDXOpenDevice)(void* dxDevice);
typedef BOOL(WINAPI* PfnDXCloseDevice)(HANDLE hDevice);
typedef HANDLE(WINAPI* PfnDXRegisterObject)(HANDLE hDevice, void* dxObject, unsigned int name, unsigned int type,
                                            unsigned int access);
typedef BOOL(WINAPI* PfnDXUnregisterObject)(HANDLE hDevice, HANDLE hObject);
typedef BOOL(WINAPI* PfnDXLockObjects)(HANDLE hDevice, int count, HANDLE* hObjects);
typedef BOOL(WINAPI* PfnDXUnlockObjects)(HANDLE hDevice, int count, HANDLE* hObjects);
typedef HRESULT(WINAPI* PfnD3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
                                              UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
typedef HRESULT(WINAPI* PfnCreateDXGIFactory1)(REFIID, void**);

static PfnDXOpenDevice dx_open;
static PfnDXCloseDevice dx_close;
static PfnDXRegisterObject dx_register;
static PfnDXUnregisterObject dx_unregister;
static PfnDXLockObjects dx_lock;
static PfnDXUnlockObjects dx_unlock;

static bool s_failed = false;      /* a hard failure: unavailable for the session */
static HWND s_hwnd = NULL;
static ID3D11Device* s_dev = NULL;
static ID3D11DeviceContext* s_ctx = NULL;
static IDXGIFactory2* s_factory = NULL;
static IDXGISwapChain1* s_chain = NULL;
static ID3D11Texture2D* s_tex = NULL; /* shared with GL */
static HANDLE s_interop_dev = NULL, s_interop_tex = NULL;
static UINT s_w = 0, s_h = 0;
static bool s_tearing = false;

static void fail(const char* what, HRESULT hr) {
    sysLogPrintf(LOG_WARNING, "HDR Output: %s failed (0x%08lx) - unavailable", what, (unsigned long) hr);
    s_failed = true;
}

template <typename T> static void release(T*& p) {
    if (p) {
        p->Release();
        p = NULL;
    }
}

static void drop_target(void) {
    if (s_interop_tex) {
        dx_unregister(s_interop_dev, s_interop_tex);
        s_interop_tex = NULL;
    }
    release(s_tex);
}

static bool load_entry_points(void) {
    dx_open = (PfnDXOpenDevice) SDL_GL_GetProcAddress("wglDXOpenDeviceNV");
    dx_close = (PfnDXCloseDevice) SDL_GL_GetProcAddress("wglDXCloseDeviceNV");
    dx_register = (PfnDXRegisterObject) SDL_GL_GetProcAddress("wglDXRegisterObjectNV");
    dx_unregister = (PfnDXUnregisterObject) SDL_GL_GetProcAddress("wglDXUnregisterObjectNV");
    dx_lock = (PfnDXLockObjects) SDL_GL_GetProcAddress("wglDXLockObjectsNV");
    dx_unlock = (PfnDXUnlockObjects) SDL_GL_GetProcAddress("wglDXUnlockObjectsNV");
    if (!dx_open || !dx_close || !dx_register || !dx_unregister || !dx_lock || !dx_unlock) {
        sysLogPrintf(LOG_WARNING, "HDR Output: the GL driver lacks WGL_NV_DX_interop2 - unavailable");
        s_failed = true;
        return false;
    }
    return true;
}

static bool create_device(SDL_Window* window) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        sysLogPrintf(LOG_WARNING, "HDR Output: no Win32 window handle - unavailable");
        s_failed = true;
        return false;
    }
    s_hwnd = info.info.win.window;

    HMODULE d3d11 = LoadLibraryA("d3d11.dll");
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    PfnD3D11CreateDevice create = d3d11 ? (PfnD3D11CreateDevice) (void*) GetProcAddress(d3d11, "D3D11CreateDevice") : NULL;
    PfnCreateDXGIFactory1 factory =
        dxgi ? (PfnCreateDXGIFactory1) (void*) GetProcAddress(dxgi, "CreateDXGIFactory1") : NULL;
    if (!create || !factory) {
        sysLogPrintf(LOG_WARNING, "HDR Output: d3d11.dll / dxgi.dll unavailable");
        s_failed = true;
        return false;
    }
    HRESULT hr = factory(__uuidof(IDXGIFactory2), (void**) &s_factory);
    if (FAILED(hr)) {
        fail("CreateDXGIFactory1", hr);
        return false;
    }
    /* the adapter driving the window's monitor: the GL context normally runs there */
    IDXGIAdapter1* adapter = NULL;
    s_factory->EnumAdapters1(0, &adapter);
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    hr = create(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &s_dev, NULL, &s_ctx);
    release(adapter);
    if (FAILED(hr)) {
        fail("D3D11CreateDevice", hr);
        return false;
    }
    IDXGIFactory5* f5 = NULL;
    if (SUCCEEDED(s_factory->QueryInterface(__uuidof(IDXGIFactory5), (void**) &f5))) {
        BOOL allow = FALSE;
        s_tearing = SUCCEEDED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))) && allow;
        f5->Release();
    }
    s_interop_dev = dx_open(s_dev);
    if (!s_interop_dev) {
        fail("wglDXOpenDeviceNV", (HRESULT) GetLastError());
        return false;
    }
    return true;
}

static bool create_chain(UINT w, UINT h) {
    DXGI_SWAP_CHAIN_DESC1 d;
    memset(&d, 0, sizeof(d));
    d.Width = w;
    d.Height = h;
    d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = 2;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    d.Flags = s_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = s_factory->CreateSwapChainForHwnd(s_dev, s_hwnd, &d, NULL, NULL, &s_chain);
    if (FAILED(hr)) {
        fail("CreateSwapChainForHwnd", hr);
        return false;
    }
    s_factory->MakeWindowAssociation(s_hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    IDXGISwapChain3* c3 = NULL;
    if (SUCCEEDED(s_chain->QueryInterface(__uuidof(IDXGISwapChain3), (void**) &c3))) {
        hr = c3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709);
        c3->Release();
        if (FAILED(hr)) {
            fail("SetColorSpace1 (scRGB)", hr);
            return false;
        }
    }
    return true;
}

extern "C" bool gfx_hdr_dxgi_target(SDL_Window* window, uint32_t w, uint32_t h, unsigned gl_texture) {
    if (s_failed || w == 0 || h == 0) return false;
    if (!s_dev && (!load_entry_points() || !create_device(window))) return false;
    if (s_chain && s_tex && s_w == w && s_h == h) return true;
    drop_target();
    if (!s_chain) {
        if (!create_chain(w, h)) return false;
    } else {
        HRESULT hr = s_chain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, s_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        if (FAILED(hr)) {
            fail("ResizeBuffers", hr);
            return false;
        }
    }
    D3D11_TEXTURE2D_DESC td;
    memset(&td, 0, sizeof(td));
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = s_dev->CreateTexture2D(&td, NULL, &s_tex);
    if (FAILED(hr)) {
        fail("CreateTexture2D", hr);
        return false;
    }
    s_interop_tex = dx_register(s_interop_dev, s_tex, gl_texture, 0x0DE1 /* GL_TEXTURE_2D */,
                                GFX_WGL_ACCESS_WRITE_DISCARD_NV);
    if (!s_interop_tex) {
        fail("wglDXRegisterObjectNV", (HRESULT) GetLastError());
        return false;
    }
    s_w = w;
    s_h = h;
    sysLogPrintf(LOG_NOTE, "HDR Output: scRGB swap chain %ux%u%s", (unsigned) w, (unsigned) h,
                 s_tearing ? " (tearing allowed)" : "");
    return true;
}

extern "C" bool gfx_hdr_dxgi_lock(void) {
    return s_interop_tex && dx_lock(s_interop_dev, 1, &s_interop_tex);
}

extern "C" bool gfx_hdr_dxgi_present(int vsync) {
    if (!s_interop_tex) return false;
    dx_unlock(s_interop_dev, 1, &s_interop_tex);
    ID3D11Texture2D* back = NULL;
    if (FAILED(s_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &back))) return false;
    s_ctx->CopyResource(back, s_tex);
    back->Release();
    const UINT flags = (!vsync && s_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    const HRESULT hr = s_chain->Present(vsync ? 1 : 0, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        fail("Present", hr);
        return false;
    }
    return true;
}

extern "C" void gfx_hdr_dxgi_shutdown(void) {
    drop_target();
    release(s_chain);
    if (s_ctx) {
        s_ctx->ClearState();
        s_ctx->Flush();
    }
    s_w = s_h = 0;
}

extern "C" bool gfx_hdr_dxgi_failed(void) {
    return s_failed;
}

/* Windows' "SDR content brightness" for the monitor: DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL (11),
 * declared by hand since older MinGW headers lack it. SDRWhiteLevel is in 1/1000 of 80 nits. */
struct GfxSdrWhiteLevel {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    ULONG SDRWhiteLevel;
};

static float sdr_white_nits(const WCHAR* gdi_name) {
    UINT32 npaths = 0, nmodes = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &npaths, &nmodes) != ERROR_SUCCESS || npaths == 0) return 0.0f;
    DISPLAYCONFIG_PATH_INFO* paths = (DISPLAYCONFIG_PATH_INFO*) calloc(npaths, sizeof(*paths));
    DISPLAYCONFIG_MODE_INFO* modes = (DISPLAYCONFIG_MODE_INFO*) calloc(nmodes ? nmodes : 1, sizeof(*modes));
    float nits = 0.0f;
    if (paths && modes && QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &npaths, paths, &nmodes, modes, NULL) == ERROR_SUCCESS) {
        for (UINT32 i = 0; i < npaths && nits == 0.0f; i++) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME src;
            memset(&src, 0, sizeof(src));
            src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            src.header.size = sizeof(src);
            src.header.adapterId = paths[i].sourceInfo.adapterId;
            src.header.id = paths[i].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS || wcscmp(src.viewGdiDeviceName, gdi_name) != 0) {
                continue;
            }
            GfxSdrWhiteLevel level;
            memset(&level, 0, sizeof(level));
            level.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE) 11;
            level.header.size = sizeof(level);
            level.header.adapterId = paths[i].targetInfo.adapterId;
            level.header.id = paths[i].targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&level.header) == ERROR_SUCCESS && level.SDRWhiteLevel > 0) {
                nits = (float) level.SDRWhiteLevel / 1000.0f * 80.0f;
            }
        }
    }
    free(paths);
    free(modes);
    return nits;
}

extern "C" bool gfx_hdr_dxgi_display(SDL_Window* window, float* peak_nits, float* white_nits) {
    *peak_nits = 0.0f;
    *white_nits = 0.0f;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) return false;
    HMONITOR mon = MonitorFromWindow(info.info.win.window, MONITOR_DEFAULTTONEAREST);
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    PfnCreateDXGIFactory1 factory =
        dxgi ? (PfnCreateDXGIFactory1) (void*) GetProcAddress(dxgi, "CreateDXGIFactory1") : NULL;
    IDXGIFactory1* f = NULL;
    if (!factory || FAILED(factory(__uuidof(IDXGIFactory1), (void**) &f))) return false;
    bool hdr = false;
    IDXGIAdapter1* a = NULL;
    for (UINT ai = 0; f->EnumAdapters1(ai, &a) != DXGI_ERROR_NOT_FOUND; ai++) {
        IDXGIOutput* o = NULL;
        for (UINT oi = 0; a->EnumOutputs(oi, &o) != DXGI_ERROR_NOT_FOUND; oi++) {
            IDXGIOutput6* o6 = NULL;
            DXGI_OUTPUT_DESC1 d;
            if (SUCCEEDED(o->QueryInterface(__uuidof(IDXGIOutput6), (void**) &o6)) && SUCCEEDED(o6->GetDesc1(&d)) &&
                d.Monitor == mon) {
                hdr = d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                *peak_nits = d.MaxLuminance;
                *white_nits = sdr_white_nits(d.DeviceName);
            }
            release(o6);
            o->Release();
        }
        a->Release();
    }
    f->Release();
    return hdr;
}

#else /* not Windows: no presenter */

extern "C" bool gfx_hdr_dxgi_target(SDL_Window*, uint32_t, uint32_t, unsigned) { return false; }
extern "C" bool gfx_hdr_dxgi_lock(void) { return false; }
extern "C" bool gfx_hdr_dxgi_present(int) { return false; }
extern "C" void gfx_hdr_dxgi_shutdown(void) {}
extern "C" bool gfx_hdr_dxgi_failed(void) { return true; }
extern "C" bool gfx_hdr_dxgi_display(SDL_Window*, float* peak_nits, float* white_nits) {
    *peak_nits = *white_nits = 0.0f;
    return false;
}

#endif

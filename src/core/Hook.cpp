#include "core/Hook.h"
#include "core/Diagnostics.h"
#include <MinHook.h>

namespace Hook {
    // Paylaşılan state
    ID3D11Device* g_dev = nullptr;
    ID3D11DeviceContext* g_ctx = nullptr;
    ID3D11RenderTargetView* g_rtv = nullptr;
    HWND                    g_hwnd = nullptr;
    WNDPROC                 g_oldWndProc = nullptr;
    bool                    g_init = false;

    // --- Overlay thread guvenligi -----------------------------------------
    // 0=idle, 1=init SURUYOR, 2=hazir, 3=vazgecildi
    volatile LONG           g_initState = 0;
    // 0=bos, 1=overlay'i bir thread isliyor (ImGui thread-safe degil!)
    volatile LONG           g_renderBusy = 0;
    // Overlay'in baglandigi swapchain; diger swapchain'lerde cizim yapmayiz.
    IDXGISwapChain*         g_swapchain = nullptr;

    // --- Unload guvenligi -------------------------------------------------
    //   ESKI HATA: unload su sirayla yapiliyordu ->
    //     MH_DisableHook -> MH_Uninitialize -> ShutdownImGui -> FreeLibrary
    //   ... ve render thread o sirada HALA bizim hook kodunun (ya da MinHook
    //   trampolininin) icinde olabiliyordu. Sonuc: serbest birakilmis
    //   trampolin'e veya unmap edilmis DLL'e atlayan thread -> oyun cokmesi.
    //   Log kaniti: cokme aninda kayit YOK; cunku artik bizim VEH/UEF'imiz
    //   de DLL ile birlikte yok olmustu.
    volatile LONG g_shuttingDown = 0;   // 1 -> overlay isi yapma, sadece gecir
    volatile LONG g_inHook       = 0;   // su an kac thread bizim hook kodunda
    UINT          g_restoreMsg   = 0;   // WndProc'u pencere thread'inde geri almak icin

    // ★ Menu'den gelen temiz cikis istegi (dllmain MainThread bunu bekler).
    volatile LONG g_ejectRequested = 0;

    void RequestEject() { InterlockedExchange(&g_ejectRequested, 1); }
    bool EjectRequested() { return InterlockedCompareExchange(&g_ejectRequested, 0, 0) != 0; }

    // Orijinal fonksiyonlar
    typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    Present_t       oPresent = nullptr;
    ResizeBuffers_t oResizeBuffers = nullptr;

    ID3D11Device* GetDevice() { return g_dev; }
    ID3D11DeviceContext* GetContext() { return g_ctx; }

    // Başka dosyalarda tanımlı
    void InitImGui(IDXGISwapChain* sc);
    void ShutdownImGui();
    HRESULT WINAPI PresentHook(IDXGISwapChain*, UINT, UINT);
    HRESULT WINAPI ResizeBuffersHook(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    bool Install() {
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = 100;
        scd.BufferDesc.Height = 100;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = GetForegroundWindow();
        scd.SampleDesc.Count = 1;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* dummy = nullptr;
        ID3D11Device* dummyDev = nullptr;
        ID3D11DeviceContext* dummyCtx = nullptr;
        D3D_FEATURE_LEVEL fl;
        const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            fls, 2, D3D11_SDK_VERSION, &scd,
            &dummy, &dummyDev, &fl, &dummyCtx);
        if (FAILED(hr)) return false;

        void** vt = *(void***)dummy;

        // ★ Unload icin: WndProc'u PENCERE THREAD'inde geri alabilmek adina
        //   benzersiz bir mesaj kimligi. (SetWindowLongPtr(GWLP_WNDPROC)'u
        //   baska thread'den cagirmak garanti sonuc vermez.)
        if (g_restoreMsg == 0)
            g_restoreMsg = RegisterWindowMessageA("BlazingInternal.RestoreWndProc");

        MH_STATUS st = MH_Initialize();
        if (st != MH_OK) {
            Diagnostics::Log("[!] MH_Initialize failed: %d", (int)st);
            dummy->Release(); dummyDev->Release(); dummyCtx->Release();
            return false;
        }

        // vt[8] = IDXGISwapChain::Present, vt[13] = ResizeBuffers
        st = MH_CreateHook(vt[8], &PresentHook, (void**)&oPresent);
        if (st != MH_OK) {
            Diagnostics::Log("[!] MH_CreateHook(Present) failed: %d", (int)st);
            dummy->Release(); dummyDev->Release(); dummyCtx->Release();
            return false;
        }

        st = MH_CreateHook(vt[13], &ResizeBuffersHook, (void**)&oResizeBuffers);
        if (st != MH_OK)
            Diagnostics::Log("[!] MH_CreateHook(ResizeBuffers) failed: %d (kritik degil)", (int)st);

        st = MH_EnableHook(MH_ALL_HOOKS);
        if (st != MH_OK) {
            Diagnostics::Log("[!] MH_EnableHook failed: %d", (int)st);
            dummy->Release(); dummyDev->Release(); dummyCtx->Release();
            return false;
        }

        Diagnostics::Log("[+] Hooks aktif (swapchain vt=0x%p)", (void*)vt);

        dummy->Release(); dummyDev->Release(); dummyCtx->Release();
        return true;
    }

    // WndProc'u GERI AL + DOGRULA.
    //   Once pencere thread'ine mesaj yollayip orada geri aldiriyoruz; bu yol
    //   hem thread-guvenli hem de kesindir. Sonra GetWindowLongPtr ile
    //   gercekten geri alindigini TEYIT ediyoruz.
    //   SendMessage ile yapildigi icin, dondugunde pencere thread'i bizim
    //   WndProc'imizda baska bir mesaji islemeye baslamis olmaz.
    static bool RestoreWndProc()
    {
        if (!g_hwnd || !g_oldWndProc) return true;

        if (IsWindow(g_hwnd) && g_restoreMsg) {
            DWORD_PTR res = 0;
            SendMessageTimeout(g_hwnd, g_restoreMsg, (WPARAM)g_oldWndProc, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &res);
        }

        bool ok = true;
        if (IsWindow(g_hwnd)) {
            WNDPROC cur = (WNDPROC)GetWindowLongPtr(g_hwnd, GWLP_WNDPROC);
            if (cur != g_oldWndProc) {
                Diagnostics::Log("[!] WndProc hala bizde (0x%p) - dogrudan deneniyor", (void*)cur);
                SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
                cur = (WNDPROC)GetWindowLongPtr(g_hwnd, GWLP_WNDPROC);
            }
            ok = (cur == g_oldWndProc);
            if (ok)
                Diagnostics::Log("[*] WndProc geri alindi");
            else
                Diagnostics::Log("[!] WndProc GERI ALINAMADI (cur=0x%p)", (void*)cur);
        }

        if (ok) g_oldWndProc = nullptr;   // basariyla birakildi
        return ok;
    }

    bool Uninstall() {
        // (1) Once "dur" de: bundan sonra gelen Present/ResizeBuffers/WndProc
        //     cagrilari bizim isimizi yapmadan gecsin.
        InterlockedExchange(&g_shuttingDown, 1);

        // (2) Su an icerde olan thread'lerin cikmasini bekle (en fazla ~3 sn).
        for (int i = 0; i < 3000; ++i) {
            if (InterlockedCompareExchange(&g_inHook, 0, 0) == 0 &&
                InterlockedCompareExchange(&g_renderBusy, 0, 0) == 0) break;
            Sleep(1);
        }
        const bool quiet = (InterlockedCompareExchange(&g_inHook, 0, 0) == 0);
        if (!quiet)
            Diagnostics::Log("[!] Render thread bizim hook'tan cikmadi (inHook=%ld)",
                (long)InterlockedCompareExchange(&g_inHook, 0, 0));

        // (3) WndProc'u geri al + DOGRULA.
        const bool wndOk = RestoreWndProc();

        // (4) Kancalari kapat. MH_Uninitialize trampolinleri serbest birakir;
        //     bu yuzden SADECE icerde thread kalmadiginda cagiriyoruz.
        MH_DisableHook(MH_ALL_HOOKS);
        if (quiet) {
            Sleep(50);
            MH_Uninitialize();
        }
        else {
            Diagnostics::Log("[!] MH_Uninitialize atlandi (icerde thread var)");
        }

        // (5) Overlay kaynaklarini birak.
        ShutdownImGui();

        return quiet && wndOk;
    }
}
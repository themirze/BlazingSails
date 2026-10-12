#include "core/Hook.h"
#include "core/Diagnostics.h"
#include "features/esp/ESP.h"
#include "sdk/Actors.h"
#include "features/aim/Aimbot.h"
#include "features/trigger/Triggerbot.h"
#include "features/magic/MagicBullet.h"
#include "sdk/Actors_World.h"
#include "ui/Menu.h"
#include "config/Config.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace Hook {
    extern ID3D11Device* g_dev;
    extern ID3D11DeviceContext* g_ctx;
    extern ID3D11RenderTargetView* g_rtv;
    extern bool                    g_init;
    extern volatile LONG           g_initState;
    extern volatile LONG           g_renderBusy;
    extern IDXGISwapChain*         g_swapchain;
    extern volatile LONG           g_shuttingDown;   // unload / soft-eject
    extern volatile LONG           g_inHook;         // su an bizim kodda olan thread sayisi

    typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(WINAPI* ResizeBuffers_t)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    extern Present_t       oPresent;
    extern ResizeBuffers_t oResizeBuffers;

    void InitImGui(IDXGISwapChain* sc);
    void PumpInputQueue();

    // Overlay bir kez istisna atarsa tekrar denemeyip kapanir; oyun yasar.
    static volatile LONG g_overlayFailed = 0;

    // -----------------------------------------------------------------------
    //  TESHIS: Present'i HANGI thread'ler cagiriyor?
    //
    //  Cokme logu (blazing_..._p15184.log) sunu gosterdi:
    //    t13080 -> [Init] Start (deneme 1) ... [Init] DONE
    //    t21780 -> [Init] Start (deneme 2) ... [Init] DONE
    //  yani IKI thread AYNI ANDA ImGui'yi init edip kullaniyordu.
    //  ImGui thread-safe degil -> bu asagidaki duzeltmelerin ana sebebi.
    // -----------------------------------------------------------------------
    static volatile LONG g_seenTids[8] = {};

    static void NotePresentThread()
    {
        const DWORD tid = (DWORD)GetCurrentThreadId();
        if (tid == 0) return;

        for (int i = 0; i < 8; ++i) {
            const LONG v = g_seenTids[i];
            if (v == (LONG)tid) return;              // bilinen thread
            if (v == 0) {
                if (InterlockedCompareExchange((volatile LONG*)&g_seenTids[i], (LONG)tid, 0) == 0) {
                    if (i == 0)
                        Diagnostics::Log("[+] Present thread: %lu", (unsigned long)tid);
                    else
                        Diagnostics::Log("[!] Present'e %d. thread de girdi: %lu  <-- ImGui tek thread ister",
                            i + 1, (unsigned long)tid);
                }
                return;
            }
        }
    }

    // -----------------------------------------------------------------------
    //  ResizeBuffers
    // -----------------------------------------------------------------------
    HRESULT WINAPI ResizeBuffersHook(IDXGISwapChain* sc, UINT bc, UINT w, UINT h,
        DXGI_FORMAT fmt, UINT flags)
    {
        // unload / soft-eject sirasinda hicbir seye dokunma.
        if (InterlockedCompareExchange(&g_shuttingDown, 0, 0) != 0)
            return oResizeBuffers(sc, bc, w, h, fmt, flags);

        InterlockedIncrement(&g_inHook);   // ★ unload bunu bekler

        // overlay hazir degil / baska swapchain -> hic karismayalim.
        if (!g_init || !g_dev || !sc || sc != g_swapchain) {
            const HRESULT hr0 = oResizeBuffers(sc, bc, w, h, fmt, flags);
            InterlockedDecrement(&g_inHook);
            return hr0;
        }

        // ★ overlay render ediliyorsa ayni device objelerine AYNI ANDA dokunma.
        if (InterlockedCompareExchange(&g_renderBusy, 1, 0) != 0) {
            const HRESULT hr0 = oResizeBuffers(sc, bc, w, h, fmt, flags);
            InterlockedDecrement(&g_inHook);
            return hr0;
        }

        // ★ KRITIK SIRA DUZELTMESI:
        //   DXGI, swapchain back-buffer'larina bizim hala ACIK bir referansimiz
        //   (g_rtv) varken ResizeBuffers'i REDDEDER (DXGI_ERROR_INVALID_CALL).
        //   Bu yuzden ONCE kendi kaynaklarimizi birakiriz, SONRA resize cagiririz.
        //   Eski kod tersini yapiyordu -> tam ekran/pencere gecisinde resize
        //   basarisiz oluyor, RTV gecersiz kaliyor ve overlay donuyor/bozuluyordu.
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
        ImGui_ImplDX11_InvalidateDeviceObjects();

        // Oyunun asil islemi HER halukarda yapilir, sonucu o dondurur.
        const HRESULT hr = oResizeBuffers(sc, bc, w, h, fmt, flags);

        __try {
            ID3D11Texture2D* bb = nullptr;
            if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) && bb) {
                g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
                bb->Release();
            }
            ImGui_ImplDX11_CreateDeviceObjects();
        }
        __except (Diagnostics::HandleOverlayException(GetExceptionInformation())) {
            InterlockedExchange(&g_overlayFailed, 1);
            Diagnostics::Log("[!] ResizeBuffers: overlay hatasi, overlay kapatildi");
        }

        InterlockedExchange(&g_renderBusy, 0);
        InterlockedDecrement(&g_inHook);
        return hr;
    }

    // Ayri fonksiyon: __try'in bulundugu fonksiyonda C++ yikim (unwind)
    // gerektiren yerel nesne bulunmamali (C2712), bu yuzden isi buraya aldik.
    static void RenderOverlay()
    {
        // ★ Game thread'inden gelen mesajlari SADECE burada, tek thread'de isle
        PumpInputQueue();

        // ★ YOL HARITASI 2: F9 ile istenen PMC/Velocity offset dump'i.
        //   Bayrak dllmain'in tus dongusunde konur; agir is ve FName
        //   cozumlemesi burada, render thread'inde yapilir. Istek yoksa
        //   fonksiyon aninda doner (maliyet ~0).
        Actors::ProcessProjectileDump();

        // ★ Dunya sayimi (census): F10 veya menu butonu bayragi.
        Actors::ProcessWorldCensus();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Feature update
        MagicBullet::Update();

        // Cizim
        ESP::Render();

        // ★ FOV circle (aim ayarlari). Eskiden Aimbot::RenderFOV() hicbir
        //   yerden cagrilmiyordu -> menude "Draw FOV circle" acik olsa da
        //   ekranda daire gorunmuyordu.
        {
            const ImGuiIO& io = ImGui::GetIO();
            Aimbot::RenderFOV(ImGui::GetBackgroundDrawList(),
                (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        }

        Menu::Render();

        ImGui::Render();
        if (g_ctx && g_rtv) {
            g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        // ★ TANİ: menünün neden görünmediğini kesinleştir. 2 sn'de bir bas.
        {
            static ULONGLONG s_last = 0;
            const ULONGLONG now = GetTickCount64();
            if (now - s_last > 2000) {
                s_last = now;
                const ImDrawData* dd = ImGui::GetDrawData();
                const ImGuiIO& io = ImGui::GetIO();
                Diagnostics::Log(
                    "[render] g_ctx=%p g_rtv=%p sc=%p drawCmds=%d disp=(%.0f,%.0f) open=%d",
                    (void*)g_ctx, (void*)g_rtv, (void*)g_swapchain,
                    dd ? dd->CmdListsCount : -1,
                    io.DisplaySize.x, io.DisplaySize.y,
                    Menu::open ? 1 : 0);
            }
        }
    }

    HRESULT WINAPI PresentHook(IDXGISwapChain* sc, UINT sync, UINT flags) {
        // ★ Unload / soft-eject: hemen gecir. Boylece unload sirasinda bizim
        //   kodda takilan thread sayisi (g_inHook) hizla 0'a iner.
        if (InterlockedCompareExchange(&g_shuttingDown, 0, 0) != 0)
            return oPresent(sc, sync, flags);

        InterlockedIncrement(&g_inHook);   // ★ unload bunu bekler

        NotePresentThread();

        // ★ YENI: ImGui tek thread ister. Sadece ILK Present thread'i
        //   (g_seenTids[0]) overlay'i render etsin; diger thread'ler kendi
        //   Present'lerini yapsin. Boylece iki thread sirasiyla render edip
        //   ImGui context'ini bozmaz (menunun gorunmemesinin sebebi buydu).
        const DWORD tid = (DWORD)GetCurrentThreadId();
        const bool isPrimary = (g_seenTids[0] == (LONG)tid);

        // ★★ KRITIK DUZELTME
        //    Overlay'i AYNI ANDA sadece BIR thread isleyebilir (g_renderBusy).
        //    Sebep: log'da iki thread ayni anda init+render yapiyordu ve
        //    cokme BlazingInternal.dll+0x4C467 = ImDrawList::PopClipRect
        //    icinde oldu -> paylasilan ImDrawList'in ic vektorlerinin
        //    (clip stack / cmd buffer) es zamanli bozulmasi.
        //    Ikinci thread artik sadece Present eder, ImGui'ye dokunmaz.
        if (isPrimary && g_overlayFailed == 0 && InterlockedCompareExchange(&g_renderBusy, 1, 0) == 0) {
            __try {
                if (!g_init) InitImGui(sc);
                // ★ sadece overlay'in baglandigi swapchain'de ciz
                if (g_init && sc == g_swapchain) {
                    RenderOverlay();
                    Diagnostics::Heartbeat();
                }
            }
            __except (Diagnostics::HandleOverlayException(GetExceptionInformation())) {
                InterlockedExchange(&g_overlayFailed, 1);
                Diagnostics::Log("[!] Overlay devre disi birakildi (oyun korunuyor)");
            }
            InterlockedExchange(&g_renderBusy, 0);
        }

        // ★ oPresent (MinHook trampolini) de bu blogun icinde: MH_Uninitialize
        //   o bellegi serbest biraktigi icin unload'un bunu beklemesi SART.
        const HRESULT hr = oPresent(sc, sync, flags);
        InterlockedDecrement(&g_inHook);
        return hr;
    }

} // namespace Hook

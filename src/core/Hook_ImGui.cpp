#include "core/Hook.h"
#include "core/Diagnostics.h"
#include "ui/Menu.h"
#include "render/Assets.h"
#include "config/Config.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <atomic>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace Hook {
    extern ID3D11Device* g_dev;
    extern ID3D11DeviceContext* g_ctx;
    extern ID3D11RenderTargetView* g_rtv;
    extern HWND                    g_hwnd;
    extern WNDPROC                 g_oldWndProc;
    extern bool                    g_init;
    extern volatile LONG           g_initState;    // 0=idle 1=init suruyor 2=hazir 3=vazgecildi
    extern IDXGISwapChain*         g_swapchain;
    extern volatile LONG           g_shuttingDown; // 1 -> unload / soft-eject
    extern UINT                    g_restoreMsg;   // WndProc geri alma mesaji

    // =======================================================================
    //  Pencere mesaji kuyrugu  (game/mesaj thread'i  ->  render thread'i)
    //
    //  NEDEN: UE4'te Present render (RHI) thread'inde, pencere mesajlari ise
    //  oyunun kendi thread'inde pompalanir. ImGui thread-safe DEGILDIR.
    //  WndProc'tan dogrudan ImGui'ye yazmak, render thread'i ayni anda
    //  io.InputEventsQueue'yu okurken o kuyrugun reallocate olmasina
    //  (heap bozulmasi -> donma/cokme) yol acar.
    //
    //  COZUM: WndProc sadece kuyruga yazar (SPSC), render thread'i
    //  NewFrame'den ONCE kuyrugu bosaltir. Boylece ImGui'ye tek thread dokunur.
    // =======================================================================
    namespace {

        struct WinMsg { UINT m; WPARAM w; LPARAM l; };

        constexpr int kQueueSize = 512;   // ~12 KB; fare 125 Hz icin fazlasiyla yeterli

        WinMsg            g_queue[kQueueSize];
        std::atomic<LONG> g_head{ 0 };     // uretici (game thread)
        LONG              g_tail = 0;      // tuketici (render thread)
        std::atomic<LONG> g_dropped{ 0 };
        volatile LONG     g_firstKeyLogged = 0;   // teshis: ilk tus mesaji log'landi mi

        // Uretici taraf: sadece bu thread yazar.
        inline void PushInputMessage(UINT m, WPARAM w, LPARAM l)
        {
            const LONG head = g_head.load(std::memory_order_relaxed);
            const LONG next = (head + 1) % kQueueSize;

            if (next == g_tail) {                  // kuyruk dolu: mesaji at
                g_dropped.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            g_queue[head].m = m;
            g_queue[head].w = w;
            g_queue[head].l = l;
            g_head.store(next, std::memory_order_release);
        }

    } // namespace

    // Tuketici taraf: SADECE render thread'inden cagrilir (Hook_Present).
    //
    // ★ INSERT (menu ac/kapa) ARTIK BURADAN islenir - GetAsyncKeyState'ten
    //   degil. Sebep: bu yol oyunun GERCEK mesaj kuyrugudur; tus fiziksel
    //   olarak basildiginda WM_KEYDOWN kesin olarak gelir. Onceki kod
    //   GetAsyncKeyState ile render (RHI) thread'inden yokluyordu ve
    //   tus pratikte hic yakalanmiyordu ("Insert menuyu kapatmiyor").
    //   Ayrica ayni degeri iki farkli mekanizma degistirmedigi icin
    //   cift tetikleme (ac-kapa = degisiklik yok) riski de yoktur.
    void PumpInputQueue()
    {
        const LONG head = g_head.load(std::memory_order_acquire);
        LONG tail = g_tail;

        // ★ Sadece render thread'i erisir -> static yerel yeterli (kilit gerekmez).
        //   Tek fiziksel Insert basisi = tek toggle garantisi icin debounce zamani.
        static ULONGLONG s_lastToggleTick = 0;

        while (tail != head)
        {
            const WinMsg msg = g_queue[tail];

            if (msg.m == WM_KEYDOWN || msg.m == WM_SYSKEYDOWN)
            {
                // Teshis: hangi tuslar geldigini BIR KEZ yaz (yol calisiyor mu?)
                if (InterlockedCompareExchange(&g_firstKeyLogged, 1, 0) == 0)
                    Diagnostics::Log("[input] ilk WM_KEYDOWN alindi: vk=0x%02X (menuKey=0x%02X)",
                        (unsigned)msg.w, cfg.menu.toggleKey);

                // bit30 = tekrarlanan (auto-repeat) tus -> yalnizca ilk basista tetikle
                const bool autoRepeat = (msg.l & (LPARAM)(1ull << 30)) != 0;

                // ★ Ek emniyet (debounce): bazi oyunlar ayni tusu birden fazla
                //   pencereye yonlendirir veya mesaji cogaltir. 250 ms'lik
                //   pencere, tek fiziksel basista TEK toggle garantiler.
                //   Cift tetikleme (true->false->true) menu hic degismemis gibi
                //   gorunur; tam da "Insert menuyu kapatmiyor" belirtisi budur.
                const ULONGLONG now = GetTickCount64();
                const bool debounced = (now - s_lastToggleTick) < 250;

                if (!autoRepeat && !debounced && cfg.menu.toggleKey != 0 &&
                    (int)msg.w == cfg.menu.toggleKey)
                {
                    s_lastToggleTick = now;
                    Menu::open = !Menu::open;
                    Diagnostics::Log("[input] menu toggle -> %s  (tek kaynak: WM_KEYDOWN)",
                        Menu::open ? "ACIK" : "KAPALI");
                }
            }

            ImGui_ImplWin32_WndProcHandler(g_hwnd, msg.m, msg.w, msg.l);
            tail = (tail + 1) % kQueueSize;
        }

        g_tail = tail;
    }

    static HWND FindGameWindow() {
        struct Ctx { DWORD pid; HWND hwnd; DWORD area; };
        Ctx ctx = { GetCurrentProcessId(), nullptr, 0 };

        EnumWindows([](HWND h, LPARAM lp) -> BOOL {
            Ctx* c = (Ctx*)lp;
            DWORD pid = 0;
            GetWindowThreadProcessId(h, &pid);
            if (pid != c->pid) return TRUE;
            if (!IsWindowVisible(h)) return TRUE;
            if (GetWindow(h, GW_OWNER)) return TRUE;

            // ★ Sadece buyuk pencereleri kabul et (splash degil)
            RECT r;
            GetClientRect(h, &r);
            DWORD area = (DWORD)((r.right - r.left) * (r.bottom - r.top));
            if (area > c->area) {
                c->hwnd = h;
                c->area = area;
            }
            return TRUE;
            }, (LPARAM)&ctx);

        if (ctx.hwnd) {
            wchar_t title[256] = {};
            GetWindowTextW(ctx.hwnd, title, 256);
            Diagnostics::Log("[Hook] Game window: %ls (area=%lu)", title, ctx.area);
        }
        return ctx.hwnd;
    }

    // ★ Bu fonksiyon game thread'inde calisir: ImGui'ye DOKUNMUYORUZ.
    //   Sadece kuyruga atip orijinal WndProc'a devrediyoruz.
    static LRESULT WINAPI WndProcHook(HWND h, UINT m, WPARAM w, LPARAM l) {
        // ★★ WndProc'u geri alma istegi. Bunu PENCERE THREAD'inde yapiyoruz:
        //    SetWindowLongPtr(GWLP_WNDPROC)'u baska thread'den cagirmak
        //    garanti degildir; burada kesin ve guvenli sekilde geri alinir.
        //    Mesaj SendMessage ile yollandigi icin, bu handler dondugunde
        //    pencere thread'i WndProc'imizda baska bir mesaj islemiyor olur.
        if (m == g_restoreMsg && h == g_hwnd) {
            SetWindowLongPtr(h, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
            return CallWindowProc(g_oldWndProc, h, m, w, l);
        }

        // ★ Unload / soft-eject sirasinda kuyruga YAZMA (artik okuyan yok).
        if (InterlockedCompareExchange(&g_shuttingDown, 0, 0) != 0)
            return CallWindowProc(g_oldWndProc, h, m, w, l);

        if (g_init && h == g_hwnd)
            PushInputMessage(m, w, l);

        return CallWindowProc(g_oldWndProc, h, m, w, l);
    }

    // -----------------------------------------------------------------------
    //  Init
    // -----------------------------------------------------------------------
    static void ReleaseDeviceRefs()
    {
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
        if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
        if (g_dev) { g_dev->Release(); g_dev = nullptr; }
    }

    // Basarili/basarisiz doner. State yonetimi cagiranda (InitImGui).
    static bool InitImGuiImpl(IDXGISwapChain* sc)
    {
        if (!sc) { Diagnostics::Log("[Init] FAIL: swapchain null"); return false; }

        if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_dev)) || !g_dev) {
            Diagnostics::Log("[Init] FAIL: GetDevice"); return false;
        }
        g_dev->GetImmediateContext(&g_ctx);
        if (!g_ctx) {
            Diagnostics::Log("[Init] FAIL: GetImmediateContext");
            ReleaseDeviceRefs();
            return false;
        }

        ID3D11Texture2D* bb = nullptr;
        if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb)) || !bb) {
            Diagnostics::Log("[Init] FAIL: GetBuffer (backbuffer null)");
            ReleaseDeviceRefs();
            return false;
        }
        const HRESULT hrRtv = g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv);
        bb->Release();
        if (FAILED(hrRtv) || !g_rtv) {
            Diagnostics::Log("[Init] FAIL: CreateRenderTargetView 0x%08lX", (unsigned long)hrRtv);
            ReleaseDeviceRefs();
            return false;
        }
        Diagnostics::Log("[Init] Device + RTV ok (swapchain=%p)", (void*)sc);

        g_hwnd = FindGameWindow();
        if (!g_hwnd) {
            Diagnostics::Log("[Init] FAIL: no hwnd");
            ReleaseDeviceRefs();
            return false;
        }

        // ★★★ 1. Context
        ImGui::CreateContext();
        ImGuiContext* ctx = ImGui::GetCurrentContext();
        if (!ctx) {
            Diagnostics::Log("[Init] FAIL: no context");
            ReleaseDeviceRefs();
            return false;
        }
        Diagnostics::Log("[Init] Context=0x%p", ctx);

        // ★★★ 2. IO ayarlari (font YUKLEMEDEN ONCE)
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;   // ★ kritik - disk IO'yu kapat
        io.LogFilename = nullptr;

        // ★★★ 3. Backend'ler (Win32 once, DX11 sonra)
        if (!ImGui_ImplWin32_Init(g_hwnd)) {
            Diagnostics::Log("[Init] FAIL: ImplWin32");
            ImGui::DestroyContext();
            ReleaseDeviceRefs();
            return false;
        }
        Diagnostics::Log("[Init] Win32 backend ok");

        if (!ImGui_ImplDX11_Init(g_dev, g_ctx)) {
            Diagnostics::Log("[Init] FAIL: ImplDX11");
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            ReleaseDeviceRefs();
            return false;
        }
        Diagnostics::Log("[Init] DX11 backend ok");

        // ★★★ 4. Font atlasini manuel build et (KRITIK)
        io.Fonts->Build();
        Diagnostics::Log("[Init] Font atlas built");

        // ★★★ 5. WndProc
        g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
        Diagnostics::Log("[Init] WndProc hooked");

        // ★★★ 6. Menu init (en son)
        Menu::Init();
        Diagnostics::Log("[Init] Menu::Init done");

        return true;
    }

    void InitImGui(IDXGISwapChain* sc) {
        // Hazir veya vazgecildi -> is yok.
        // ★ Atomik OKUMA: ayni degisken baska thread'lerde Interlocked* ile
        //   degistiriliyor; duz okuma veri yarisi teskil eder (uyari C28112).
        //   No-op takas ile okuyoruz: deger degismez, yalnizca okunur.
        const LONG st = InterlockedCompareExchange(&g_initState, 0, 0);
        if (st == 2 || st == 3) return;

        // ★★ KRITIK: ayni anda SADECE BIR thread init edebilir.
        //    (Eski kodda iki thread birden giriyordu: "deneme 1"/"deneme 2",
        //     iki kez CreateContext + iki kez WndProc + iki kez backend.)
        if (InterlockedCompareExchange(&g_initState, 1, 0) != 0) {
            Diagnostics::Log("[Init] atlandi (baska thread init ediyor)");
            return;
        }

        static volatile LONG s_attempts = 0;
        const LONG n = InterlockedIncrement(&s_attempts);
        if (n > 5) {
            InterlockedExchange(&g_initState, 3);
            Diagnostics::Log("[Init] cok deneme - vazgecildi");
            return;
        }

        Diagnostics::Log("[Init] Start (deneme %ld, thread %lu)",
            (long)n, (unsigned long)GetCurrentThreadId());

        if (InitImGuiImpl(sc)) {
            g_swapchain = sc;
            g_init      = true;
            InterlockedExchange(&g_initState, 2);
            Diagnostics::Log("[Init] DONE");
        }
        else {
            // sonraki karede tekrar denenebilsin (basarisiz init kilitlenmesin)
            InterlockedExchange(&g_initState, 0);
            Diagnostics::Log("[Init] BASARISIZ - sonraki karede tekrar denenecek");
        }
    }

    void ShutdownImGui() {
        // ★ WndProc geri alma artik Hook::Uninstall() icindeki
        //   RestoreWndProc()'ta, DOGRULANARAK yapiliyor. Burada tekrar
        //   denemek yaniltici olurdu ("geri alindi" sanip unload etmek).
        if (g_init) {
            Assets::Shutdown();
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        ReleaseDeviceRefs();
        g_init = false;
    }

} // namespace Hook

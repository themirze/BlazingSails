#include <Windows.h>
#include <cstdio>
#include "core/Diagnostics.h"
#include "core/Hook.h"
#include "core/Game.h"
#include "ui/Menu.h"
#include "config/Config.h"
#include "sdk/Actors.h"
#include "sdk/Actors_World.h"

static HMODULE g_selfModule = nullptr;   // DLL tutamacini saklar (unload icin gerekli)

static DWORD WINAPI MainThread(LPVOID) {
    // ★ En basta teshis: log dosyasi + crash handler.
    //   Konsol gorunmese bile artik her sey C:\BlazingInternal\logs altinda.
    Diagnostics::Install();

#ifndef NDEBUG
    AllocConsole();
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
#endif

    Diagnostics::Log("=== BlazingInternal === (pid=%lu)", (unsigned long)GetCurrentProcessId());

    Sleep(3000);

    if (!Game::Attach()) {
        Diagnostics::Log("[!] Attach failed (BlazingSails-Win64-Shipping.exe bulunamadi)");
        return 1;
    }
    Diagnostics::Log("[+] Attached. Base=0x%llX", (unsigned long long)Game::moduleBase);

    if (!Hook::Install()) {
        Diagnostics::Log("[!] Hook install failed");
        return 1;
    }
    Diagnostics::Log("[+] Hook installed");

    Game::ReadGlobals();
    Diagnostics::Log("[+] Globals: GWorld=0x%llX GObjects=0x%llX GNames=0x%llX",
        (unsigned long long)Game::GWorld,
        (unsigned long long)Game::GObjects,
        (unsigned long long)Game::GNames);

    // ★★ MENU AC/KAPA ARTIK BURADA YAPILMAZ ★★
    //   Buradaki GetAsyncKeyState(Insert) yolu ile Hook::PumpInputQueue()
    //   icindeki WM_KEYDOWN yolu AYNI ANDA calisiyordu. Tek Insert basisi iki
    //   kez toggle ediyordu (true -> false -> true) ve sonuc degismemis gibi
    //   gorunuyordu. Log kaniti: ust uste "[input] menu toggle -> ACIK"
    //   satirlari -> durum hep true'da kilitli kaliyordu. Cift tetikleme tam
    //   olarak "Insert menuyu kapatmiyor" belirtisini uretir.
    //   Artik TEK kaynak var: oyunun mesaj kuyrugundan gelen WM_KEYDOWN
    //   (bkz. Hook::PumpInputQueue, Hook_ImGui.cpp).
    bool prevExit = false;
    bool prevF9   = false;
    bool prevF10  = false;

    while (true) {
        // END ile cik (overlay kapaliyken de calismali -> polling burada uygun)
        bool ex = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (ex && !prevExit) break;
        prevExit = ex;

        // ★ Menu -> "Clean quit" butonu. Menu butonu bayragi kurar; burada
        //   gorunce END tusuyla AYNI guvenli unload yolunu isletiyoruz
        //   (asagidaki Uninstall + FreeLibraryAndExitThread). Boylece oyundan
        //   cikmadan DLL ayrilir ve guncellenen build tekrar inject edilebilir.
        if (Hook::EjectRequested()) {
            Diagnostics::Log("[input] menu -> CLEAN QUIT istendi, kancalar sokuluyor");
            break;
        }

        // ★ YOL HARITASI 2: F9 -> PMC + Velocity offset dump'i.
        //   Burada SADECE bayrak konur; agir is (aktor taramasi + FName
        //   cozumlemesi) render thread'inde Actors::ProcessProjectileDump()
        //   icinde yapilir. Sebep: ayni duruma iki thread'den dokunmak
        //   menu toggle'da bize cokme/yaris olarak geri donmustu.
        bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (f9 && !prevF9) {
            Actors::RequestProjectileDump();
            Diagnostics::Log("[input] F9 -> projectile offset dump istendi");
        }
        prevF9 = f9;

        // ★ F10 -> dunya sayimi (census): sinif adlari + gemi/silah/anchor.
        bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (f10 && !prevF10) {
            Actors::RequestWorldCensus();
            Diagnostics::Log("[input] F10 -> dunya sayimi (census) istendi");
        }
        prevF10 = f10;

        Sleep(10);
    }

    Diagnostics::Log("[*] Unloading (istisna toplami=%ld)", Diagnostics::AvCount());

#ifndef NDEBUG
    if (f) fclose(f);
    FreeConsole();
#endif

    // ★★ GUVENLI SOKME (bkz. Hook::Uninstall):
    //    (a) render thread bizim hook kodundan cikana kadar bekler,
    //    (b) WndProc'u pencere thread'inde geri alip DOGRULAR,
    //    (c) ancak her sey teyit edilirse unload'a izin verir.
    //    Donus false ise WndProc'tan bize acik bir referans kalmis olabilir;
    //    o durumda DLL'i ASLA unload etmiyoruz.
    // Kancalari sok (render thread beklenir, WndProc geri alinip DOGRULANIR).
    (void)Hook::Uninstall();

    // ★★ KRITIK: VEH + UEF'i geri al. DLL yuklu kalsa bile, artik surecin
    //    istisna isleyisine mudahale etmiyoruz (oyunun kendi crash handler'i
    //    tekrar devrede).
    Diagnostics::Uninstall();

    // ★★ FreeLibrary YAPMIYORUZ  (ESKI HATA):
    //    Present/ResizeBuffers (MinHook) + WndProc hook'layan ve arka planda
    //    overlay thread'i calistiran bir DLL'i CANLI surecte unload etmek
    //    TEMELDEN riskli. Kancalar kapali olsa bile, baska bir thread o anda
    //    MinHook trampolininin ya da bizim kodumuzun icinde olabilir;
    //    FreeLibrary bu bellegi serbest birakir -> o thread serbest birakilmis
    //    adrese atlar -> OYUN ANINDA COKER. Log kaniti: "clean quit" sonrasi
    //    oyunun da kapanmasi tam olarak buydu.
    //    Bu yuzden: DLL surecte yuklu birakilir, kancalar kapali, overlay
    //    durdurulmus -> oyun calismaya DEVAM EDER.
    Diagnostics::Log("[*] Kancalar sokuldu, overlay durduruldu. DLL yuklu birakildi (guvenli).");
    Diagnostics::Log("[*] Guncelleme icin: oyunu yeniden baslat, yeni build'i inject et");
    Diagnostics::Log("[*]   (yuklu durumdaki DLL dosyasi Windows tarafindan kilitlidir, degistirilemez)");
    ExitThread(0);
    return 0;
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_selfModule = hMod;   // ★ sakla
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

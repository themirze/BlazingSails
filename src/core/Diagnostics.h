#pragma once
#include <Windows.h>

// ---------------------------------------------------------------------------
//  Teshis katmani
//
//  Neden gerekli: "oyun donuyor, birkac saniye sonra kapaniyor" tablosu
//  genelde render thread'inde olusan bir istisna (exception) ve ardindan
//  oyunun kendi crash reporter'inin devreye girmesidir. Konsol log'u
//  cogu zaman gorunmez/kaybolur, bu yuzden kalici bir log dosyasi ve
//  minidump uretiyoruz.
//
//  - Install()   : log dosyasini acar, VEH + UnhandledExceptionFilter kurar
//  - Log()        : zaman damgasi + thread id ile dosyaya/konsola yazar
//  - Heartbeat()  : render thread'inden cagrilir; thread'in yasadigini kanitlar
// ---------------------------------------------------------------------------
namespace Diagnostics {

    void        Install();
    // ★ Temiz sokme: VEH + UEF geri alinir. DLL unload edilmeden ONCE
    //   MUTLAKA cagrilmali; aksi halde bu handler'lar serbest birakilmis
    //   DLL bellegine isaret eder ve sonraki bir istisnada oyun coker.
    void        Uninstall();
    void        Log(const char* fmt, ...);
    void        Heartbeat();
    const char* LogPath();
    long        AvCount();

    // __except filtresi olarak kullanilir: istisnayi loglar + minidump yazar
    // ve EXCEPTION_EXECUTE_HANDLER dondurur.
    int         HandleOverlayException(EXCEPTION_POINTERS* ep);

} // namespace Diagnostics

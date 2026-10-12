#pragma once
#include <Windows.h>
#include <d3d11.h>

namespace Hook {
    bool Install();

    // Kancalari + WndProc'u GUVENLI sirayla geri alir.
    //   true  : geri alma dogrulandi (DLL unload edilebilir)
    //   false : geri alma DOGRULANAMADI -> DLL mutlaka YUKLU birakilmali
    bool Uninstall();

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetContext();

    // ★ Menu -> main thread TEMIZ CIKIS istegi.
    //   Menu butonu sadece bu bayragi kurar (thread-safe). dllmain'deki
    //   MainThread dongusu bayragi gorunce END tusuyla AYNI kanitlanmis
    //   unload yolunu isletir: Hook::Uninstall() -> FreeLibraryAndExitThread.
    //   Boylece "oyundan cik/gir" yapmadan DLL ayrilir ve yeni build
    //   tekrar inject edilebilir.
    void RequestEject();
    bool EjectRequested();
}
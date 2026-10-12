#pragma once
#include "sdk/ItemClasses.h"

// Global settings bound to the menu. Feature modules read from `cfg`.

struct Color4
{
    float r, g, b, a;
};

struct Config
{
    struct Esp
    {
        // Players
        bool   enabled        = true;
        bool   box            = true;
        int    boxStyle       = 0;      // 0 = Full, 1 = Corners, 2 = Filled
        bool   skeleton       = false;
        bool   name           = true;
        bool   health         = true;
        bool   distance       = true;
        bool   snaplines      = false;
        bool   visibleCheck   = true;
        bool   ignoreTeam     = true;
        float  maxDistance    = 300.0f;
        Color4 colVisible     = { 0.30f, 0.90f, 0.55f, 1.0f };
        Color4 colHidden      = { 0.95f, 0.35f, 0.40f, 1.0f };
        Color4 colSkeleton    = { 1.00f, 1.00f, 1.00f, 0.85f };

        // Items
        //   items    : ana anahtar (oyuncu ESP'sinden BAGIMSIZ calisir)
        //   itemOn[] : her item turu icin ayri anahtar - hepsi varsayilan KAPALI
        bool   items          = false;
        bool   itemOn[ItemDB::kItemCount] = {};
        bool   itemShowDist   = true;
        float  itemDistance   = 120.0f;
        Color4 colItems       = { 0.98f, 0.80f, 0.30f, 1.0f };

        // Ships  (dusman gemi isaretleri; sinif adi belirtecleriyle bulunur)
        bool   ships          = false;
        float  shipsDistance  = 2000.0f;
        Color4 colShips       = { 0.55f, 0.75f, 1.00f, 1.0f };
    } esp;

    struct Aim
    {
        bool   enabled        = false;
        int    key            = 0x02;   // VK_RBUTTON
        int    bone           = 0;      // 0 = Head, 1 = Neck, 2 = Chest, 3 = Nearest
        int    priority       = 0;      // 0 = Crosshair, 1 = Distance, 2 = Health
        float  fov            = 8.0f;
        float  smooth         = 5.0f;
        float  maxDistance    = 250.0f;
        bool   visibleOnly    = true;
        bool   ignoreTeam     = true;
        bool   drawFov        = true;
        Color4 colFov         = { 1.0f, 1.0f, 1.0f, 0.35f };

        bool   trigger        = false;
        int    triggerKey     = 0x12;   // VK_MENU
        float  triggerDelay   = 40.0f;  // ms

        // --- Magic bullet (Proj_Velocity = actor+0x258, OLCULDU) ---------
        bool   magicBullet    = false;
        int    mbKey          = 0;       // tus atama; 0 (bossa) = HEP ACIK (hold yok)
        bool   mbOnlyNear     = false;   // v20: VARSAYILAN KAPALI (acikken mermi
                                         //      60m'yi gecince filtreden duser ve
                                         //      magic bullet ~1 kare calisirdi)
        float  mbNearRadius   = 60.0f;   // m  (mbOnlyNear icin)
        float  mbSpeedMul     = 1.0f;    // hiz carpani (1.0 = hiz korunur)
        float  mbAimZOffset   = 40.0f;   // hedef gogsune nisan (cm)
        float  mbMaxTargetDist= 300.0f;  // m  hedef arama yaricapi
        bool   mbEnemiesOnly  = false;   // isEnemy guvenilmez -> varsayilan kapali
        bool   mbLog          = false;   // saniyede 1 teshis satiri

        // ★ BUG FIX: hedef secimi. Eskiden "yerel oyuncuya EN YAKIN karakter"
        //   secilirdi; bu, NISAN ALINAN dusman ile ayni degildir (birden fazla
        //   dusmanda mermi yanlis kisiye kivrilir -> "calismiyor" hissi).
        //   Artik oncelik: CROSSHAIR'e (kamera ileri yonu) en yakin karakter.
        bool   mbUseCrosshair = true;    // false -> eski davranis (oyuncuya en yakin)
        // YARIM aci (crosshair'den kenara). 180 = koni siniri YOK (tum on yarim kure).
        // Magic, bu aci disindaki hedefleri eler. 180 -> her zaman crosshair'e en yakin dusmani secer.
        float  mbConeDeg      = 180.0f;

        // ★ KALIBRASYON MODU (gozlemci): acikken MagicBullet HICBIR SEY YAZMAZ.
        //   Amac: arkadasina / bilinen silahlarla ates edip F9 dump ile HER
        //   silahin velocity offsetini temiz olcmek. Yazma kapali oldugu icin
        //   mermi dogal ucar, offset tespiti kirlenmez, cokme riski sifirdir.
        bool   mbCalibrate    = false;
    } aim;

    struct Misc
    {
        bool   crosshair      = false;
        int    crosshairStyle = 0;      // 0 = Cross, 1 = Dot, 2 = Circle
        float  crosshairSize  = 6.0f;
        Color4 colCrosshair   = { 0.30f, 0.90f, 0.55f, 1.0f };
        bool   watermark      = true;
        bool   fpsCounter     = true;
        bool   streamProof    = false;
    } misc;

    struct Menu
    {
        int    toggleKey      = 0x2D;   // VK_INSERT
        Color4 accent         = { 1.00f, 0.42f, 0.21f, 1.0f };
        bool   animations     = true;
    } menu;

    bool mbAlsoReplicated = false;   // replike alana da yaz (gorunurluk)
};

inline Config cfg;

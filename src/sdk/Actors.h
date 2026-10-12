#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "sdk/UE4_Types.h"

namespace Actors {

    struct Character {
        uintptr_t actor = 0;
        uintptr_t rootComponent = 0;
        uintptr_t mesh = 0;
        FVector   worldPos;
        float     health = 0.0f;
        float     maxHealth = 0.0f;
        bool      isDead = false;
        bool      isEnemy = false;
        bool      isLocalPlayer = false;
        char      name[64] = {};
    };

    // Item: itemId = ItemDB::kItems indeksi (sinif adiyla BIREBIR eslesme).
    // Menu bu indeks uzerinden tur bazli ac/kapa yapar.
    struct Item {
        uintptr_t   actor = 0;
        std::string className;          // gosterim etiketi (ItemDB label)
        FVector     worldPos;
        int         itemId = -1;        // -1 = eslesmedi
    };

    // -------------------------------------------------------------------
    //  TESHIS: "ESP calismiyor" vakasinda zincirin NERESINDE koptugunu
    //  olcmek icin tarama istatistikleri. ESP katmani bunlari log'a yazar.
    //      count == 0            -> ULevel::Actors offset'i yanlis
    //      countRejected == true -> sayi sacma (>16384) -> offset yanlis
    //      actors var, matched==0-> FName cozumlemesi / sinif haritasi uyusmuyor
    // -------------------------------------------------------------------
    struct ScanInfo {
        uintptr_t level = 0;                 // UWorld::PersistentLevel
        uintptr_t data = 0;                  // TArray<AActor*> veri isaretcisi
        int32_t   count = 0;                 // ham TArray sayisi
        bool      countRejected = false;     // sayi sacma -> tarama iptal edildi
        int       matched = 0;               // sinif haritasini gecen aktor
        int       clsRead = 0;               // cozulen sinif adi sayisi
        int       clsBad = 0;                // "?" donen (cozulemeyen) isim
        int       sampleCount = 0;           // samples[] icinde kac ornek var
        char      samples[8][48] = {};       // ilk 8 farkli sinif adi
    };

    extern ScanInfo g_charInfo;
    extern ScanInfo g_itemInfo;

    std::vector<Character> GetCharacters();
    std::vector<Item>      GetItems();

    // ======================================================================
    //  MERMI LISTESI  --  MagicBullet (YOL HARITASI ADIM 3)
    //
    //  Her karede cagrilabilir: iceride ~150 ms onbelleklenir ve yalnizca
    //  IsProjectileClass() gecen (BP_Projectile*) aktorler doner.
    //  Hiz alani burada OKUNMAZ; MagicBullet canli okur/yazar
    //  (Offsets::Proj_Velocity = actor+0x258).
    // ======================================================================
    struct Projectile {
        uintptr_t   actor = 0;
        std::string className;
        FVector     pos;            // root bileseninin dunya konumu
    };

    std::vector<Projectile> GetProjectiles();

    // ======================================================================
    //  MERMI (PROJECTILE)  --  YOL HARITASI ADIM 1
    //
    //  Mermi siniflari isim oneki ile ayrilir: "BP_Projectile" (bazi
    //  build'lerde "ABP_"/"UBP_"). ★ DIKKAT: UE, "A"/"U" sinif onekini FName
    //  havuzuna YAZMAZ; bu oyunda log'da gorulen gercek adlar
    //      BP_ProjectileCrossbowArrow_C
    //      BP_ProjectileBlunderbussNew_C
    //  bicimindedir. Trail ve PathSphere parcalari mermi DEGILDIR
    //  (gorsel/iz bilesenleri), haric tutulur.
    //
    //  Bu filtre listedeki 17 projectile'dan 15'ini yakalar.
    // ======================================================================
    bool IsProjectileClass(const std::string& n);

    // ======================================================================
    //  TESHIS / DUMP  --  YOL HARITASI ADIM 2
    //
    //  Amac: "PMC offset + Velocity offset bul". Mermi aktorunde
    //  ProjectileMovementComponent (PMC) isaretcisinin hangi offset'te
    //  oldugunu ve PMC icindeki Velocity (FVector) alanini ISMIYLE ve
    //  KALIBIYLA (3 ardisik float, magnitude 1000..50000) bulur.
    //
    //  NEDEN IKI FONKSIYON:
    //    Dump agir bir istir (aktor basina ~1000 offset + FName cozumlemesi)
    //    ve FName onbellegine dokunur. Tarama/ESP ile AYNI thread'de calismali.
    //    F9'u dinleyen thread (dllmain/MainThread) bu yuzden yalnizca BAYRAK
    //    koyar; asil isi render thread'inde ProcessProjectileDump() yapar.
    //    (Menu toggle'da yasadigimiz "iki thread ayni duruma dokunuyor ->
    //     tek basis iki kez toggle" hatasinin tekrarlanmamasi icin.)
    // ======================================================================
    void RequestProjectileDump();    // herhangi bir thread: sadece bayrak koyar
    void ProcessProjectileDump();    // render thread: bayrak varsa dump'i yapar
    bool IsProjectileDumpPending();
}

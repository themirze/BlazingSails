#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "sdk/UE4_Types.h"

// ===========================================================================
//  DUNYA (WORLD) MODULU  --  census + gemi (ship) listesi
//
//  AMAC
//    Oyundaki TUM actor siniflarini tek seferde dokmek (silah/gemi/anchor
//    arastirmasi icin) ve gemi aktorlerini ESP icin surekli listelemek.
//
//  NEDEN
//    Simdiye kadar mermi offsetlerini F9 dump'i ile aradik. Ayni mantigi
//    "dunyada ne var?" sorusuna uyguluyoruz: sinif adi histogrami bize
//    hangi gemi/silah/anchor siniflarinin GERCEK adlarini verir. Boylece
//    gemi ESP'si ve anchor gibi isler tahminle degil VERIYLE yapilir.
//
//  THREAD MODELI (mermi dump'i ile ayni)
//    RequestWorldCensus() : herhangi bir thread -> yalnizca BAYRAK kurar
//    ProcessWorldCensus() : render thread -> bayrak varsa agir is yapar
//    (Iki thread ayni duruma dokunmaz; menu toggle cift tetikleme hatasi
//     burada da tekrarlanmasin diye.)
// ===========================================================================

namespace Actors {

    // ---- Census (tek seferlik agir tarama) --------------------------------
    void RequestWorldCensus();     // bayrak kur (tus/menu)
    void ProcessWorldCensus();     // render thread: bayrak varsa dok
    bool IsWorldCensusPending();

    // ---- Gemi (ship) listesi ---------------------------------------------
    //  Sinif adi gemi belirteclerinden birini iceren aktorler (~500 ms onbellek).
    struct Ship {
        uintptr_t   actor = 0;
        std::string cls;
        FVector     pos;   // root bileseni dunya konumu
    };
    std::vector<Ship> GetShips();

    // Sinif adi gemi gibi mi? (ESP ve census ayni testi kullanir)
    bool IsShipClass(const std::string& n);

}

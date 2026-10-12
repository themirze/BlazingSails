#pragma once
#include <cstdint>
#include "sdk/Actors.h"

namespace ESP {
    // -------------------------------------------------------------------
    //  TESHIS: karakter ciziminin hangi asamada eledigi. "ESP calismiyor"
    //  sikayetini tahminle degil sayiyla cozmek icin her kare doldurulur,
    //  saniyede bir log'a yazilir.
    //      eslenen>0 ama cizilen=0 -> asagidaki skip sayaclarindan biri
    //      hepsini yiyor; hangisi oldugu log'da acikca gorunur.
    // -------------------------------------------------------------------
    struct Diag {
        int  listed = 0;        // GetCharacters() kac kayit dondu
        int  drawn = 0;         // ekrana gercekten cizilen
        int  skipRoot = 0;      // rootComponent okunamadi
        int  skipLocal = 0;     // kendi oyuncumuz
        int  skipDead = 0;      // isDead==true        (offset supheli: 0x8F6)
        int  skipEnemy = 0;     // ignoreTeam && !isEnemy (offset supheli: 0xCA0)
        int  skipDist = 0;      // maxDistance disi
        int  skipW2S = 0;       // WorldToScreen basarisiz / ekran disi
        bool hasLocalPos = false;
        uintptr_t pawn = 0;
        uintptr_t localRoot = 0;
        float localX = 0, localY = 0, localZ = 0;
    };

    extern Diag g_diag;

    void Render();
}

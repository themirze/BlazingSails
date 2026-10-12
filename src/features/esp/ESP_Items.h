#pragma once
#include <imgui.h>
#include <vector>
#include "sdk/Actors.h"
#include "sdk/UE4_Types.h"

namespace ESP {
    // ------------------------------------------------------------------
    //  Item ciziminin hangi asamada eledigi. Menu "Live" karti ve log
    //  bu sayaclari gosterir; boylece "item ESP cizmiyor" tahminle degil
    //  sayiyla cozulur.
    // ------------------------------------------------------------------
    struct ItemDiag {
        int listed = 0;      // GetItems() kac kayit dondu (sinif haritasi tuttu)
        int drawn = 0;       // ekrana gercekten cizilen
        int skipOff = 0;     // item turu menuden ACILMAMIS
        int skipDist = 0;    // maxDistance disi
        int skipW2S = 0;     // WorldToScreen basarisiz / ekran disi
    };
    extern ItemDiag g_itemDiag;

    void DrawItems(ImDrawList* dl, const std::vector<Actors::Item>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos);
}

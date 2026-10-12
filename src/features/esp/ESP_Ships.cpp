// ===========================================================================
//  GEMI (SHIP) ESP  --  sinif adi + mesafe isaretleri
//
//  NOT: Gemi actor'u yoksa (bu oyunda gemi sinifi farkli adlandirilmissa)
//  hicbir sey cizilmez. Gercek sinif adlari icin Misc sekmesindeki
//  "Dump world census" (F10) -> [census] log satirlarina bakin.
// ===========================================================================
#include "features/esp/ESP_Ships.h"
#include "sdk/UE4_W2S.h"
#include "render/Draw.h"
#include "config/Config.h"
#include <cstdio>

namespace ESP {

    void DrawShips(ImDrawList* dl, const std::vector<Actors::Ship>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos)
    {
        if (!dl || list.empty()) return;

        const ImU32 col = Draw::U32(cfg.esp.colShips);

        for (const auto& s : list) {
            float dist = 0.0f;
            if (hasLocalPos) {
                dist = (s.pos - localPos).Length() / 100.0f;
                if (cfg.esp.shipsDistance > 0.0f && dist > cfg.esp.shipsDistance)
                    continue;
            }

            // Gemi merkezi (yaklasik olarak su seviyesinin biraz ustu).
            FVector c = s.pos; c.Z += 200.0f;

            ImVec2 sp;
            if (!UE4::WorldToScreen(c, pov, sw, sh, sp)) continue;
            if (sp.x < 0.0f || sp.x > (float)sw || sp.y < 0.0f || sp.y > (float)sh) continue;

            // Isaret
            dl->AddRect({ sp.x - 7.0f, sp.y - 7.0f }, { sp.x + 7.0f, sp.y + 7.0f },
                col, 0.0f, 0, 1.5f);
            dl->AddLine({ sp.x, sp.y - 12.0f }, { sp.x, sp.y - 7.0f }, col, 1.5f);

            char buf[112];
            if (hasLocalPos)
                snprintf(buf, sizeof(buf), "%s  %.0fm", s.cls.c_str(), dist);
            else
                snprintf(buf, sizeof(buf), "%s", s.cls.c_str());

            const ImVec2 ts = ImGui::CalcTextSize(buf);
            Draw::TextShadow(dl, { sp.x - ts.x * 0.5f, sp.y - 30.0f }, col, buf);
        }
    }

} // namespace ESP

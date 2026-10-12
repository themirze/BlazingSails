#include "features/esp/ESP_Items.h"
#include "render/Draw.h"
#include "sdk/UE4_W2S.h"
#include "sdk/ItemClasses.h"
#include "config/Config.h"
#include <cstdio>

namespace ESP {
    ItemDiag g_itemDiag;

    void DrawItems(ImDrawList* dl, const std::vector<Actors::Item>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos)
    {
        for (const auto& it : list) {
            // ★ Tur bazli filtre: menude ACILMAMIS her tur atlanir.
            //   (Eski davranis kategori bayragina bakiyordu ve Cat_Other
            //    her zaman acikti -> tutarsizdi.)
            if (it.itemId < 0 || it.itemId >= ItemDB::kItemCount) { g_itemDiag.skipOff++; continue; }
            if (!cfg.esp.itemOn[it.itemId])                       { g_itemDiag.skipOff++; continue; }

            ImVec2 sp;
            if (!UE4::WorldToScreen(it.worldPos, pov, sw, sh, sp)) { g_itemDiag.skipW2S++; continue; }
            if (sp.x < 0 || sp.x > sw || sp.y < 0 || sp.y > sh)    { g_itemDiag.skipW2S++; continue; }

            float dist = 0.0f;
            if (hasLocalPos) {
                dist = (it.worldPos - localPos).Length() / 100.0f;
                if (dist > cfg.esp.itemDistance) { g_itemDiag.skipDist++; continue; }
            }

            g_itemDiag.drawn++;

            const char* label = ItemDB::kItems[it.itemId].label;
            ImU32 col = Draw::U32(cfg.esp.colItems);

            char buf[96];
            if (hasLocalPos && cfg.esp.itemShowDist)
                snprintf(buf, sizeof(buf), "%s [%.0fm]", label, dist);
            else
                snprintf(buf, sizeof(buf), "%s", label);

            ImVec2 sz = ImGui::CalcTextSize(buf);
            ImVec2 tp(sp.x - sz.x * 0.5f, sp.y - sz.y * 0.5f);

            dl->AddRectFilled(
                { tp.x - 4, tp.y - 2 },
                { tp.x + sz.x + 4, tp.y + sz.y + 2 },
                IM_COL32(0, 0, 0, 180), 3.0f);

            Draw::TextShadow(dl, tp, col, buf);
        }
    }
}

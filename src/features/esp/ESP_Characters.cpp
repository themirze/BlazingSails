#include "features/esp/ESP_Characters.h"
#include "features/esp/ESP.h"          // ★ teshis sayaclari (g_diag)
#include "render/Draw.h"
#include "sdk/UE4_W2S.h"
#include "config/Config.h"
#include <cstdio>

namespace ESP {
    void DrawCharacters(ImDrawList* dl, const std::vector<Actors::Character>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos)
    {
        for (const auto& ch : list) {
            if (!ch.rootComponent) { g_diag.skipRoot++; continue; }
            if (ch.isDead)         { g_diag.skipDead++; continue; }
            if (ch.isLocalPlayer)  { g_diag.skipLocal++; continue; }
            if (cfg.esp.ignoreTeam && !ch.isEnemy) { g_diag.skipEnemy++; continue; }

            float dist = 0.0f;
            if (hasLocalPos) {
                dist = (ch.worldPos - localPos).Length() / 100.0f;
                if (dist > cfg.esp.maxDistance) { g_diag.skipDist++; continue; }
            }

            FVector head = ch.worldPos; head.Z += 90.0f;
            FVector feet = ch.worldPos; feet.Z -= 90.0f;

            ImVec2 hs, fs;
            if (!UE4::WorldToScreen(head, pov, sw, sh, hs)) { g_diag.skipW2S++; continue; }
            if (!UE4::WorldToScreen(feet, pov, sw, sh, fs)) { g_diag.skipW2S++; continue; }
            if (hs.x < 0 || hs.x > sw || hs.y < 0 || hs.y > sh) { g_diag.skipW2S++; continue; }

            float h = fs.y - hs.y;
            if (h < 5.0f)    h = 5.0f;
            if (h > 1000.0f) h = 1000.0f;
            float w = h * 0.4f;

            ImVec2 tl(hs.x - w * 0.5f, hs.y);
            ImVec2 br(hs.x + w * 0.5f, fs.y);

            ImU32 col = Draw::U32(cfg.esp.colVisible);

            g_diag.drawn++;   // ★ sinirlari gecti -> gercekten ciziliyor

            if (cfg.esp.box)
                Draw::Box(dl, tl, br, col, cfg.esp.boxStyle, 1.5f);

            if (cfg.esp.name && ch.name[0]) {
                ImVec2 sz = ImGui::CalcTextSize(ch.name);
                ImVec2 np(tl.x + w * 0.5f - sz.x * 0.5f, tl.y - 16.0f);
                Draw::TextShadow(dl, np, col, ch.name);
            }

            if (cfg.esp.distance && hasLocalPos) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.0fm", dist);
                ImVec2 sz = ImGui::CalcTextSize(buf);
                ImVec2 dp(tl.x + w * 0.5f - sz.x * 0.5f, br.y + 2.0f);
                Draw::TextShadow(dl, dp, col, buf);
            }

            if (cfg.esp.health && ch.maxHealth > 0.0f) {
                float r = ch.health / ch.maxHealth;
                Draw::HealthBar(dl, { tl.x, br.y + 14.0f }, w, 3.0f, r);
            }

            if (cfg.esp.snaplines) {
                Draw::Snapline(dl, { sw * 0.5f, (float)sh }, { hs.x, br.y }, col);
            }
        }
    }
}

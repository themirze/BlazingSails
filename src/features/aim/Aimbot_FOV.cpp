// ============================================================================
//  AIMBOT  --  FOV CIRCLE (nisan alma dairesi)
//
//  NEDEN BU DOSYA BOSTU: motor gelistirme sirasinda Aimbot_FOV.cpp olusturuldu
//  ama icine HIC kod yazilmadi ve Aimbot::RenderFOV() hicbir yerden
//  CAGRILMIYORDU. Bu yuzden menude "Draw FOV circle" acik olsa da ekranda
//  daire gorunmuyordu. (Aimbot.cpp icinde de bos bir stub vardi.)
//
//  DUZELTME:
//    1) Gercek cizim buraya yazildi (Aimbot.cpp'deki stub kaldirildi ->
//       aksi halde ayni fonksiyon iki kez tanimlanip LNK2005 verir).
//    2) Cagri Hook::RenderOverlay() icinden yapiliyor (render thread,
//       ImGui::NewFrame ile ImGui::Render arasinda).
//
//  GEOMETRI (magic bullet'a bagli):
//    cfg.aim.mbConeDeg = magic'in hedef secim konisi (YARI ACI, derece).
//    Daire, ekranin gercek dikey FOV'una gore olceklendirilir:
//        r = (sh/2) * tan(fov/2) / tan(gameFov/2)
//    Boylece daire, magic'in gercekten hedefledigi koniyi gosterir.
// ============================================================================

#include "features/aim/Aimbot.h"
#include "core/Game.h"
#include "config/Config.h"
#include "sdk/UE4_Types.h"
#include <cmath>
#include <imgui.h>

namespace Aimbot {

    void RenderFOV(ImDrawList* dl, int sw, int sh)
    {
        if (!dl)              return;
        if (!cfg.aim.magicBullet) return;   // magic kapali -> daire yok
        if (!cfg.aim.drawFov) return;       // menude kapali
        if (sw <= 0 || sh <= 0) return;

        // Oyunun gercek dikey FOV'u (pov.FOV). Okunamazsa 90 varsay.
        float gameFov = 90.0f;
        FMinimalViewInfo pov;
        if (Game::ReadCamera(pov) && pov.FOV > 1.0f && pov.FOV < 179.0f)
            gameFov = pov.FOV;

        const float D2R   = 3.14159265358979f / 180.0f;
        const float halfH = (float)sh * 0.5f;

        float aimFov = cfg.aim.mbConeDeg;
        if (!(aimFov > 0.01f)) aimFov = 0.01f;
        if (aimFov > 89.0f)    aimFov = 89.0f;

        const float aimRad  = aimFov  * D2R * 0.5f;
        const float gameRad = gameFov * D2R * 0.5f;

        const float tg = std::tan(gameRad);
        float r = halfH * (std::tan(aimRad) / (tg > 0.0001f ? tg : 0.0001f));

        // NaN / 0 / asiri buyuk koruma.
        if (!(r > 1.0f)) r = 1.0f;
        const float maxR = halfH * 1.50f;
        if (r > maxR)    r = maxR;

        const ImVec4 c(cfg.aim.colFov.r, cfg.aim.colFov.g,
                       cfg.aim.colFov.b, cfg.aim.colFov.a);
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(c);

        const ImVec2 ctr((float)sw * 0.5f, (float)sh * 0.5f);

        dl->AddCircle(ctr, r, col, 128, 1.5f);   // dis cember
        dl->AddCircleFilled(ctr, 1.5f, col);     // merkez noktasi
    }

} // namespace Aimbot

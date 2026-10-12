// ===========================================================================
//  AIMBOT  --  fare tabanli aimlock (offset-free)
//
//  NEDEN FARE (BELLEK YAZMA DEGIL):
//    PlayerController::ControlRotation'a yazmak sunucu-otoriter akista
//    etkisiz/riskli olabilir (ve target offset gerektirir). Fare hareketi
//    (SendInput) oyunun KENDI girdi yolunu kullanir: ek offset yok, crash
//    riski yok. ImGui menusu acikken kapatilir ki fare menusyle catismasin.
//
//  Akis:
//    Enable + tusa basili + menu kapali -> kameraya en uygun (FOV icindeki)
//    dusmani sec -> ekran konumuna dogru fareyi 'smooth' ile kaydir.
//
//  ★ RenderFOV() bu dosyada DEGIL (Aimbot_FOV.cpp) - aksi halde LNK2005.
// ===========================================================================
#include "features/aim/Aimbot.h"
#include "core/Game.h"
#include "sdk/Actors.h"
#include "sdk/UE4_W2S.h"
#include "config/Config.h"
#include "ui/Menu.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>

namespace {

    constexpr float kD2R = 3.1415926535897932f / 180.0f;

    FVector ForwardFromRot(const FRotator& r)
    {
        const float p = r.Pitch * kD2R;
        const float y = r.Yaw   * kD2R;
        return FVector(cosf(p) * cosf(y), cosf(p) * sinf(y), sinf(p));
    }

    void MoveMouse(int dx, int dy)
    {
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dx = dx;
        in.mi.dy = dy;
        in.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &in, sizeof(INPUT));
    }

} // namespace

namespace Aimbot {

    void Update()
    {
        if (!cfg.aim.enabled) return;
        if (Menu::open)       return;          // menu acikken fareyi kapma
        if (!Game::moduleBase) return;
        if (!(GetAsyncKeyState(cfg.aim.key) & 0x8000)) return;

        FMinimalViewInfo pov;
        if (!Game::ReadCamera(pov)) return;

        ImGuiIO& io = ImGui::GetIO();
        const int sw = (int)io.DisplaySize.x;
        const int sh = (int)io.DisplaySize.y;
        if (sw <= 0 || sh <= 0) return;

        const std::vector<Actors::Character> chars = Actors::GetCharacters();
        if (chars.empty()) return;

        const FVector fwd = ForwardFromRot(pov.Rotation);
        const float maxDistCm = cfg.aim.maxDistance * 100.0f;

        // Bone -> hedef Z offseti (cm). 0 Head, 1 Neck, 2 Chest, 3 Nearest.
        const float zi[4] = { 90.0f, 72.0f, 55.0f, 70.0f };
        const float zoff = zi[(cfg.aim.bone >= 0 && cfg.aim.bone < 4) ? cfg.aim.bone : 0];

        const Actors::Character* best = nullptr;
        float bestAngle = 1e9f, bestDist = 1e9f, bestHp = 1e9f;

        for (const auto& c : chars) {
            if (c.isLocalPlayer || c.isDead || !c.rootComponent) continue;
            if (cfg.aim.ignoreTeam && !c.isEnemy) continue;

            FVector t = c.worldPos; t.Z += zoff;
            FVector dir = t - pov.Location;
            const float d = dir.Length();
            if (d < 1.0f) continue;
            if (maxDistCm > 0.0f && d > maxDistCm) continue;

            const FVector nd = dir * (1.0f / d);
            float cosv = nd.Dot(fwd);
            if (cosv > 1.0f)  cosv = 1.0f;
            if (cosv < -1.0f) cosv = -1.0f;
            const float ang = acosf(cosv) / kD2R;
            if (ang > cfg.aim.fov) continue;   // FOV disi

            bool take = false;
            if (cfg.aim.priority == 1)      take = (d < bestDist);
            else if (cfg.aim.priority == 2) take = (!best || (c.maxHealth > 0.0f && c.health < bestHp));
            else                            take = (ang < bestAngle);

            if (take) {
                best = &c; bestAngle = ang; bestDist = d; bestHp = c.health;
            }
        }

        if (!best) return;

        FVector t = best->worldPos; t.Z += zoff;
        ImVec2 sp;
        if (!UE4::WorldToScreen(t, pov, sw, sh, sp)) return;

        float dx = sp.x - (float)sw * 0.5f;
        float dy = sp.y - (float)sh * 0.5f;

        // Smoothing: yuksek deger = daha yavas/dogal.
        const float smooth = (cfg.aim.smooth < 1.0f) ? 1.0f : cfg.aim.smooth;
        dx /= smooth;
        dy /= smooth;

        // Tek karede asiri sicrama yapma.
        if (dx >  60.0f) dx =  60.0f;
        if (dx < -60.0f) dx = -60.0f;
        if (dy >  60.0f) dy =  60.0f;
        if (dy < -60.0f) dy = -60.0f;

        if (fabsf(dx) < 0.5f && fabsf(dy) < 0.5f) return;   // yeterince yakin

        MoveMouse((int)lroundf(dx), (int)lroundf(dy));
    }

} // namespace Aimbot

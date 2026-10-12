// ===========================================================================
//  TRIGGERBOT  --  nisan dusmanin uzerindeyken ates et (offset-free)
//
//  Yontem: dusman karakterinin ekran kutusu (head..feet) hesaplanir; ekran
//  MERKEZI (crosshair) bu kutunun icindeyse "hedefte" sayilir. Tus basili +
//  reactionDelay doldu ise TEK tik gonderilir (hedef kaybolana kadar tekrar
//  ateslenmez -> silah dongusune birakilir).
//
//  Girdi: SendInput (oyunun kendi girdi yolu). Menu acikken kapali.
// ===========================================================================
#include "features/trigger/Triggerbot.h"
#include "core/Game.h"
#include "sdk/Actors.h"
#include "sdk/UE4_W2S.h"
#include "config/Config.h"
#include "ui/Menu.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>

namespace {

    void ClickOnce()
    {
        INPUT in[2]{};
        in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, in, sizeof(INPUT));
    }

} // namespace

namespace Triggerbot {

    void Update()
    {
        static bool      s_onTarget = false;
        static bool      s_fired    = false;
        static ULONGLONG s_armTick  = 0;

        if (!cfg.aim.trigger || Menu::open || !Game::moduleBase) {
            s_onTarget = false; s_fired = false;
            return;
        }
        if (!(GetAsyncKeyState(cfg.aim.triggerKey) & 0x8000)) {
            s_onTarget = false; s_fired = false;
            return;
        }

        FMinimalViewInfo pov;
        if (!Game::ReadCamera(pov)) return;

        ImGuiIO& io = ImGui::GetIO();
        const int sw = (int)io.DisplaySize.x;
        const int sh = (int)io.DisplaySize.y;
        if (sw <= 0 || sh <= 0) return;

        const float cx = (float)sw * 0.5f;
        const float cy = (float)sh * 0.5f;
        const float maxDistCm = cfg.aim.maxDistance * 100.0f;

        const std::vector<Actors::Character> chars = Actors::GetCharacters();

        bool onTarget = false;
        for (const auto& c : chars) {
            if (c.isLocalPlayer || c.isDead || !c.rootComponent) continue;
            if (cfg.aim.ignoreTeam && !c.isEnemy) continue;

            if (maxDistCm > 0.0f) {
                const FVector local = pov.Location;   // kameradan uzaklik (yaklasik)
                if ((c.worldPos - local).Length() > maxDistCm) continue;
            }

            FVector head = c.worldPos; head.Z += 90.0f;
            FVector feet = c.worldPos; feet.Z -= 90.0f;

            ImVec2 hs, fs;
            if (!UE4::WorldToScreen(head, pov, sw, sh, hs)) continue;
            if (!UE4::WorldToScreen(feet, pov, sw, sh, fs)) continue;

            float h = fs.y - hs.y;
            if (h < 4.0f) h = 4.0f;
            const float halfW = h * 0.22f;

            const float x0 = hs.x - halfW, x1 = hs.x + halfW;
            const float y0 = (hs.y < fs.y) ? hs.y : fs.y;
            const float y1 = (hs.y < fs.y) ? fs.y : hs.y;

            if (cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1) { onTarget = true; break; }
        }

        const ULONGLONG now = GetTickCount64();

        if (onTarget) {
            if (!s_onTarget) { s_onTarget = true; s_fired = false; s_armTick = now; }
            if (!s_fired && (now - s_armTick) >= (ULONGLONG)cfg.aim.triggerDelay) {
                ClickOnce();
                s_fired = true;
            }
        } else {
            s_onTarget = false;
            s_fired = false;
        }
    }

} // namespace Triggerbot

#include "features/esp/ESP.h"
#include "features/esp/ESP_Characters.h"
#include "features/esp/ESP_Items.h"
#include "features/esp/ESP_Ships.h"
#include "sdk/Actors_World.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "core/Diagnostics.h"
#include "sdk/Offsets.h"
#include "config/Config.h"
#include <Windows.h>
#include <cstring>
#include <imgui.h>

namespace ESP {
    Diag g_diag;

    // =====================================================================
    //  TESHIS: ESP zincirinin nerede koptugunu log'dan okunur hale getirir.
    //    [esp] DURDU: GWorld=0            -> globals/offset
    //    [esp] DURDU: ReadCamera=false    -> kamera offset zinciri
    //    [esp] ... actors=0 / SACMA       -> ULevel::Actors offset'i
    //    [esp] ... actors=350 eslenen=0   -> FName/sinif filtresi uyusmuyor
    //    [esp] ... eslenen=6 cizilen=0    -> olu/dusman/mesafe/w2s eliyor
    //  Saniyede en fazla bir satir (dosya I/O'su kareyi yavaslatmasin).
    // =====================================================================
    static ULONGLONG s_lastDiag = 0;
    static ULONGLONG s_lastGlobals = 0;
    static bool      s_samplesLogged = false;
    static bool      s_itemSamplesLogged = false;

    static void LogSamples()
    {
        if (s_samplesLogged || Actors::g_charInfo.sampleCount <= 0) return;
        s_samplesLogged = true;

        char line[512] = {};
        for (int i = 0; i < Actors::g_charInfo.sampleCount; ++i) {
            if (i) strcat_s(line, "  ");
            strcat_s(line, Actors::g_charInfo.samples[i]);
        }
        Diagnostics::Log("[esp] ornek sinif adlari (%d): %s",
            Actors::g_charInfo.sampleCount, line);
    }

    // ★ Item tarafinda en kritik teshis: item GIBI gorunen ama CLASS_MAP'te
    //   bulunmayan isimler. "Eslenen=0" durumunda sebep dogrudan buradan gorunur.
    static void LogItemSamples()
    {
        if (s_itemSamplesLogged || Actors::g_itemInfo.sampleCount <= 0) return;
        s_itemSamplesLogged = true;

        char line[512] = {};
        for (int i = 0; i < Actors::g_itemInfo.sampleCount; ++i) {
            if (i) strcat_s(line, "  ");
            strcat_s(line, Actors::g_itemInfo.samples[i]);
        }
        Diagnostics::Log("[esp/items] ornek sinif adlari (%d): %s",
            Actors::g_itemInfo.sampleCount, line);
    }

    // stop != nullptr ise ESP o asamada durdu demektir.
    static void DiagTick(const char* stop, const FMinimalViewInfo* pov,
        bool wantChars, bool wantItems)
    {
        const ULONGLONG now = GetTickCount64();
        if (now - s_lastDiag < 1000) return;
        s_lastDiag = now;

        if (stop) {
            Diagnostics::Log("[esp] DURDU: %s", stop);
            return;
        }

        Diagnostics::Log("[esp] cam ok  pov=(%.0f,%.0f,%.0f)  rot=(%.1f,%.1f,%.1f)  fov=%.1f  "
            "lvl=0x%llX data=0x%llX actors=%d%s",
            pov->Location.X, pov->Location.Y, pov->Location.Z,
            pov->Rotation.Pitch, pov->Rotation.Yaw, pov->Rotation.Roll, pov->FOV,
            (unsigned long long)Actors::g_charInfo.level,
            (unsigned long long)Actors::g_charInfo.data,
            Actors::g_charInfo.count,
            Actors::g_charInfo.countRejected ? "  <-- SAYI SACMA, tarama iptal (ULevel_Actors offseti yanlis)" : "");

        if (wantChars) {
            Diagnostics::Log("[esp] liste=%d cizilen=%d  | elenen: root=%d local=%d olu=%d dusman=%d mesafe=%d w2s=%d  "
                "| FName: ok=%d bozuk=%d  | localPawn=0x%llX locPos=(%.0f,%.0f,%.0f) ok=%d",
                g_diag.listed, g_diag.drawn,
                g_diag.skipRoot, g_diag.skipLocal, g_diag.skipDead, g_diag.skipEnemy,
                g_diag.skipDist, g_diag.skipW2S,
                Actors::g_charInfo.clsRead - Actors::g_charInfo.clsBad, Actors::g_charInfo.clsBad,
                (unsigned long long)g_diag.pawn,
                g_diag.localX, g_diag.localY, g_diag.localZ, g_diag.hasLocalPos ? 1 : 0);
            LogSamples();
        }

        if (wantItems) {
            Diagnostics::Log("[esp/items] actors=%d eslenen=%d  FName: ok=%d bozuk=%d  | liste=%d cizilen=%d  "
                "| elenen: tur(kapali)=%d mesafe=%d w2s=%d",
                Actors::g_itemInfo.count, Actors::g_itemInfo.matched,
                Actors::g_itemInfo.clsRead - Actors::g_itemInfo.clsBad, Actors::g_itemInfo.clsBad,
                g_itemDiag.listed, g_itemDiag.drawn,
                g_itemDiag.skipOff, g_itemDiag.skipDist, g_itemDiag.skipW2S);
            LogItemSamples();
        }
    }

    void Render() {
        g_diag = Diag{};             // sayaclari her karede sifirla
        g_itemDiag = ItemDiag{};

        if (!Game::IsAttached()) return;

        // ★ ONEMLI DUZELTME: eskiden item ESP, oyuncu ESP ana anahtarinin
        //   (cfg.esp.enabled) ARKASINDA kaliyordu; yani "Enable player ESP"
        //   kapali oldugunda items da hic cizilmiyordu. Artik ikisi bagimsiz.
        const bool wantChars = cfg.esp.enabled;
        const bool wantItems = cfg.esp.items;
        const bool wantShips = cfg.esp.ships;

        if (!wantChars && !wantItems && !wantShips) {
            DiagTick("ESP kapali (Players/Items/Ships anahtarlarinin hepsi kapali)", nullptr, false, false);
            return;
        }

        // ★ GWorld her harita/seviye gecisinde yeniden olusturulur; deger
        //   bayatlamasin diye saniyede bir tazeleniyor.
        const ULONGLONG now = GetTickCount64();
        if (now - s_lastGlobals > 1000) {
            s_lastGlobals = now;
            Game::ReadGlobals();
        }

        if (!Game::GWorld) {
            DiagTick("GWorld=0 (globals okunamadi / oyun henuz dunyada degil)", nullptr, wantChars, wantItems);
            return;
        }

        FMinimalViewInfo pov;
        if (!Game::ReadCamera(pov)) {
            DiagTick("ReadCamera=false (kamera offset zinciri)", nullptr, wantChars, wantItems);
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        int sw = (int)io.DisplaySize.x;
        int sh = (int)io.DisplaySize.y;
        if (sw <= 0 || sh <= 0) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        // Local player pozisyonu (mesafe hesabi icin)
        FVector localPos;
        bool hasLocalPos = false;
        g_diag.pawn = Game::localPawn;
        if (Game::localPawn) {
            uintptr_t root = Memory::Read<uintptr_t>(
                Game::localPawn + Offsets::Actor_RootComponent);
            if (root) {
                localPos = Memory::Read<FVector>(
                    root + Offsets::Scene_RelativeLocation);
                hasLocalPos = true;
                g_diag.localRoot = root;
                g_diag.localX = localPos.X;
                g_diag.localY = localPos.Y;
                g_diag.localZ = localPos.Z;
            }
        }
        g_diag.hasLocalPos = hasLocalPos;

        if (wantItems) {
            auto items = Actors::GetItems();
            g_itemDiag.listed = (int)items.size();
            DrawItems(dl, items, pov, sw, sh, localPos, hasLocalPos);
        }

        if (wantChars) {
            auto chars = Actors::GetCharacters();
            g_diag.listed = (int)chars.size();
            DrawCharacters(dl, chars, pov, sw, sh, localPos, hasLocalPos);
        }

        if (wantShips) {
            auto ships = Actors::GetShips();
            DrawShips(dl, ships, pov, sw, sh, localPos, hasLocalPos);
        }

        DiagTick(nullptr, &pov, wantChars, wantItems);
    }
}

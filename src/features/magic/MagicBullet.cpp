// ============================================================================
//  MAGIC BULLET  (v24 -- Direction tabanli, DOGRU offset)
//
//  ★ KOK DUZELTME: mermi (ABP_ProjectileBase_C) UProjectileMovementComponent
//    KULLANMIYOR ve "Velocity" adli bir FVector alani YOK. Hareket her tick
//    Blueprint'te ELLE yapilir:
//
//         pos += Direction * TravelspeedXY * dt           (yatay)
//         pos.Z += TravelspeedZ * dt - Droprate*dt        (yay/yercekimi)
//         pos += AddedVelocity
//
//    GERCEK SURUCU alan = Direction (birim yon vektoru) @ actor+0x280.
//    Eskiden 0x260'a FVector yaziyorduk; orasi 4 baytlik SKALER
//    (TravelspeedXY) oldugu icin mermi donmuyor, komsu alanlar bozuluyordu.
//
//  SIMDI: her mermi icin  Direction = normalize(hedef - mermi)  yazilir.
//  Boylece mermi dogrudan hedefe yonelir (yatay yon kesin; dikey yay
//  Droprate ile dogal olarak korunur).
// ============================================================================

#include "features/magic/MagicBullet.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "core/Diagnostics.h"
#include "sdk/Actors.h"
#include "sdk/Offsets.h"
#include "config/Config.h"
#include <cmath>
#include <string>

namespace MagicBullet {

    static bool      s_firstRun = true;
    static bool      s_firstWriteLogged = false;
    static ULONGLONG s_lastLog = 0;
    static int       s_writtenLast = 0;

    static uintptr_t   s_cachedWorld = 0;
    static ULONGLONG   s_worldChangeTime = 0;

    // ------------------------------------------------------------------
    //  SEH korumali yazma
    // ------------------------------------------------------------------
    template <class T>
    static bool WriteSafe(uintptr_t addr, const T& v)
    {
        if (!Memory::IsSaneRange(addr)) return false;
        __try {
            *reinterpret_cast<T*>(addr) = v;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool VecBad(const FVector& v)
    {
        return !(v.X == v.X) || !(v.Y == v.Y) || !(v.Z == v.Z) ||
               std::fabs(v.X) > 2000000.0f || std::fabs(v.Y) > 2000000.0f ||
               std::fabs(v.Z) > 2000000.0f;
    }

    bool Update()
    {
        if (!cfg.aim.magicBullet) { s_firstRun = true; return false; }

        // tus atama: 0 = hep acik; aksi halde tus basili olmali
        if (cfg.aim.mbKey != 0 && !(GetAsyncKeyState(cfg.aim.mbKey) & 0x8000))
            return false;

        if (s_firstRun) {
            s_firstRun = false;
            s_firstWriteLogged = false;
            Diagnostics::Log(
                "[mb] magic bullet v24 (Direction tabanli): Direction=actor+0x280 "
                "yazilir. Mermi UProjectileMovementComponent KULLANMIYOR; hiz "
                "0x260=TravelspeedXY (skaler). Dogru surucu = Direction (birim vektor).");
        }

        if (!Game::IsAttached() || !Game::localPawn) return false;

        // harita gecisi korumasi
        {
            const uintptr_t world = Game::GWorld;
            if (world != s_cachedWorld) {
                s_cachedWorld = world;
                s_worldChangeTime = GetTickCount64();
                s_writtenLast = 0;
                return false;
            }
            if (GetTickCount64() - s_worldChangeTime < 3000) return false;
        }

        // ---- yerel oyuncu konumu ----
        FVector localPos; bool hasLocal = false;
        {
            const uintptr_t root = Memory::Read<uintptr_t>(
                Game::localPawn + Offsets::Actor_RootComponent);
            if (root) {
                localPos = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);
                hasLocal = true;
            }
        }

        // ---- HEDEF SECIMI (crosshair) ----
        const std::vector<Actors::Character> chars = Actors::GetCharacters();

        FMinimalViewInfo pov;
        const bool hasCam = Game::ReadCamera(pov);
        const FVector camLoc = pov.Location;
        FVector camFwd{ 0.0f, 0.0f, 0.0f };
        if (hasCam) {
            const float R2D = 3.14159265358979f / 180.0f;
            const float cp  = std::cos(pov.Rotation.Pitch * R2D);
            camFwd.X = cp * std::cos(pov.Rotation.Yaw * R2D);
            camFwd.Y = cp * std::sin(pov.Rotation.Yaw * R2D);
            camFwd.Z = std::sin(pov.Rotation.Pitch * R2D);
        }
        const bool  useCross = cfg.aim.mbUseCrosshair && hasCam;
        const float coneDeg  = cfg.aim.mbConeDeg > 1.0f ? cfg.aim.mbConeDeg : 1.0f;
        const float cosCone  = std::cos(coneDeg * 3.14159265358979f / 180.0f);

        FVector   target; bool hasTarget = false;
        float     bestSq = 0.0f, bestCos = -2.0f;
        FVector   nearestTarget; bool hasNearest = false;
        const float maxDistCm =
            (cfg.aim.mbMaxTargetDist > 1.0f ? cfg.aim.mbMaxTargetDist : 1.0f) * 100.0f;

        if (hasLocal) {
            for (const auto& c : chars) {
                if (!c.actor || c.isDead) continue;
                if (c.actor == Game::localPawn || c.isLocalPlayer) continue;
                if (cfg.aim.mbEnemiesOnly && !c.isEnemy) continue;

                const float dx = c.worldPos.X - localPos.X;
                const float dy = c.worldPos.Y - localPos.Y;
                const float dz = c.worldPos.Z - localPos.Z;
                const float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 < 100.0f) continue;
                if (d2 > maxDistCm * maxDistCm) continue;

                // Her zaman en yakini takip et (fallback icin)
                if (!hasNearest || d2 < bestSq) {
                    bestSq = d2; nearestTarget = c.worldPos; hasNearest = true;
                }

                if (useCross) {
                    const FVector to{ c.worldPos.X - camLoc.X,
                                      c.worldPos.Y - camLoc.Y,
                                      c.worldPos.Z - camLoc.Z };
                    const float len = to.Length();
                    if (len < 1.0f) continue;
                    const float cosA = to.Dot(camFwd) / len;
                    if (cosA < cosCone) continue;
                    if (!hasTarget || cosA > bestCos) {
                        bestCos = cosA; target = c.worldPos; hasTarget = true;
                    }
                }
            }
        }

        // Koni (crosshair) hicbir sey bulamadiysa -> en yakin dusmana dus
        if (!hasTarget && hasNearest) {
            target = nearestTarget; hasTarget = true;
        }

        const std::vector<Actors::Projectile> projs = Actors::GetProjectiles();

        if (cfg.aim.mbLog) {
            const ULONGLONG now = GetTickCount64();
            if (now - s_lastLog >= 1000) {
                s_lastLog = now;
                Diagnostics::Log(
                    "[mb] karakter=%d mermi=%d yazilan=%d hedef=%d cross=%d",
                    (int)chars.size(), (int)projs.size(), s_writtenLast,
                    hasTarget ? 1 : 0, useCross ? 1 : 0);
            }
        }

        if (!hasTarget) return false;

        // aim Z offset (gogus hizasi) -- sinirli
        {
            float zoff = cfg.aim.mbAimZOffset;
            if (!(zoff == zoff)) zoff = 0.0f;
            if (zoff >  300.0f) zoff =  300.0f;
            if (zoff < -300.0f) zoff = -300.0f;
            target.Z += zoff;
        }

        // ---- mermiler: Direction'i hedefe cevir ----
        int written = 0;

        for (const auto& p : projs) {
            if (!p.actor) continue;
            if (p.className.find("Projectile") == std::string::npos) continue;

            const uintptr_t root = Memory::Read<uintptr_t>(
                p.actor + Offsets::Actor_RootComponent);

            FVector pos = p.pos;
            if (root)
                pos = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);
            if (VecBad(pos)) continue;

            FVector dir = target - pos;
            const float d = dir.Length();
            if (d < 1.0f || d > 500000.0f) continue;
            dir = dir * (1.0f / d);   // normalize

            if (WriteSafe<FVector>(p.actor + Offsets::Proj_Direction, dir))
                ++written;
        }

        s_writtenLast = written;

        if (cfg.aim.mbLog && written == 0 && !projs.empty()) {
            static ULONGLONG s_lastNoWrite = 0;
            const ULONGLONG nw = GetTickCount64();
            if (nw - s_lastNoWrite >= 1000) {
                s_lastNoWrite = nw;
                Diagnostics::Log("[mb] yazma yok (mermi=%d) — mermi Direction'i "
                    "zaten hedefe mi bakiyor, yoksa sinif 'Projectile' degil mi?",
                    (int)projs.size());
            }
        }

        if (written > 0 && !s_firstWriteLogged) {
            s_firstWriteLogged = true;
            Diagnostics::Log("[mb] ilk yazma OK: mermi=%d yazilan=%d hedef=(%.0f,%.0f,%.0f)",
                (int)projs.size(), written, target.X, target.Y, target.Z);
        }

        return written > 0;
    }

} // namespace MagicBullet

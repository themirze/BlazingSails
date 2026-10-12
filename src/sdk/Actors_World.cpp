// ===========================================================================
//  DUNYA (WORLD) MODULU  --  census + gemi listesi
//  Ayrinti ve gerekce icin bkz. Actors_World.h
// ===========================================================================
#include "sdk/Actors_World.h"
#include "core/Diagnostics.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/Offsets.h"
#include "sdk/UE4_FName.h"
#include <Windows.h>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Actors {

    namespace {

        bool ContainsAny(const std::string& n, const char* const* toks, int cnt)
        {
            for (int i = 0; i < cnt; ++i)
                if (n.find(toks[i]) != std::string::npos) return true;
            return false;
        }

        // Gemi sinifi belirtecleri. Bu oyunda GERCEK adlar census log'unda
        // gorunecek; gerekirse bu liste genisletilir.
        const char* const kShipToks[] = {
            "Ship", "Boat", "Vessel", "Galleon", "Sloop",
            "Schooner", "Frigate", "Brigantine", "Raft", "Hull"
        };
        const int kShipTokCount = (int)(sizeof(kShipToks) / sizeof(kShipToks[0]));

        // Silah sinifi belirtecleri (census'ta gruplanir).
        const char* const kWeaponToks[] = {
            "Weapon", "Musket", "Blunderbuss", "Bow", "Crossbow",
            "Cutlass", "Sword", "Cannon", "Harpoon", "Grenade",
            "Pistol", "Rifle", "Ammo"
        };
        const int kWeaponTokCount = (int)(sizeof(kWeaponToks) / sizeof(kWeaponToks[0]));

        // Anchor (demir) ile ilgili siniflar (ileride menu aksiyonu icin).
        const char* const kAnchorToks[] = { "Anchor" };
        const int kAnchorTokCount = (int)(sizeof(kAnchorToks) / sizeof(kAnchorToks[0]));

        // ---- bayrak + isim onbellegi ------------------------------------
        volatile LONG s_censusRequested = 0;

        std::unordered_map<int32_t, std::string> s_names;

        const std::string& ClassName(int32_t idx)
        {
            if (s_names.size() > 20000) s_names.clear();
            auto it = s_names.find(idx);
            if (it == s_names.end())
                it = s_names.emplace(idx, UE4::ReadFName(Game::GNames, idx)).first;
            return it->second;
        }

        // Ortak: seviye aktor dizisini dondur (data/count gecerli mi?).
        bool ReadActorArray(uintptr_t& outData, int32_t& outCount)
        {
            outData = 0; outCount = 0;
            if (!Game::GWorld) return false;

            const uintptr_t level = Memory::Read<uintptr_t>(
                Game::GWorld + Offsets::UWorld_PersistentLevel);
            if (!level) return false;

            const uintptr_t data = Memory::Read<uintptr_t>(level + Offsets::ULevel_Actors);
            const int32_t   count = Memory::Read<int32_t>(level + Offsets::ULevel_Actors + 8);
            if (!data || count <= 0 || count > 16384) return false;

            outData = data; outCount = count;
            return true;
        }

    } // namespace

    bool IsShipClass(const std::string& n)
    {
        if (n.empty() || n.size() > 64) return false;
        return ContainsAny(n, kShipToks, kShipTokCount);
    }

    void RequestWorldCensus()     { InterlockedExchange(&s_censusRequested, 1); }
    bool IsWorldCensusPending()   { return s_censusRequested != 0; }

    // =======================================================================
    //  CENSUS  --  tum actor sinif adlarini histogram + kategori olarak dok.
    //
    //  NOT: Dis SEH yok. Tum okumalar Memory::Read ile zaten SEH-korumali.
    //  (C++ nesneleri olan fonksiyonda __try C2712 verir.)
    // =======================================================================
    void ProcessWorldCensus()
    {
        if (InterlockedCompareExchange(&s_censusRequested, 0, 1) != 1) return;

        uintptr_t data = 0; int32_t count = 0;
        if (!ReadActorArray(data, count)) {
            Diagnostics::Log("[census] actor dizisi okunamadi (GWorld/level/offset)");
            return;
        }

        Diagnostics::Log("=== DUNYA SAYIMI (census) ===  actor=%d", count);

        std::unordered_map<std::string, int> hist;
        std::vector<std::pair<std::string, FVector>> ships;
        std::unordered_map<std::string, int> weapons;
        std::unordered_map<std::string, int> anchors;
        int unnamed = 0;

        for (int i = 0; i < count; ++i) {
            const uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::IsSanePtr(a)) continue;

            const uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);   // ClassPrivate
            if (!Memory::IsSanePtr(cp)) continue;

            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);     // UObject::Name
            if (idx <= 0 || idx > 10000000) continue;

            const std::string n = ClassName(idx);
            if (n.empty() || n == "?") { ++unnamed; continue; }

            ++hist[n];

            if (IsShipClass(n)) {
                const uintptr_t root = Memory::Read<uintptr_t>(a + Offsets::Actor_RootComponent);
                FVector pos;
                if (root) pos = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);
                ships.emplace_back(n, pos);
            }
            if (ContainsAny(n, kWeaponToks, kWeaponTokCount)) ++weapons[n];
            if (ContainsAny(n, kAnchorToks, kAnchorTokCount)) ++anchors[n];
        }

        // ---- histogram (en cok bulunan 40 sinif) ------------------------
        std::vector<std::pair<std::string, int>> hv(hist.begin(), hist.end());
        std::sort(hv.begin(), hv.end(),
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return a.second > b.second;
            });
        Diagnostics::Log("[census] --- TUM sinif adlari (adet) ---");
        int k = 0;
        for (const auto& kv : hv) {
            Diagnostics::Log("[census]   %-52s : %d", kv.first.c_str(), kv.second);
            if (++k >= 60) { Diagnostics::Log("[census]   ... (%d farkli sinif toplam)", (int)hv.size()); break; }
        }

        // ---- gemiler ----------------------------------------------------
        Diagnostics::Log("[census] GEMI adaylari = %d", (int)ships.size());
        int m = 0;
        for (const auto& s : ships) {
            Diagnostics::Log("[census]   GEMI %-44s pos=(%.0f, %.0f, %.0f)",
                s.first.c_str(), s.second.X, s.second.Y, s.second.Z);
            if (++m >= 24) { Diagnostics::Log("[census]   ... (%d gemi toplam)", (int)ships.size()); break; }
        }

        // ---- silahlar ---------------------------------------------------
        std::vector<std::pair<std::string, int>> wv(weapons.begin(), weapons.end());
        std::sort(wv.begin(), wv.end(),
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return a.second > b.second;
            });
        Diagnostics::Log("[census] SILAH/ilisikli siniflar = %d", (int)wv.size());
        m = 0;
        for (const auto& kv : wv) {
            Diagnostics::Log("[census]   SILAH %-44s : %d", kv.first.c_str(), kv.second);
            if (++m >= 30) break;
        }

        // ---- anchor -----------------------------------------------------
        if (!anchors.empty()) {
            Diagnostics::Log("[census] ANCHOR/ilisikli siniflar = %d", (int)anchors.size());
            m = 0;
            for (const auto& kv : anchors) {
                Diagnostics::Log("[census]   ANCHOR %-44s : %d", kv.first.c_str(), kv.second);
                if (++m >= 15) break;
            }
        } else {
            Diagnostics::Log("[census] ANCHOR/ilisikli sinif BULUNAMADI (isim farkli olabilir)");
        }

        if (unnamed) Diagnostics::Log("[census] cozulemeyen sinif adi = %d", unnamed);
        Diagnostics::Log("=== DUNYA SAYIMI BITTI ===");
    }

    // =======================================================================
    //  GEMI LISTESI  (ESP icin, ~500 ms onbellek)
    // =======================================================================
    std::vector<Ship> GetShips()
    {
        static ULONGLONG s_next = 0;
        static std::vector<Ship> s_cache;

        const ULONGLONG now = GetTickCount64();
        if (now < s_next) return s_cache;
        s_next = now + 500;

        s_cache.clear();

        uintptr_t data = 0; int32_t count = 0;
        if (!ReadActorArray(data, count)) return s_cache;

        for (int i = 0; i < count; ++i) {
            const uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::IsSanePtr(a)) continue;

            const uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);
            if (!Memory::IsSanePtr(cp)) continue;

            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);
            if (idx <= 0 || idx > 10000000) continue;

            const std::string n = ClassName(idx);
            if (!IsShipClass(n)) continue;

            const uintptr_t root = Memory::Read<uintptr_t>(a + Offsets::Actor_RootComponent);
            if (!root) continue;

            Ship s;
            s.actor = a;
            s.cls   = n;
            s.pos   = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);
            s_cache.push_back(std::move(s));

            if (s_cache.size() >= 256) break;   // guvenlik siniri
        }
        return s_cache;
    }

} // namespace Actors

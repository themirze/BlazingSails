#include "sdk/Actors.h"
#include "sdk/ItemClasses.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/UE4_FName.h"
#include "sdk/Offsets.h"
#include <Windows.h>
#include <cstring>
#include <unordered_map>

namespace Actors {
    ScanInfo g_itemInfo;

    static std::vector<uintptr_t> s_cache;
    static ULONGLONG s_nextScan = 0;   // sonraki tarama zamani (backoff icin)
    static std::unordered_map<int32_t, int> s_idCache;   // FName idx -> ItemDB id (-1 = item degil)

    static void NoteSample(const std::string& n)
    {
        if (n.empty() || n == "?") return;
        if (g_itemInfo.sampleCount >= 8) return;
        for (int i = 0; i < g_itemInfo.sampleCount; ++i)
            if (n == g_itemInfo.samples[i]) return;

        strncpy_s(g_itemInfo.samples[g_itemInfo.sampleCount], 48, n.c_str(), _TRUNCATE);
        g_itemInfo.sampleCount++;
    }

    // ★ TESHIS: item gibi gorunen isimleri ornekle. Bunlar CLASS_MAP'te
    //   yoksa (ornegin "_C" eki yok ya da isim degismis) log'dan dogrudan
    //   gorulur ve harita ona gore duzeltilir.
    static bool LooksLikeItem(const std::string& n)
    {
        return n.find("Pickup") != std::string::npos
            || n.find("Loot")   != std::string::npos
            || n.find("Chest")  != std::string::npos
            || n.find("Crate")  != std::string::npos
            || n.find("Barrel") != std::string::npos
            || n.find("Cannon") != std::string::npos;
    }

    // FName indeksi -> ItemDB id. Sonuc onbellege alinir (indeks basina BIR kez
    // cozulur), boylece her taramada strcmp dongusu tekrarlanmaz.
    static int ItemIdForIndex(int32_t idx)
    {
        if (s_idCache.size() > 16384) s_idCache.clear();

        auto it = s_idCache.find(idx);
        if (it != s_idCache.end()) return it->second;

        const std::string n = UE4::ReadFName(Game::GNames, idx);
        g_itemInfo.clsRead++;
        if (n.empty() || n == "?") g_itemInfo.clsBad++;
        else if (LooksLikeItem(n)) NoteSample(n);

        const int id = ItemDB::FindByClass(n.c_str());
        s_idCache.emplace(idx, id);
        return id;
    }

    static void Scan()
    {
        s_cache.clear();
        g_itemInfo = ScanInfo{};

        uintptr_t level = Memory::Read<uintptr_t>(
            Game::GWorld + Offsets::UWorld_PersistentLevel);
        g_itemInfo.level = level;
        if (!level) return;

        uintptr_t data = Memory::Read<uintptr_t>(level + Offsets::ULevel_Actors);
        int32_t   count = Memory::Read<int32_t>(level + Offsets::ULevel_Actors + 8);
        g_itemInfo.data = data;
        g_itemInfo.count = count;
        // Ust sinir: offset'ler uyusmadiginda 'count' cop olur ve on binlerce
        // SEH korumali okuma render thread'ini kilitler.
        if (!data || count <= 0 || count > 16384) { g_itemInfo.countRejected = true; return; }

        for (int i = 0; i < count; i++) {
            uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::Valid(a)) continue;

            uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);   // UObject::ClassPrivate
            if (!cp) continue;

            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);   // UObject::NamePrivate
            if (ItemIdForIndex(idx) < 0) continue;                  // CLASS_MAP'te yok

            if (s_cache.size() >= 2048) break;   // guvenlik siniri
            s_cache.push_back(a);
        }

        g_itemInfo.matched = (int)s_cache.size();
    }

    std::vector<Item> GetItems()
    {
        std::vector<Item> out;
        if (!Game::GWorld) return out;

        // Karakter taramasindaki ile ayni mantik: bos sonucta backoff.
        const ULONGLONG now = GetTickCount64();
        if (now >= s_nextScan) {
            Scan();
            s_nextScan = now + (s_cache.empty() ? 2000 : 500);
        }

        out.reserve(s_cache.size());
        for (uintptr_t a : s_cache) {
            uintptr_t root = Memory::Read<uintptr_t>(a + Offsets::Actor_RootComponent);
            if (!root) continue;

            FVector pos = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);
            if (pos.X == 0.0f && pos.Y == 0.0f) continue;

            uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);
            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);
            const int id = ItemIdForIndex(idx);
            if (id < 0) continue;

            Item it;
            it.actor     = a;
            it.worldPos  = pos;
            it.itemId    = id;
            it.className = ItemDB::kItems[id].label;
            out.push_back(it);
        }
        return out;
    }
}

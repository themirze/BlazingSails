#include "sdk/Actors.h"
#include "core/Diagnostics.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/UE4_FName.h"
#include "sdk/Offsets.h"
#include <Windows.h>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace Actors {
    ScanInfo g_charInfo;

    static std::vector<uintptr_t> s_cache;
    static ULONGLONG s_nextScan = 0;   // ★ sonraki tarama zamani (backoff icin)
    static std::unordered_map<int32_t, std::string> s_names;

    // ★ TESHIS: FName cozumlemesi gercekten calisiyor mu? Ilk farkli
    //   sinif adlarini sakla; ESP log'u bunlari yazdirir. "?" doluyorsa
    //   GNames/FNamePool yerlesimi yanlis demektir.
    static void NoteSample(const std::string& n)
    {
        if (n.empty() || n == "?") return;
        if (g_charInfo.sampleCount >= 8) return;
        for (int i = 0; i < g_charInfo.sampleCount; ++i)
            if (n == g_charInfo.samples[i]) return;   // ayni ismi tekrar yazma

        strncpy_s(g_charInfo.samples[g_charInfo.sampleCount], 48, n.c_str(), _TRUNCATE);
        g_charInfo.sampleCount++;
    }

    static const std::string& ClassName(int32_t idx) {
        // ★ Sinirsiz buyumeyi engelle (garbage idx'lerle bellek sismesin)
        if (s_names.size() > 8192) s_names.clear();
        auto it = s_names.find(idx);
        if (it == s_names.end()) {
            std::string n = UE4::ReadFName(Game::GNames, idx);
            g_charInfo.clsRead++;
            if (n.empty() || n == "?") g_charInfo.clsBad++;
            else                       NoteSample(n);
            it = s_names.emplace(idx, std::move(n)).first;
        }
        return it->second;
    }

    static bool IsCharClass(const std::string& n) {
        if (n.empty() || n.length() > 64)              return false;
        if (n.find("BP_") != 0)                        return false;
        if (n.find("Character") == std::string::npos)  return false;
        if (n.find("Ref") != std::string::npos) return false;
        if (n.find("Movement") != std::string::npos) return false;
        if (n.find("Controller") != std::string::npos) return false;
        if (n.find("Camera") != std::string::npos) return false;
        return true;
    }

    static void Scan() {
        s_cache.clear();
        g_charInfo = ScanInfo{};

        uintptr_t level = Memory::Read<uintptr_t>(
            Game::GWorld + Offsets::UWorld_PersistentLevel);
        g_charInfo.level = level;
        if (!level) return;

        uintptr_t data = Memory::Read<uintptr_t>(level + Offsets::ULevel_Actors);
        int32_t   count = Memory::Read<int32_t>(level + Offsets::ULevel_Actors + 8);
        g_charInfo.data = data;
        g_charInfo.count = count;
        // ★ Ust sinir: offset'ler uyusmadiginda 'count' cop olur ve on binlerce
        //   SEH korumali okuma render thread'ini kilitler.
        if (!data || count <= 0 || count > 16384) { g_charInfo.countRejected = true; return; }

        for (int i = 0; i < count; i++) {
            uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::Valid(a)) continue;

            uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);
            if (!cp) continue;

            int32_t idx = Memory::Read<int32_t>(cp + 0x18);
            if (!IsCharClass(ClassName(idx))) continue;

            if (s_cache.size() >= 512) break;   // ★ guvenlik siniri
            s_cache.push_back(a);
        }

        g_charInfo.matched = (int)s_cache.size();
    }

    std::vector<Character> GetCharacters() {
        std::vector<Character> out;
        if (!Game::GWorld) return out;

        // ★ ESKI HATA: `now - s_lastScan > 200 || s_cache.empty()` kosulu
        //   yuzunden tarama hic sonuc vermedigi surece HER KAREDE tekrar
        //   calisiyordu (16k'ya kadar SEH okumasi -> stall + istisna patlamasi).
        //   Cozum: sonraki tarama zamanini her halukarda ilerlet; bos sonucta
        //   daha uzun bekle.
        const ULONGLONG now = GetTickCount64();
        if (now >= s_nextScan) {
            Scan();
            s_nextScan = now + (s_cache.empty() ? 2000 : 200);
        }

        for (uintptr_t a : s_cache) {
            Character c;
            c.actor = a;
            c.rootComponent = Memory::Read<uintptr_t>(a + Offsets::Actor_RootComponent);
            if (!c.rootComponent) continue;

            c.worldPos = Memory::Read<FVector>(c.rootComponent + Offsets::Scene_RelativeLocation);
            c.mesh = Memory::Read<uintptr_t>(a + Offsets::Char_Mesh);
            c.health = (float)Memory::Read<int32_t>(a + Offsets::Char_Health);
            c.maxHealth = (float)Memory::Read<int32_t>(a + Offsets::Char_MaxHealth);
            c.isDead = Memory::Read<bool>(a + Offsets::Char_IsDead);
            c.isEnemy = Memory::Read<bool>(a + Offsets::Char_IsEnemy);
            c.isLocalPlayer = Game::IsLocalPlayer(a);

            std::string n = UE4::ReadFString(a + Offsets::Char_UserName);
            if (n.empty() || n.length() > 32) n = "Unknown";
            strncpy_s(c.name, n.c_str(), sizeof(c.name) - 1);

            out.push_back(c);
        }
        return out;
    }
}

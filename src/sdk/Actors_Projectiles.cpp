// ===========================================================================
//  MERMI (PROJECTILE) MODULU  --  YOL HARITASI ADIM 1 + 2
//
//  ADIM 1 : IsProjectileClass()  -> mermi sinifi filtresi
//  ADIM 2 : PMC offset'i + PMC icindeki Velocity (FVector) offset'ini OLCEREK bul
//
//  -------------------------------------------------------------------------
//  REVIZYON GECMISI (log kanitlariyla)
//  -------------------------------------------------------------------------
//  [R1] DAR FILTRE: "ABP_/UBP_Projectile" yanlisti; UE "A"/"U" onekini FName
//       havuzuna yazmaz. Gercek bicim "BP_Projectile*_C"  ->  DUZELTILDI.
//
//  [R2] ISTISNA PATLAMASI (~1200/sn): actor+0x100..0x2000 arasi her 8 bayt
//       Memory::Read() ile okunuyordu -> her okuma bir SEH istisnasi.
//       Cozum: Memory::IsSaneRange on-kontrolu + PageReadable(VirtualQuery)
//       onbellegi + SnapshotActor (sayfa sayfa TEK SEH ile kopya).
//       LOG KANITI (cozuldu): F9 penceresinde 707 -> 1263 (87 sn) = 6.4/sn.
//
//  [R3] F9 SIFIRLAMA: her F9 s_prev'i temizliyordu -> hicbir aktor ikinci
//       ornegi alamayinca "measured" hesaplanamaz, PMC taramasina HIC
//       girilmez. Artik izleme aktifken F9 SIFIRLAMAZ, UZATIR.
//       LOG KANITI: "izlenen: BP_ProjectileCrossbowArrow_C hiz=14854 cm/s"
//
//  [R4] PMC SINIF ADI ESLESMIYOR -- bu revizyonun konusu.
//       LOG KANITI: "PMC bulunamadi (taranan aday=109/385)". 4 KB tarandi,
//       yuzlerce gecerli isaretci bulundu, ama HICBIRININ sinif adi
//       "ProjectileMovement"/"MovementComponent" icermedi. Yani isim tabanli
//       arama bu oyunda calismiyor (bilesen farkli adlandirilmis, ya da
//       isaretci 4 KB disinda).
//
//       COZUM: isimden BAGIMSIZ vektor arayisi. "measured" (konumdan olculen
//       hiz) ve "yon" elimizde oldugu icin velocity'yi dogrudan eslestiririz:
//           |(x,y,z)|  ~ measured      (0.70 .. 1.30)
//           cos( (x,y,z), Δpos ) >= 0.75
//       Ayni offset'te birden fazla mermide tutan aday = gercek alan.
//
//       UC ASAMALI ARAMA:
//         A) actor'un KENDISI      (0x20..0x2000) -> ReplicatedMovement.LinearVelocity vb.
//         B) RootComponent         (0x20..0x0300) -> USceneComponent::ComponentVelocity
//         C) her UObject adayinin  (0x20..0x0300) -> PMC icindeki Velocity
//       Ayrica C asamasi her aday nesnenin SINIF ADINI histograma yazar ->
//       PMC gercekte nasil adlandirilmis, log'da goruruz.
//
//  YONTEM NOTU: velocity KONUM DEGISIMINDEN dogrulanir; boylece "3 ardisik
//  float" gibi zayif bir kalip yerine FIZIKSEL tutarlilik aranir.
//
//  KULLANIM: oyunda CANLIYKEN silahi al -> ATES ET -> F9 -> 20 sn bekle.
//            (F9'u tekrar tekrar basma; izleme zaten uzatilir.)
// ===========================================================================

#include "sdk/Actors.h"
#include "core/Diagnostics.h"
#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/Offsets.h"
#include "sdk/UE4_FName.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Actors {

    // =======================================================================
    //  ADIM 1: DAR filtre (MagicBullet bunu kullanacak).
    //  ★ Gercek adlar "BP_Projectile*_C" biciminde (log kaniti, [R1]).
    // =======================================================================
    bool IsProjectileClass(const std::string& n)
    {
        if (n.empty() || n.size() > 64) return false;

        const bool prefix = n.rfind("BP_Projectile",  0) == 0    // ★ gercek bicim
                         || n.rfind("ABP_Projectile", 0) == 0
                         || n.rfind("UBP_Projectile", 0) == 0;
        if (!prefix) return false;

        if (n.find("Trail")      != std::string::npos) return false;
        if (n.find("Path")       != std::string::npos) return false;
        if (n.find("PathSphere") != std::string::npos) return false;

        return true;
    }

    // =======================================================================
    //  ADIM 1b: GEVSETILMIS teshis filtresi ("gercek isim ne?").
    //  ★ Yanlis pozitifler elendi: UI ikonlari, ammo standlari, Trail.
    // =======================================================================
    static const char* kTokens[] = {
        "Projectile", "Bullet", "Cannonball", "CannonBall", "Arrow",
        "Bolt", "GrapeShot", "Harpoon", "Grenade", "Bomb", "Pellet", "Shot"
    };

    static bool LooksLikeProjectile(const std::string& n)
    {
        if (n.empty() || n.size() > 64) return false;
        if (n.find("Trail")       != std::string::npos) return false;
        if (n.find("Path")        != std::string::npos) return false;
        if (n.find("Pickup")      != std::string::npos) return false;
        if (n.find("QuickAcces")  != std::string::npos) return false;  // BP_QuickAccesPoint_*
        if (n.find("QuickAccess") != std::string::npos) return false;
        if (n.find("UI")          != std::string::npos) return false;  // BP_UIArrow_C

        for (const char* t : kTokens)
            if (n.find(t) != std::string::npos) return true;
        return false;
    }

    // =======================================================================
    //  Dump'a ozel isim onbellegi. Ayni indeks binlerce kez sorulur.
    // =======================================================================
    static std::unordered_map<int32_t, std::string> s_names;

    static const std::string& ProjName(int32_t idx)
    {
        if (s_names.size() > 20000) s_names.clear();   // sinirsiz buyume korumasi

        auto it = s_names.find(idx);
        if (it == s_names.end())
            it = s_names.emplace(idx, UE4::ReadFName(Game::GNames, idx)).first;
        return it->second;
    }

    // =======================================================================
    //  F9 -> izleme modu. Tus dongusu YALNIZCA bayrak koyar (thread yarisi yok).
    // =======================================================================
    static volatile LONG s_dumpRequested = 0;

    void RequestProjectileDump()   { InterlockedExchange(&s_dumpRequested, 1); }
    bool IsProjectileDumpPending() { return s_dumpRequested != 0; }

    // =======================================================================
    //  Sabitler
    // =======================================================================
    static const ULONGLONG kWatchMs = 20000;   // izleme suresi
    static const ULONGLONG kScanMs  = 80;      // tarama araligi (throttle)

    static const uintptr_t kActorSpan        = 0x2000;  // ★ actor'un taranan ilk 8 KB'i
    static const uintptr_t kObjSpan          = 0x0300;  // aday nesnenin taranan basi
    static const int       kMaxNarrowPerScan = 32;      // tek taramada incelenecek DAR mermi
    static const int       kMaxBroadPerScan  = 8;       // ek olarak incelenecek GENIS aday
    static const int       kMaxCandPerActor  = 96;      // actor basina taranacak UObject adayi
    static const int       kMaxObjScans      = 3;       // actor basina nesne taramasi (maliyet siniri)
    static const int       kMaxNameLog       = 120;     // en fazla bu kadar farkli yeni isim logla

    static const float kMinMoveSpeed = 120.0f;   // bu hizin altindaki nesne "duruk" sayilir
    static const float kMagLo        = 0.65f;    // |v| alt sinir carpani
    static const float kMagHi        = 1.45f;    // |v| ust sinir carpani
    static const float kMinCos       = 0.70f;    // yon uyumu (cos)

    // =======================================================================
    //  Asagidaki durum SADECE render thread'inden erisilir (kilit gerekmez).
    // =======================================================================
    static ULONGLONG s_watchEnd   = 0;
    static ULONGLONG s_lastScan   = 0;
    static int       s_lastNarrow = -1;
    static int       s_lastBroad  = -1;
    static bool      s_strongDone = false;

    struct PrevSamp {
        uintptr_t root  = 0;
        FVector   pos;
        ULONGLONG tick  = 0;
    };

    static std::unordered_map<uintptr_t, PrevSamp>  s_prev;      // actor -> son ornek
    static std::unordered_map<uintptr_t, int>       s_scans;     // actor -> kac kez nesne tarandi
    static std::unordered_map<uintptr_t, uintptr_t> s_pmcCache;  // actor -> PMC offset (isimle bulunan)

    // --- OYLAR: (offset) -> kac kez dogrulandi ---------------------------
    static std::unordered_map<int, int>   s_actorVelVotes;   // actor+off
    static std::unordered_map<int, float> s_actorVelMag;
    // ★ SINIF BAZLI: her mermi sinifi (silah) icin AYRI oy tablosu.
    //   Boylece crossbow/musket/blunderbuss offsetleri birbirine KARISMAZ.
    static std::unordered_map<std::string, std::unordered_map<int, int>>   s_clsActorVotes;
    static std::unordered_map<std::string, std::unordered_map<int, float>> s_clsActorMag;
    static std::unordered_map<int, int>   s_rootVelVotes;    // root+off  (ComponentVelocity)
    static std::unordered_map<int, float> s_rootVelMag;
    static std::unordered_map<unsigned long long, int>   s_pmcVelVotes;  // (actorOff<<32)|velOff
    static std::unordered_map<unsigned long long, float> s_pmcVelMag;

    // --- KESIF / KORUMA durumu -------------------------------------------
    static std::unordered_set<uintptr_t>        s_pmcMiss;        // isimle PMC bulunamadi
    static std::unordered_set<uintptr_t>        s_seen;           // izleme basinda gorulen aktorler
    static std::unordered_set<uintptr_t>        s_logged;         // "izlenen" satiri yazildi mi
    static std::unordered_set<uintptr_t>        s_hitLogged;      // "velocity adayi" yazildi mi
    static std::unordered_set<uintptr_t>        s_missLogged;     // "PMC bulunamadi" yazildi mi
    static bool                                 s_seeded = false; // ilk tarama yapildi mi
    static std::unordered_map<std::string, int> s_newNames;       // izleme sirasinda YENI cikan siniflar
    static std::unordered_map<std::string, int> s_broadNames;     // genis filtreye takilan siniflar
    static std::unordered_map<std::string, int> s_candNames;      // ★ aday nesnelerin SINIF adlari
    static std::unordered_map<uintptr_t, int>   s_probeCnt;       // ★ ham prob satir siniri
    static int                                  s_candNoName = 0; // sinif adi cozulemeyen aday

    // =======================================================================
    //  ★ SAYFA GECERLILIK ONBELLEGI ★
    //  VirtualQuery ile sayfa durumunu BIR KEZ ogreniriz; gecersiz sayfalar
    //  icin Memory::Read() cagirmayiz -> SEH istisnasi hic uretilmez.
    // =======================================================================
    static std::unordered_map<uintptr_t, bool> s_pageOk;

    static bool PageReadable(uintptr_t p)
    {
        if (s_pageOk.size() > 16384) s_pageOk.clear();

        const uintptr_t page = p & ~(uintptr_t)0xFFF;
        auto it = s_pageOk.find(page);
        if (it != s_pageOk.end()) return it->second;

        MEMORY_BASIC_INFORMATION mbi{};
        bool ok = false;
        if (VirtualQuery((LPCVOID)page, &mbi, sizeof(mbi)) == sizeof(mbi))
            ok = (mbi.State == MEM_COMMIT) &&
                 ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0);

        s_pageOk.emplace(page, ok);
        return ok;
    }

    // =======================================================================
    //  ★ ANLIK GORUNTU (SNAPSHOT) ★
    //  actor'un / aday nesnenin ilk `span` baytini SAYFA SAYFA tek SEH ile
    //  kopyalar. Boylece 992 ayri SEH yerine ~1-2 SEH olur ve dolasim saf
    //  C++ olur (SEH maliyeti yok).
    // =======================================================================
    static thread_local std::vector<unsigned char> s_snap;     // actor
    static thread_local std::vector<unsigned char> s_objsnap;  // aday nesne

    static void SnapshotActor(uintptr_t actor)
    {
        if (s_snap.size() < kActorSpan) s_snap.assign(kActorSpan, 0);
        std::memset(s_snap.data(), 0, (size_t)kActorSpan);
        Memory::SafeCopy(actor, s_snap.data(), (size_t)kActorSpan);
    }

    static void SnapshotObject(uintptr_t p)
    {
        if (s_objsnap.size() < kObjSpan) s_objsnap.assign(kObjSpan, 0);
        std::memset(s_objsnap.data(), 0, (size_t)kObjSpan);
        Memory::SafeCopy(p, s_objsnap.data(), (size_t)kObjSpan);
    }

    static inline uintptr_t SnapQword(uintptr_t off)
    {
        return *(const uintptr_t*)(s_snap.data() + off);
    }

    // =======================================================================
    //  ★ VEKTOR ESLESTIRME (isimden BAGIMSIZ) ★
    //
    //  buf+vo adresindeki 3 ardisik float'i, olculen hiz ve yon ile
    //  karsilastirir. Esleşme varsa true doner ve buyuklugu mOut'a yazar.
    //
    //  memcpy kullanilir: tampon icindeki 4-bayt hizasiz okumada UB olmasin.
    // =======================================================================
    static inline bool MatchTriple(const unsigned char* buf,
                                   size_t vo, size_t span,
                                   float lo, float hi,
                                   float nx, float ny, float nz,
                                   float& mOut)
    {
        if (vo + 12 > span) return false;

        float x, y, z;
        std::memcpy(&x, buf + vo,     4);
        std::memcpy(&y, buf + vo + 4, 4);
        std::memcpy(&z, buf + vo + 8, 4);

        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;

        const float m = std::sqrt(x * x + y * y + z * z);
        if (m < lo || m > hi) return false;

        const float cosv = (x * nx + y * ny + z * nz) / m;
        if (cosv < kMinCos) return false;

        mOut = m;
        return true;
    }

    // =======================================================================
    //  ★ HAM PROB ★  (filtresiz)
    //  Verilen taban + offset'teki FVector'i dogrudan okur. Boylece
    //  TURETILMIS alanlari (or. UMovementComponent::Velocity = 0xC0),
    //  buyukluk/yon filtresi tutmasa bile log'da GORURUZ.
    // =======================================================================
    static bool ProbeVector(uintptr_t base, uintptr_t off,
                            float& x, float& y, float& z)
    {
        const FVector v = Memory::Read<FVector>(base + off);
        x = v.X; y = v.Y; z = v.Z;
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }

    // Bir tamponda tum 4-bayt hizali pencereleri tarar, oy toplar.
    static void SearchBuffer(const unsigned char* buf, size_t span,
                             float lo, float hi,
                             float nx, float ny, float nz,
                             std::unordered_map<int, int>&   votes,
                             std::unordered_map<int, float>& mags)
    {
        for (size_t vo = 0x20; vo + 12 <= span; vo += 4) {
            float m;
            if (MatchTriple(buf, vo, span, lo, hi, nx, ny, nz, m)) {
                ++votes[(int)vo];
                mags[(int)vo] = m;
            }
        }
    }

    // =======================================================================
    //  En cok oy alan kayit (unordered_map "begin()" sirali degildir).
    // =======================================================================
    static void BestOf(const std::unordered_map<int, int>& votes,
                       int& bestOff, int& bestCnt)
    {
        bestOff = 0; bestCnt = 0;
        for (const auto& kv : votes)
            if (kv.second > bestCnt) { bestCnt = kv.second; bestOff = kv.first; }
    }

    static void BestOf64(const std::unordered_map<unsigned long long, int>& votes,
                         unsigned long long& bestKey, int& bestCnt)
    {
        bestKey = 0; bestCnt = 0;
        for (const auto& kv : votes)
            if (kv.second > bestCnt) { bestCnt = kv.second; bestKey = kv.first; }
    }

    // =======================================================================
    //  Oylari buyukten kucuge yaz.
    // =======================================================================
    static void PrintOffsetVotes(const std::unordered_map<int, int>&   votes,
                                 const std::unordered_map<int, float>& mags,
                                 const char* title)
    {
        if (votes.empty()) return;

        std::vector<std::pair<int, int>> v(votes.begin(), votes.end());
        std::sort(v.begin(), v.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                return a.second > b.second;
            });

        Diagnostics::Log("[dump] --- %s (offset : oy) ---", title);
        int n = 0;
        for (const auto& kv : v) {
            auto m = mags.find(kv.first);
            Diagnostics::Log("[dump]     0x%03X : %d   (son mag=%.0f)",
                (unsigned)kv.first, kv.second,
                m != mags.end() ? m->second : 0.0f);
            if (++n >= 12) break;
        }
    }

    static void PrintPmcVotes()
    {
        if (s_pmcVelVotes.empty()) return;

        std::vector<std::pair<unsigned long long, int>> v(
            s_pmcVelVotes.begin(), s_pmcVelVotes.end());
        std::sort(v.begin(), v.end(),
            [](const std::pair<unsigned long long, int>& a,
               const std::pair<unsigned long long, int>& b) {
                return a.second > b.second;
            });

        Diagnostics::Log("[dump] --- NESNE ICINDE velocity adaylari (actor+X / nesne+Y : oy) ---");
        int n = 0;
        for (const auto& kv : v) {
            const unsigned actorOff = (unsigned)(kv.first >> 32);
            const unsigned velOff   = (unsigned)(kv.first & 0xFFFFFFFFull);
            auto m = s_pmcVelMag.find(kv.first);
            Diagnostics::Log("[dump]     actor+0x%03X / +0x%03X : %d   (son mag=%.0f)",
                actorOff, velOff, kv.second,
                m != s_pmcVelMag.end() ? m->second : 0.0f);
            if (++n >= 12) break;
        }
    }

    // ★ SINIF BAZLI OZET: her mermi sinifi (silah) icin en guclu actor-ici
    //   velocity offset'ini AYRI yazdirir. Boylece arkadasina ates edince
    //   "CrossbowArrow -> 0x25C, Musket -> 0x260" gibi NET tablo cikar.
    static void PrintClassVotes()
    {
        if (s_clsActorVotes.empty()) return;

        Diagnostics::Log("[dump] === SINIF BAZLI velocity tablosu (silah : offset : oy) ===");

        std::vector<std::pair<std::string, int>> order;
        for (const auto& kv : s_clsActorVotes) {
            int tot = 0;
            for (const auto& o : kv.second) tot += o.second;
            order.emplace_back(kv.first, tot);
        }
        std::sort(order.begin(), order.end(),
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return a.second > b.second;
            });

        for (const auto& oc : order) {
            const std::string& cn = oc.first;
            const auto& votes = s_clsActorVotes[cn];

            std::vector<std::pair<int, int>> v(votes.begin(), votes.end());
            std::sort(v.begin(), v.end(),
                [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                    return a.second > b.second;
                });

            std::string top;
            int n = 0;
            for (const auto& kv : v) {
                char buf[48];
                const auto m = s_clsActorMag[cn].find(kv.first);
                const float mag = (m != s_clsActorMag[cn].end()) ? m->second : 0.0f;
                snprintf(buf, sizeof(buf), " actor+0x%03X(%d,%.0f)", (unsigned)kv.first, kv.second, mag);
                top += buf;
                if (++n >= 3) break;
            }

            Diagnostics::Log("[dump]   %-44s :%s", cn.c_str(), top.c_str());
        }
    }

    // =======================================================================
    //  Guclu aday olustuysa (>=3 oy) erken rapor ver.
    //  Sirasi: PMC velocity > RootComponent velocity > actor-ici velocity
    // =======================================================================
    static void ReportStrong()
    {
        if (s_strongDone) return;

        unsigned long long bestKey = 0;
        int bestCnt = 0;
        BestOf64(s_pmcVelVotes, bestKey, bestCnt);

        if (bestKey && bestCnt >= 3) {
            s_strongDone = true;
            const unsigned actorOff = (unsigned)(bestKey >> 32);
            const unsigned velOff   = (unsigned)(bestKey & 0xFFFFFFFFull);

            Diagnostics::Log("=== MERMI VELOCITY ADAYI (guclu, oy=%d) ===", bestCnt);
            Diagnostics::Log("[dump] >>> Actor_ProjectileMovement = 0x%03X", actorOff);
            Diagnostics::Log("[dump] >>> PMC_Velocity             = 0x%03X  (son mag=%.0f)",
                velOff, s_pmcVelMag.count(bestKey) ? s_pmcVelMag[bestKey] : 0.0f);
            Diagnostics::Log("[dump] (buyukluk + yon, konum degisiminden dogrulandi)");
            return;
        }

        int off = 0, cnt = 0;
        BestOf(s_rootVelVotes, off, cnt);
        if (off && cnt >= 3) {
            s_strongDone = true;
            Diagnostics::Log("=== ROOT COMPONENT VELOCITY ADAYI (guclu, oy=%d) ===", cnt);
            Diagnostics::Log("[dump] >>> Actor_RootComponent = 0x%03X",
                (unsigned)Offsets::Actor_RootComponent);
            Diagnostics::Log("[dump] >>> ComponentVelocity  = 0x%03X  (son mag=%.0f)",
                (unsigned)off, s_rootVelMag.count(off) ? s_rootVelMag[off] : 0.0f);
            return;
        }

        BestOf(s_actorVelVotes, off, cnt);
        if (off && cnt >= 3) {
            s_strongDone = true;
            Diagnostics::Log("=== ACTOR-ICI VELOCITY ADAYI (guclu, oy=%d) ===", cnt);
            Diagnostics::Log("[dump] >>> Actor+0x%03X  (son mag=%.0f)",
                (unsigned)off, s_actorVelMag.count(off) ? s_actorVelMag[off] : 0.0f);
        }
    }

    // =======================================================================
    //  Tek mermi aktorunu incele.
    // =======================================================================
    static void AnalyzeOne(uintptr_t actor, const std::string& name)
    {
        const ULONGLONG now  = GetTickCount64();
        const uintptr_t root = Memory::Read<uintptr_t>(actor + Offsets::Actor_RootComponent);

        FVector pos{};
        if (root)
            pos = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);

        // ---- Konum degisiminden OLCULEN hiz ----
        float measured = 0.0f;
        bool  haveMeas = false;
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;

        auto it = s_prev.find(actor);
        if (it != s_prev.end() && it->second.root == root && root) {
            const double dt = (double)(now - it->second.tick) / 1000.0;
            if (dt > 0.005) {
                const float dx = pos.X - it->second.pos.X;
                const float dy = pos.Y - it->second.pos.Y;
                const float dz = pos.Z - it->second.pos.Z;
                const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                measured = dist / (float)dt;

                if (measured > 50.0f && measured < 200000.0f && dist > 0.01f) {
                    haveMeas = true;
                    nx = dx / dist; ny = dy / dist; nz = dz / dist;   // birim yon
                }
            }
        }

        s_prev[actor] = PrevSamp{ root, pos, now };

        // Duruk nesneler (sandik, ammo standi) icin arama YAPILMAZ.
        if (!haveMeas || measured < kMinMoveSpeed) return;
        if (s_logged.insert(actor).second)
            Diagnostics::Log("[dump] izlenen: %s  hiz=%.0f cm/s", name.c_str(), measured);

        const float lo = measured * kMagLo;
        const float hi = measured * kMagHi;

        // ===================================================================
        //  (A) ACTOR'UN KENDISI  ->  ReplicatedMovement.LinearVelocity vb.
        // ===================================================================
        SnapshotActor(actor);
        SearchBuffer(s_snap.data(), kActorSpan, lo, hi, nx, ny, nz,
                     s_actorVelVotes, s_actorVelMag);
        // ★ sinif bazli oy: bu merminin sinifina AYRI tabloda oy at
        SearchBuffer(s_snap.data(), kActorSpan, lo, hi, nx, ny, nz,
                     s_clsActorVotes[name], s_clsActorMag[name]);

        // ===================================================================
        //  (B) ROOT COMPONENT  ->  USceneComponent::ComponentVelocity
        //      (AActor::GetVelocity() bu alani dondurur)
        // ===================================================================
        if (root) {
            SnapshotObject(root);
            SearchBuffer(s_objsnap.data(), kObjSpan, lo, hi, nx, ny, nz,
                         s_rootVelVotes, s_rootVelMag);
        }

        // ===================================================================
        //  (C) ADAY NESNELER  ->  PMC icindeki Velocity
        //      Maliyet siniri: actor basina en fazla kMaxObjScans kez.
        // ===================================================================
        int& scans = s_scans[actor];
        if (scans >= kMaxObjScans) return;
        ++scans;

        int cand = 0;
        for (uintptr_t off = 0x40; off + 8 <= kActorSpan; off += 8) {
            const uintptr_t p = SnapQword(off);
            if (!Memory::IsSanePtr(p) || !PageReadable(p)) continue;

            const uintptr_t vft = Memory::Read<uintptr_t>(p);          // vftable
            if (!Memory::IsSanePtr(vft)) continue;

            const uintptr_t cls = Memory::Read<uintptr_t>(p + 0x10);   // ClassPrivate
            if (!Memory::IsSanePtr(cls) || !PageReadable(cls)) continue;

            const int32_t idx = Memory::Read<int32_t>(cls + 0x18);     // UObject::Name
            if (idx <= 0 || idx > 10000000) continue;                  // FName isini basta kes

            const std::string cn = ProjName(idx);
            if (cn.empty() || cn == "?") { ++s_candNoName; continue; }  // gercek UObject degil

            if (cn.size() < 64) ++s_candNames[cn];                     // ★ TANI: gercek adlar

            if (++cand > kMaxCandPerActor) break;

            // ---- nesnenin basinda velocity ara ----
            SnapshotObject(p);

            bool  hit    = false;
            int   hitOff = 0;
            float hitMag = 0.0f;

            for (size_t vo = 0x20; vo + 12 <= kObjSpan; vo += 4) {
                float m;
                if (MatchTriple(s_objsnap.data(), vo, kObjSpan, lo, hi, nx, ny, nz, m)) {
                    const unsigned long long key =
                        ((unsigned long long)off << 32) | (unsigned)vo;
                    ++s_pmcVelVotes[key];
                    s_pmcVelMag[key] = m;
                    if (!hit) { hit = true; hitOff = (int)vo; hitMag = m; }
                }
            }

            // Isim "Movement" iceriyorsa PMC offset'ini de kaydet (bonus).
            if (cn.find("Movement") != std::string::npos)
                s_pmcCache[actor] = off;

            if (hit && s_hitLogged.insert(actor).second)
                Diagnostics::Log("[dump]   actor+0x%03X (%s) icinde velocity adayi: +0x%03X mag=%.0f",
                    (unsigned)off, cn.c_str(), hitOff, hitMag);

            // ---- ★ HAM PROB: UMovementComponent::Velocity (turetilmis 0xC0) ----
            //  Filtre tutmasa bile gercek degeri gormek icin dogrudan okuma.
            if (s_probeCnt[actor] < 4) {
                float px, py, pz;
                if (ProbeVector(p, Offsets::Move_Velocity, px, py, pz)) {
                    const float pm = std::sqrt(px * px + py * py + pz * pz);
                    if (pm > 300.0f && pm < 200000.0f) {
                        ++s_probeCnt[actor];
                        Diagnostics::Log(
                            "[dump]   prob actor+0x%03X (%s) +0x%03X=(%.0f, %.0f, %.0f) mag=%.0f",
                            (unsigned)off, cn.c_str(), (unsigned)Offsets::Move_Velocity,
                            px, py, pz, pm);
                    }
                }
            }
        }

        if (!s_pmcCache.count(actor) && s_missLogged.insert(actor).second)
            Diagnostics::Log("[dump]   isimde 'Movement' yok (taranan UObject aday=%d)", cand);
    }

    // =======================================================================
    //  Tek tarama: seviyedeki aktorleri gez.
    //  ★ Iki gecis: once DAR (gercek mermi), sonra GENIS adaylar.
    // =======================================================================
    static std::vector<std::pair<uintptr_t, std::string>> s_narrowList;
    static std::vector<std::pair<uintptr_t, std::string>> s_broadList;

    static void ScanOnce()
    {
        if (!Game::GWorld) return;

        const uintptr_t level = Memory::Read<uintptr_t>(
            Game::GWorld + Offsets::UWorld_PersistentLevel);
        if (!level) return;

        const uintptr_t data  = Memory::Read<uintptr_t>(level + Offsets::ULevel_Actors);
        const int32_t   count = Memory::Read<int32_t>(level + Offsets::ULevel_Actors + 8);
        if (!data || count <= 0 || count > 16384) return;

        s_narrowList.clear();
        s_broadList.clear();

        const bool firstScan = !s_seeded;

        for (int i = 0; i < count; ++i) {
            const uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::IsSanePtr(a)) continue;

            const uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);   // ClassPrivate
            if (!Memory::IsSanePtr(cp)) continue;

            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);
            if (idx <= 0 || idx > 10000000) continue;

            const std::string name = ProjName(idx);

            // ---- KESIF: izleme basladiktan SONRA ortaya cikan aktorler ----
            //   Mermi ATES ANINDA spawn edilir; gercek sinif adini tahmin
            //   etmeye gerek yok, ates edince adi log'a duser.
            if (s_seen.insert(a).second && !firstScan &&
                !name.empty() && name.size() < 64) {
                if (++s_newNames[name] == 1 && (int)s_newNames.size() <= kMaxNameLog)
                    Diagnostics::Log("[dump] YENI AKTOR: %s", name.c_str());
            }

            if (IsProjectileClass(name)) {
                s_narrowList.emplace_back(a, name);
            }
            else if (LooksLikeProjectile(name)) {
                s_broadList.emplace_back(a, name);
                if (!name.empty() && name.size() < 64) ++s_broadNames[name];
            }
        }

        s_seeded = true;

        const int narrow = (int)s_narrowList.size();
        const int broad  = (int)s_broadList.size();

        int analyzed = 0;
        for (int i = 0; i < narrow && i < kMaxNarrowPerScan; ++i, ++analyzed)
            AnalyzeOne(s_narrowList[i].first, s_narrowList[i].second);
        for (int i = 0; i < broad && i < kMaxBroadPerScan; ++i, ++analyzed)
            AnalyzeOne(s_broadList[i].first, s_broadList[i].second);

        if (narrow != s_lastNarrow || broad != s_lastBroad) {
            s_lastNarrow = narrow;
            s_lastBroad  = broad;
            Diagnostics::Log("[dump] tarama: dar=%d  genis=%d  (incelenen=%d)",
                narrow, broad, analyzed);
        }

        ReportStrong();
    }

    // =======================================================================
    //  Izleme bitti -> ozet.
    // =======================================================================
    static void DumpNameHistogram(const char* title,
                                  std::unordered_map<std::string, int>& m,
                                  int maxLines)
    {
        if (m.empty()) return;

        std::vector<std::pair<std::string, int>> v(m.begin(), m.end());
        std::sort(v.begin(), v.end(),
            [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                return a.second > b.second;
            });

        Diagnostics::Log("[dump] --- %s (sinif adi : adet) ---", title);
        int n = 0;
        for (const auto& kv : v) {
            Diagnostics::Log("[dump]     %-46s : %d", kv.first.c_str(), kv.second);
            if (++n >= maxLines) break;
        }
    }

    static void FinishWatch()
    {
        Diagnostics::Log("=== PROJECTILE IZLEME BITTI (ozet) ===");
        Diagnostics::Log("[dump] izlenen hareketli mermi sayisi = %d", (int)s_logged.size());
        Diagnostics::Log("[dump] isimde 'Movement' bulunan mermi = %d", (int)s_pmcCache.size());
        Diagnostics::Log("[dump] nesne taramasi yapilan aktor = %d", (int)s_scans.size());
        Diagnostics::Log("[dump] sinif adi cozulemeyen aday isaretci = %d", s_candNoName);

        DumpNameHistogram("GENIS filtreye takilan siniflar",       s_broadNames, 30);
        DumpNameHistogram("IZLEME SIRASINDA YENI CIKAN siniflar",  s_newNames,   30);
        DumpNameHistogram("★ ADAY NESNE sinif adlari",             s_candNames,  40);

        PrintPmcVotes();
        PrintClassVotes();
        PrintOffsetVotes(s_rootVelVotes,  s_rootVelMag,  "ROOT COMPONENT velocity adaylari (root+X)");
        PrintOffsetVotes(s_actorVelVotes, s_actorVelMag, "ACTOR-ICI velocity adaylari (actor+X)");

        if (s_pmcVelVotes.empty() && s_rootVelVotes.empty() && s_actorVelVotes.empty()) {
            Diagnostics::Log("[dump] VELOCITY adayi YOK. Muhtemel sebepler:");
            Diagnostics::Log("[dump]   (a) izleme boyunca HAREKETLI mermi yakalanmadi");
            Diagnostics::Log("[dump]       -> canliyken silahi al, ATES ET, sonra F9");
            Diagnostics::Log("[dump]   (b) mermi aktorunun RootComponent offset'i farkli");
            Diagnostics::Log("[dump]   (c) velocity baska bir nesnede ve > 0x300 ofsette");
        }
    }

    // =======================================================================
    //  Render thread'inden HER KARE cagrilir. Istek yoksa ~0 maliyet.
    // =======================================================================
    static void ResetWatchState(ULONGLONG now)
    {
        s_prev.clear();
        s_scans.clear();
        s_pmcCache.clear();
        s_pmcMiss.clear();
        s_seen.clear();
        s_logged.clear();
        s_hitLogged.clear();
        s_missLogged.clear();
        s_pageOk.clear();
        s_seeded = false;
        s_newNames.clear();
        s_broadNames.clear();
        s_candNames.clear();
        s_probeCnt.clear();
        s_candNoName = 0;
        s_actorVelVotes.clear();
        s_actorVelMag.clear();
        s_clsActorVotes.clear();
        s_clsActorMag.clear();
        s_rootVelVotes.clear();
        s_rootVelMag.clear();
        s_pmcVelVotes.clear();
        s_pmcVelMag.clear();
        s_narrowList.clear();
        s_broadList.clear();
        s_strongDone = false;
        s_lastNarrow = s_lastBroad = -1;
        s_lastScan   = 0;
        s_watchEnd   = now + kWatchMs;
    }

    void ProcessProjectileDump()
    {
        const ULONGLONG now = GetTickCount64();

        // Yeni istek (F9)
        if (InterlockedCompareExchange((volatile LONG*)&s_dumpRequested, 0, 1) == 1) {
            const bool active = (s_watchEnd != 0 && now < s_watchEnd);

            if (active) {
                // ★ SIFIRLAMA YOK, SADECE UZAT  ([R3])
                s_watchEnd = now + kWatchMs;
                Diagnostics::Log("[dump] izleme ZATEN AKTIF -> %llu sn'ye uzatildi (sifirlanmadi)",
                    kWatchMs / 1000);
            }
            else {
                ResetWatchState(now);
                Diagnostics::Log("=== PROJECTILE IZLEME BASLADI (%llu sn) ===", kWatchMs / 1000);
                Diagnostics::Log("[dump] canliyken silahi al, ATES ET. Her %llu ms taranir.",
                    kScanMs);
                Diagnostics::Log("[dump] (bu sure icinde F9'a tekrar basmak sifirlamaz, uzatir)");
                Diagnostics::Log("[dump] arama: actor / RootComponent / aday nesne icinde velocity");
            }
        }

        if (s_watchEnd == 0) return;

        if (now >= s_watchEnd) {
            s_watchEnd = 0;
            __try   { FinishWatch(); }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                Diagnostics::Log("[dump] SEH: ozet yazilirken istisna");
            }
            return;
        }

        if (now - s_lastScan < kScanMs) return;
        s_lastScan = now;

        __try   { ScanOnce(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Diagnostics::Log("[dump] SEH: tarama istisnasi (atlandi, izleme devam)");
        }
    }

    // =======================================================================
    //  MERMI LISTESI  --  MagicBullet (YOL HARITASI ADIM 3)
    //
    //  Yukaridaki ScanOnce() F9 TESHIS taramasidir (agir: 4 KB snapshot +
    //  FName cozumlemesi + oy toplama). Bu fonksiyon ise her karede
    //  cagrilabilir: yalnizca sinif adi filtresinden gecer ve ~150 ms
    //  onbelleklenir, boylece kare hizina etkisi olcum edilemez duzeydedir.
    //
    //  Hiz alani burada okunmaz; MagicBullet canli okur ve yazar
    //  (Offsets::Proj_Velocity = actor+0x258).
    // =======================================================================
    std::vector<Projectile> GetProjectiles()
    {
        static ULONGLONG s_nextScan = 0;
        static std::vector<Projectile> s_cache;

        const ULONGLONG now = GetTickCount64();
        if (now < s_nextScan) return s_cache;
        s_nextScan = now + 150;

        s_cache.clear();
        if (!Game::GWorld) return s_cache;

        const uintptr_t level = Memory::Read<uintptr_t>(
            Game::GWorld + Offsets::UWorld_PersistentLevel);
        if (!level) return s_cache;

        const uintptr_t data  = Memory::Read<uintptr_t>(level + Offsets::ULevel_Actors);
        const int32_t   count = Memory::Read<int32_t>(level + Offsets::ULevel_Actors + 8);
        if (!data || count <= 0 || count > 16384) return s_cache;

        for (int i = 0; i < count; ++i) {
            const uintptr_t a = Memory::Read<uintptr_t>(data + (uintptr_t)i * 8);
            if (!Memory::IsSanePtr(a)) continue;

            const uintptr_t cp = Memory::Read<uintptr_t>(a + 0x10);   // ClassPrivate
            if (!Memory::IsSanePtr(cp)) continue;

            const int32_t idx = Memory::Read<int32_t>(cp + 0x18);
            if (idx <= 0 || idx > 10000000) continue;

            const std::string name = ProjName(idx);
            if (!IsProjectileClass(name)) continue;

            const uintptr_t root = Memory::Read<uintptr_t>(
                a + Offsets::Actor_RootComponent);
            if (!root) continue;

            Projectile p;
            p.actor     = a;
            p.className = name;
            p.pos       = Memory::Read<FVector>(root + Offsets::Scene_RelativeLocation);

            if (s_cache.size() < 64) s_cache.push_back(p);
        }

        return s_cache;
    }

} // namespace Actors

#pragma once
#include "core/Memory.h"
#include <string>
#include <cstdint>

namespace UE4 {

    // ======================================================================
    //  FName cozumlemesi (UE4.23+ FNamePool)
    //
    //  ★★ KOK NEDEN DUZELTMESI ("ESP hicbir sey cizmiyor") ★★
    //
    //  FNameEntry basligindaki uzunluk alani build'e gore IKI farkli
    //  yerlesimde olabilir:
    //
    //    WITH_CASE_PRESERVING_NAME (UE4.23+ varsayilan / modern build)
    //        bit 0     : bIsWide
    //        bit 1..15 : Len                 -> (header >> 1) & 0x7FFF
    //
    //    legacy (case-preserving kapali / eski build)
    //        bit 0     : bIsWide
    //        bit 1..5  : ProbeHash
    //        bit 6..15 : Len                 -> (header >> 6) & 0x3FF
    //
    //  Onceki kod YALNIZCA legacy yerlesimi okuyordu. Modern bir build'de
    //  bu her isim icin len == 0 dondurur -> ReadFName "?" verir ->
    //  IsCharClass() hicbir aktore uymaz ve item CLASS_MAP'i de hicbir
    //  seye uymaz -> hem karakter hem item ESP bombos kalir. Yasanan
    //  belirtinin tamami budur.
    //
    //  Cozum: iki yerlesim de denenir, sonuc DOGRULANIR
    //    (a) yazdirilabilir ASCII olmali
    //    (b) isimden sonraki bayt 0 (NUL/terminator) olmali
    //  Dogrulama gecen aday kazanir; hicbiri gecmezse yazdirilabilir olan
    //  ilk aday yedek olarak kullanilir.
    // ======================================================================

    namespace detail {

        inline bool AllPrintable(const std::string& s) {
            if (s.empty() || s.size() > 250) return false;
            for (unsigned char c : s)
                if (c < 0x20 || c > 0x7E) return false;
            return true;
        }

        // entry = FNameEntry baslangici (header dahil). Isim header'dan SONRA baslar.
        inline std::string ReadChars(uintptr_t entry, int len, bool wide) {
            std::string out;
            out.reserve((size_t)len);
            const int step = wide ? 2 : 1;
            for (int i = 0; i < len; ++i) {
                const uint16_t c = Memory::Read<uint16_t>(entry + 2 + (uintptr_t)i * step);
                if (!c) break;
                out += wide ? (char)(c & 0x7F) : (char)(c & 0xFF);
            }
            return out;
        }

        inline std::string Decode(uintptr_t entry, uint16_t header) {
            const bool wide = (header & 1) != 0;
            const int  lengths[2] = {
                (header >> 1) & 0x7FFF,   // modern (case preserving)
                (header >> 6) & 0x03FF    // legacy
            };

            std::string fallback;
            for (int i = 0; i < 2; ++i) {
                const int len = lengths[i];
                if (len < 1 || len > 250) continue;

                std::string s = ReadChars(entry, len, wide);
                if (!AllPrintable(s)) continue;

                // Dogru uzunlukta, ismin bittigi yerde 0 (NUL / dolgu) bulunur.
                const uint16_t next = Memory::Read<uint16_t>(
                    entry + 2 + (uintptr_t)len * (wide ? 2 : 1));
                if (next == 0) return s;
                if (fallback.empty()) fallback = std::move(s);
            }
            return fallback;
        }

    } // namespace detail

    inline std::string ReadFName(uintptr_t pool, int32_t idx) {
        if (!pool || idx < 0 || idx > 10000000) return "?";
        const int32_t block = idx >> 16;
        const int32_t off   = idx & 0xFFFF;

        uintptr_t blockPtr = Memory::Read<uintptr_t>(pool + 0x10 + (uintptr_t)block * 8);

        // Bazi build'lerde GNames offseti FNamePool NESNESINE, bazilarinda
        // FNamePool* ISARETCISINE isaret eder; ikincisinde bir kez deref gerekir.
        if (!blockPtr) {
            const uintptr_t poolPtr = Memory::Read<uintptr_t>(pool);
            if (poolPtr)
                blockPtr = Memory::Read<uintptr_t>(poolPtr + 0x10 + (uintptr_t)block * 8);
            if (!blockPtr) return "?";
        }

        const uintptr_t entry  = blockPtr + (uintptr_t)off * 2;
        const uint16_t  header = Memory::Read<uint16_t>(entry);

        std::string s = detail::Decode(entry, header);
        return s.empty() ? "?" : s;
    }

    inline std::string ReadFString(uintptr_t addr) {
        if (!addr) return "";
        uintptr_t data = Memory::Read<uintptr_t>(addr);
        int32_t   len = Memory::Read<int32_t>(addr + 8);
        if (!data || len <= 0 || len > 128) return "";

        std::string out;
        out.reserve(len);
        for (int i = 0; i < len; i++) {
            uint16_t c = Memory::Read<uint16_t>(data + (uintptr_t)i * 2);
            if (!c) break;
            out += (char)(c & 0xFF);
        }
        return out;
    }
}

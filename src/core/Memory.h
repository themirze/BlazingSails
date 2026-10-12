#pragma once
#include <Windows.h>
#include <cstdint>
#include <cstring>

namespace Memory {

    // ==================================================================
    //  ★ SEH'siz UCUZ SANITY CHECK ★
    //
    //  Onceden Read<T>() her cagrida dogrudan __try/__except'e giriyordu.
    //  Gecersiz adres basina SEH maliyeti ~1-5 mikrosaniyedir. Projectile
    //  dump'i aktorun 0x100..0x2000 araligini 8 bayt adimla tariyordu
    //  (~992 okuma/aktor); o araligin buyuk kismi eslenmemis sayfadir ->
    //  F9 penceresinde istisna sayaci ~1200/sn'ye cikti (log: 705 -> 435226)
    //  ve oyunun kare hizi dustu.
    //
    //  Asagidaki iki kontrol SEH'ye GIRMEDEN sacma adresleri eler:
    //    * 0 ve kucuk tamsayilar (cig offset degerleri)
    //    * cekirdek / non-canonical adresler (x64 user mode ust siniri)
    //
    //  NOT: OKUMA adresi icin hizalama sarti KOYULMAZ — float alanlari
    //       (+4, +0x11C gibi) 8'e hizali olmak zorunda degildir.
    //       Hizalama sarti yalnizca NESNE ISARETCISI kontrolunde (IsSanePtr)
    //       uygulanir.
    // ==================================================================
    inline bool IsSaneRange(uintptr_t a) {
        return a >= 0x10000 && a <= 0x00007FFFFFFFFFFF;
    }

    // UObject / AActor / UActorComponent isaretcileri icin:
    // aralik + 8-byte hizalama (UE nesneleri en az 8'e hizalalidir).
    inline bool IsSanePtr(uintptr_t p) {
        return (p & 0x7) == 0 && IsSaneRange(p);
    }

    template<typename T>
    inline T Read(uintptr_t addr) {
        if (!IsSaneRange(addr)) return T{};      // ★ SEH'ye girmeden reddet
        __try { return *(T*)addr; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return T{}; }
    }

    template<typename T>
    inline bool Write(uintptr_t addr, const T& v) {
        if (!IsSaneRange(addr)) return false;
        __try { *(T*)addr = v; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // Not: Valid() davranisi korunur (ucuz aralik kontrolu + SEH sondasi).
    // Hizalama sarti BURADA aranmaz ki mevcut cagrilarin semantigi degismesin.
    inline bool Valid(uintptr_t addr) {
        if (!IsSaneRange(addr)) return false;
        __try { volatile char c = *(char*)addr; return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // ==================================================================
    //  ★ TOPLU OKUMA ★  (SEH maliyetini ~1000x azaltir)
    //
    //  Kaynaktan `size` bayti SAYFA SAYFA (4 KB) kopyalar; tek bir
    //  eslenmemis sayfa tum kopyayi iptal etmez, o sayfa bos kalir.
    //
    //  Neden onemli: PMC offset'i icin actor'un icini taramak gerekir.
    //  Eski kod her alan icin ayri Read() yapiyordu (992 SEH/aktor).
    //  Bu fonksiyonla ayni is 1 SEH ile yapilir; sonra tampon uzerinde
    //  SEH'siz, saf C++ ile dolasilir.
    //
    //  NOT: Bu fonksiyonda C++ yikim gerektiren nesne YOKTUR; aksi halde
    //       MSVC C2712 ("cannot use __try in functions that require
    //       object unwinding") hatasi verir. Bu yuzden POD tutulur.
    // ==================================================================
    inline void SafeCopy(uintptr_t src, void* dst, size_t size)
    {
        unsigned char* out = (unsigned char*)dst;
        size_t done = 0;
        while (done < size) {
            const size_t chunk = (size - done) > 0x1000 ? 0x1000 : (size - done);
            __try { std::memcpy(out + done, (const void*)(src + done), chunk); }
            __except (EXCEPTION_EXECUTE_HANDLER) { /* o sayfa bos kaldi */ }
            done += chunk;
        }
    }

}

#include "core/Diagnostics.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

namespace {

    constexpr int kRingLines = 64;    // cokme aninda dosyaya dokulecek son satirlar
    constexpr int kRingChars = 200;

    CRITICAL_SECTION g_lock      = {};
    bool             g_lockInit  = false;
    char             g_logPath[MAX_PATH] = {};

    char             g_ring[kRingLines][kRingChars] = {};
    volatile LONG    g_ringPos   = 0;

    volatile LONG    g_avCount   = 0;            // gorulen toplam istisna sayisi
    volatile LONG    g_lastCode  = 0;
    volatile LONG64  g_lastAddr  = 0;

    volatile LONG    g_dumping   = 0;            // reentrancy korumasi
    volatile LONG    g_frame     = 0;

    // --- modul araliklari: VEH icinde UCUZ filtre icin onceden hesaplanir ---
    uintptr_t        g_ourBase   = 0, g_ourEnd  = 0;   // BlazingInternal.dll
    uintptr_t        g_mainBase  = 0, g_mainEnd = 0;   // oyun (.exe)
    volatile LONG    g_siteCount = 0;                  // yakalanan "oyun kodu" AV sayisi

    // --- temiz sokme (DLL unload) icin kayitli handler'lar -----------------
    PVOID                        g_vehHandle   = nullptr;
    LPTOP_LEVEL_EXCEPTION_FILTER g_prevUef     = nullptr;
    bool                         g_uefInstalled = false;

    // --- dusuk seviyeli yardimcilar ---------------------------------------

    // Kilit TUTMAYAN yazim. Cokme aninda baska bir thread dosya kilidini
    // tutuyor olabilecegi icin burada dosya her seferinde yeniden acilir.
    void AppendFileUnlocked(const char* text)
    {
        if (!g_logPath[0] || !text) return;

        FILE* f = nullptr;
        if (fopen_s(&f, g_logPath, "a") == 0 && f)
        {
            fputs(text, f);
            fflush(f);
            fclose(f);
        }
    }

    void PushRing(const char* line)
    {
        const LONG i = InterlockedIncrement(&g_ringPos) - 1;
        strncpy_s(g_ring[i % kRingLines], kRingChars, line, _TRUNCATE);
    }

    void Format(char* out, size_t cap, const char* fmt, va_list ap)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        const int n = snprintf(out, cap, "[%02d:%02d:%02d.%03d][t%05lu] ",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            (unsigned long)GetCurrentThreadId());

        const size_t off = (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
        vsnprintf(out + off, cap - off, fmt, ap);

        const size_t len = strlen(out);
        if (len + 2 < cap) { out[len] = '\n'; out[len + 1] = '\0'; }
    }

    // Adresi "modul+offset" olarak yazar; dump olmadan da hangi kutuphanede
    // oldugunu anlamamizi saglar.
    void DescribeAddress(const void* addr, char* out, size_t cap)
    {
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &mod) && mod)
        {
            char path[MAX_PATH] = {};
            GetModuleFileNameA(mod, path, MAX_PATH);
            const char* name = strrchr(path, '\\');
            name = name ? name + 1 : path;
            snprintf(out, cap, "%s+0x%llX", name,
                (unsigned long long)((uintptr_t)addr - (uintptr_t)mod));
        }
        else
        {
            snprintf(out, cap, "0x%p (bilinen modul yok)", addr);
        }
    }

    void BuildLogPath()
    {
        char dir[MAX_PATH] = {};
        strcpy_s(dir, "C:\\BlazingInternal\\logs");
        CreateDirectoryA("C:\\BlazingInternal", nullptr);

        if (!CreateDirectoryA(dir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            // Yazilamazsa %TEMP% altina dus
            char tmp[MAX_PATH] = {};
            if (GetTempPathA(MAX_PATH, tmp) > 0)
            {
                snprintf(dir, sizeof(dir), "%sBlazingInternal", tmp);
                CreateDirectoryA(dir, nullptr);
            }
        }

        SYSTEMTIME st;
        GetLocalTime(&st);
        snprintf(g_logPath, sizeof(g_logPath),
            "%s\\blazing_%04d%02d%02d_%02d%02d%02d_p%lu.log",
            dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            (unsigned long)GetCurrentProcessId());
    }

    const char* DumpDir()
    {
        static char s_dir[MAX_PATH] = {};
        if (!s_dir[0] && g_logPath[0])
        {
            strncpy_s(s_dir, g_logPath, _TRUNCATE);
            char* slash = strrchr(s_dir, '\\');
            if (slash) *slash = '\0';
        }
        return s_dir;
    }

    void WriteCrashDump(const char* tag, EXCEPTION_POINTERS* ep)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);

        char path[MAX_PATH] = {};
        snprintf(path, sizeof(path), "%s\\crash_%s_%04d%02d%02d_%02d%02d%02d_p%lu.dmp",
            DumpDir(), tag, st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            (unsigned long)GetCurrentProcessId());

        HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            AppendFileUnlocked("minidump: dosya olusturulamadi\n");
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION ei = {};
        ei.ThreadId          = GetCurrentThreadId();
        ei.ExceptionPointers = ep;
        ei.ClientPointers    = FALSE;

        const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h,
            (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo),
            ep ? &ei : nullptr, nullptr, nullptr);

        CloseHandle(h);

        char msg[512] = {};
        snprintf(msg, sizeof(msg), "minidump: %s (%s)\n", path, ok ? "yazildi" : "BASARISIZ");
        AppendFileUnlocked(msg);
    }

    void DumpRingToFile()
    {
        AppendFileUnlocked("--- son log satirlari ---\n");
        const LONG pos = g_ringPos;
        for (int i = 0; i < kRingLines; i++)
        {
            const char* line = g_ring[(pos + i) % kRingLines];
            if (line[0]) AppendFileUnlocked(line);
        }
        AppendFileUnlocked("--- son log satirlari sonu ---\n");
    }

    // Bir modulun [base,end) araligini PE basliklarindan hesaplar.
    void ModuleRange(HMODULE hm, uintptr_t& base, uintptr_t& end)
    {
        base = end = 0;
        if (!hm) return;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(hm);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            reinterpret_cast<const BYTE*>(hm) + dos->e_lfanew);
        if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return;
        base = reinterpret_cast<uintptr_t>(hm);
        end  = base + nt->OptionalHeader.SizeOfImage;
    }

    void CalcModuleRanges()
    {
        HMODULE ours = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&Diagnostics::Install), &ours);
        ModuleRange(ours, g_ourBase, g_ourEnd);
        ModuleRange(GetModuleHandleA(nullptr), g_mainBase, g_mainEnd);
    }

    // ★ "Ilk sans" cokme yakalama: VEH, oyunun kendi UEF'inden ONCE cagrilir.
    //   Boylece UE4'un crash reporter'i bizim filtremizi gecersiz kilmis olsa
    //   bile, oyun kodundaki erisim ihlalini burada diske yazabiliyoruz.
    void LogCrashStack(const char* tag, EXCEPTION_POINTERS* ep)
    {
        char line[512] = {};
        const auto* er = ep->ExceptionRecord;

        const uintptr_t rip = (uintptr_t)er->ExceptionAddress;
        char where[256] = {};
        DescribeAddress((void*)rip, where, sizeof(where));

        const ULONG64 rd = (er->NumberParameters >= 1) ? er->ExceptionInformation[0] : 0;
        const ULONG64 fa = (er->NumberParameters >= 2) ? er->ExceptionInformation[1] : 0;

        AppendFileUnlocked("\n=== COKME YERI (ilk sans / VEH) ===\n");
        snprintf(line, sizeof(line),
            "code=0x%08lX  rip=%s  (%s)  thread=%lu  hatali_adres=0x%llX\n",
            (unsigned long)er->ExceptionCode, where, tag,
            (unsigned long)GetCurrentThreadId(), (unsigned long long)fa);
        AppendFileUnlocked(line);

        snprintf(line, sizeof(line), "erisim_turu=%s\n",
            rd == 0 ? "READ" : (rd == 1 ? "WRITE" : (rd == 8 ? "EXEC" : "?")));
        AppendFileUnlocked(line);

        // Basit yigin taramasi: RSP'den yukari, modul adresine benzeyen
        // 8-bayt degerlerini "cagri zinciri" olarak yaz.
        const auto* ctx = ep->ContextRecord;
        if (ctx)
        {
#ifdef _M_X64
            const uintptr_t sp = (uintptr_t)ctx->Rsp;
            const uintptr_t bp = (uintptr_t)ctx->Rbp;
#else
            const uintptr_t sp = (uintptr_t)ctx->Esp;
            const uintptr_t bp = (uintptr_t)ctx->Ebp;
#endif
            AppendFileUnlocked("--- yigin (modul+offset) ---\n");
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery((LPCVOID)sp, &mbi, sizeof(mbi)) &&
                mbi.State == MEM_COMMIT && mbi.Protect != PAGE_NOACCESS)
            {
                size_t avail = (size_t)((uintptr_t)mbi.BaseAddress + mbi.RegionSize - sp);
                if (avail > 2048) avail = 2048;

                int printed = 0;
                for (size_t off = 0; off + 8 <= avail && printed < 24; off += 8)
                {
                    uintptr_t v = 0;
                    memcpy(&v, (const void*)(sp + off), sizeof(v));
                    if (v < 0x10000) continue;

                    HMODULE m = nullptr;
                    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                            (LPCSTR)v, &m) || !m)
                        continue;
                    char w[256] = {};
                    DescribeAddress((void*)v, w, sizeof(w));
                    snprintf(line, sizeof(line), "  rsp+0x%03zX  %s\n", off, w);
                    AppendFileUnlocked(line);
                    ++printed;
                }
                if (printed == 0) AppendFileUnlocked("  (modul adresi bulunamadi)\n");
            }
            else
            {
                AppendFileUnlocked("  (yigin okunamadi)\n");
            }

            snprintf(line, sizeof(line), "rbp=0x%llX  rsp=0x%llX\n",
                (unsigned long long)bp, (unsigned long long)sp);
            AppendFileUnlocked(line);
        }
    }

    // Vectored handler: SEH ile yakalanan erisim ihlalleri DAHIL her istisnada
    // cagrilir. Bu yuzden burada yalnizca UCUZ islem yapiyoruz; "oyun kodu"
    // disindaki istisnalar (bizim Memory problarimiz) hic loglanmaz.
    LONG CALLBACK Veh(EXCEPTION_POINTERS* ep)
    {
        InterlockedIncrement(&g_avCount);
        if (ep && ep->ExceptionRecord)
        {
            const ULONG code = ep->ExceptionRecord->ExceptionCode;
            g_lastCode = (LONG)code;
            g_lastAddr = (LONG64)(uintptr_t)ep->ExceptionRecord->ExceptionAddress;

            // ★ Gercek cokme: erisim ihlali VE hatali komut OYUN modulunde.
            //   Bizim SEH problarimiz BlazingInternal.dll icinde oldugu icin
            //   burada elenir -> log spam'i olmaz.
            if (code == 0xC0000005 /* AV */)
            {
                const uintptr_t rip = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
                if (rip >= g_mainBase && rip < g_mainEnd)
                {
                    if (InterlockedIncrement(&g_siteCount) <= 4)
                    {
                        LogCrashStack("AV", ep);
                        DumpRingToFile();
                    }
                }
            }
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    LONG WINAPI Uef(EXCEPTION_POINTERS* ep)
    {
        if (InterlockedExchange(&g_dumping, 1) != 0)
            return EXCEPTION_CONTINUE_SEARCH;

        char line[1024] = {};
        AppendFileUnlocked("\n=== YAKALANMAMIS ISTISNA (unhandled exception) ===\n");

        if (ep && ep->ExceptionRecord)
        {
            char where[256] = {};
            DescribeAddress(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));

            snprintf(line, sizeof(line),
                "code=0x%08lX  addr=%s  thread=%lu  faulting_param=0x%llX\n",
                (unsigned long)ep->ExceptionRecord->ExceptionCode, where,
                (unsigned long)GetCurrentThreadId(),
                (unsigned long long)(ep->ExceptionRecord->NumberParameters > 0
                    ? ep->ExceptionRecord->ExceptionInformation[0] : 0));
            AppendFileUnlocked(line);

            if (ep->ContextRecord)
            {
#ifdef _M_X64
                snprintf(line, sizeof(line), "rip=0x%llX  rsp=0x%llX  rbp=0x%llX\n",
                    (unsigned long long)ep->ContextRecord->Rip,
                    (unsigned long long)ep->ContextRecord->Rsp,
                    (unsigned long long)ep->ContextRecord->Rbp);
#else
                snprintf(line, sizeof(line), "eip=0x%08lX  esp=0x%08lX  ebp=0x%08lX\n",
                    (unsigned long)ep->ContextRecord->Eip,
                    (unsigned long)ep->ContextRecord->Esp,
                    (unsigned long)ep->ContextRecord->Ebp);
#endif
                AppendFileUnlocked(line);
            }
        }

        snprintf(line, sizeof(line), "toplam istisna (SEH dahil)=%ld  son_code=0x%08lX  son_addr=0x%llX\n",
            (long)g_avCount, (unsigned long)g_lastCode, (unsigned long long)g_lastAddr);
        AppendFileUnlocked(line);

        DumpRingToFile();
        WriteCrashDump("unhandled", ep);

        // Oyunun kendi crash reporter'i da calissin diye zinciri devam ettiriyoruz.
        return EXCEPTION_CONTINUE_SEARCH;
    }

} // namespace

namespace Diagnostics {

    void Install()
    {
#ifndef NDEBUG
        if (!g_lockInit)
        {
            InitializeCriticalSection(&g_lock);
            g_lockInit = true;
        }

        if (!g_logPath[0]) BuildLogPath();

        CalcModuleRanges();

        g_vehHandle    = AddVectoredExceptionHandler(1, Veh);
        g_prevUef      = SetUnhandledExceptionFilter(Uef);
        g_uefInstalled = true;

        Log("=== BlazingInternal teshis basladi ===");
        Log("log dosyasi: %s", g_logPath);
#endif
    }

    // ★ Temiz sokme: handler'lar DLL unload edilmeden once geri alinir.
    //   Aksi halde hem VEH hem UEF serbest birakilmis DLL bellegine isaret
    //   eder; oyunun sonraki her istisnasinda (bizim SEH problarimiz dahil)
    //   o adrese atlanir -> cokme. "Clean quit" akisinin on sarti budur.
    void Uninstall()
    {
        if (g_vehHandle) {
            RemoveVectoredExceptionHandler(g_vehHandle);
            g_vehHandle = nullptr;
        }
        if (g_uefInstalled) {
            SetUnhandledExceptionFilter(g_prevUef);   // onceki (oyun) filtresine don
            g_uefInstalled = false;
        }
    }

    void Log(const char* fmt, ...)
    {
#ifndef NDEBUG
        char buf[512] = {};

        va_list ap;
        va_start(ap, fmt);
        Format(buf, sizeof(buf), fmt, ap);
        va_end(ap);

        PushRing(buf);

        // ★★ TEK YAZMA YOLU: her satirda "a" modunda ac/yaz/kapat.
        //    ESKI HATA: log dosyasi bir kez "w" ile acilip handle tutuluyordu;
        //    cokme yolundaki "a" eklemeleri o handle'in ESKI offset'i tarafindan
        //    eziliyordu -> cokme anindaki log satirlari kayboluyordu.
        if (g_lockInit)
        {
            EnterCriticalSection(&g_lock);
            AppendFileUnlocked(buf);
            LeaveCriticalSection(&g_lock);
        }

        fputs(buf, stdout);
        OutputDebugStringA(buf);
#else
        (void)fmt;
#endif
    }

    // Render thread'inden cagrilir. "frame=..." satiri gorunuyorsa render
    // thread'i o ana kadar calisiyor demektir; donma anini bu sayede buluruz.
    void Heartbeat()
    {
#ifndef NDEBUG
        const LONG f = InterlockedIncrement(&g_frame);
        if ((f % 120) == 0)
            Log("[hb] frame=%ld  istisna=%ld", (long)f, (long)g_avCount);
#endif
    }

    const char* LogPath() { return g_logPath; }

    long AvCount() { return (long)g_avCount; }

    int HandleOverlayException(EXCEPTION_POINTERS* ep)
    {
#ifndef NDEBUG
        if (ep && ep->ExceptionRecord)
        {
            char where[256] = {};
            DescribeAddress(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));

            Log("[!] OVERLAY ISTISNASI: code=0x%08lX at %s (thread %lu)",
                (unsigned long)ep->ExceptionRecord->ExceptionCode, where,
                (unsigned long)GetCurrentThreadId());
        }
        else
        {
            Log("[!] OVERLAY ISTISNASI (bilgi yok)");
        }

        InterlockedExchange(&g_dumping, 1);
        DumpRingToFile();
        // ★★ ONEMLI: burada minidump ALMIYORUZ.
        //    MiniDumpWriteDump tum thread'leri askiya alir; bir istisna ele
        //    alinirken cagrilmasi oyunu kilitler/oldurur (log'un tam o anda
        //    kesilmesinin sebebi buydu: dump yazilirken surec oldu).
        //    Overlay hatasi zaten "oyun yasasin" senaryosudur: log + bayrak yeter.
        //    Gercek (yakalanmamis) cokmelerde Uef minidump almaya devam eder.
        InterlockedExchange(&g_dumping, 0);

        // Istisnayi "ele al": oyun olmesin, sadece overlay kapatilsin.
#else
        (void)ep;
#endif
        return EXCEPTION_EXECUTE_HANDLER;
    }

} // namespace Diagnostics

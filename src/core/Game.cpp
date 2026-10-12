#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/Offsets.h"

namespace Game {
    uintptr_t moduleBase = 0;
    DWORD     processId = 0;
    uintptr_t GWorld = 0, GObjects = 0, GNames = 0;
    uintptr_t localPlayerController = 0;
    uintptr_t localPawn = 0;
    uintptr_t cameraManager = 0;

    bool Attach() {
        HMODULE mod = GetModuleHandleW(L"BlazingSails-Win64-Shipping.exe");
        if (!mod) return false;
        moduleBase = (uintptr_t)mod;
        processId = GetCurrentProcessId();
        return true;
    }

    void Detach() {
        moduleBase = 0;
        GWorld = GObjects = GNames = 0;
        localPlayerController = localPawn = cameraManager = 0;
    }

    bool IsAttached() { return moduleBase != 0; }

    void ReadGlobals() {
        if (!IsAttached()) return;
        GWorld = Memory::Read<uintptr_t>(moduleBase + Offsets::GWorld);
        GObjects = Memory::Read<uintptr_t>(moduleBase + Offsets::GObjects);
        GNames = moduleBase + Offsets::GNames;
    }

    bool IsLocalPlayer(uintptr_t actor) {
        return actor && actor == localPawn;
    }
}
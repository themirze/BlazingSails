#pragma once
#include <Windows.h>
#include <cstdint>
#include "sdk/UE4_Types.h"

namespace Game {
    extern uintptr_t moduleBase;
    extern DWORD     processId;
    extern uintptr_t GWorld, GObjects, GNames;
    extern uintptr_t localPlayerController;
    extern uintptr_t localPawn;
    extern uintptr_t cameraManager;

    bool  Attach();
    void  Detach();
    bool  IsAttached();
    void  ReadGlobals();
    bool  ReadCamera(FMinimalViewInfo& pov);
    bool  IsLocalPlayer(uintptr_t actor);
}
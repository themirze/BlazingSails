#include "core/Game.h"
#include "core/Memory.h"
#include "sdk/Offsets.h"

namespace Game {
    bool ReadCamera(FMinimalViewInfo& pov) {
        if (!GWorld) return false;

        uintptr_t gi = Memory::Read<uintptr_t>(GWorld + Offsets::UWorld_OwningGameInstance);
        if (!gi) return false;

        uintptr_t playersPtr = Memory::Read<uintptr_t>(gi + Offsets::GameInstance_LocalPlayers);
        if (!playersPtr) return false;

        uintptr_t player = Memory::Read<uintptr_t>(playersPtr);
        if (!player) return false;

        uintptr_t pc = Memory::Read<uintptr_t>(player + Offsets::LocalPlayer_PlayerController);
        if (!pc) return false;
        localPlayerController = pc;

        uintptr_t pcm = Memory::Read<uintptr_t>(pc + Offsets::PC_PlayerCameraManager);
        if (!pcm) return false;
        cameraManager = pcm;

        uintptr_t cache = pcm + Offsets::PCM_CameraCachePrivate;
        pov = Memory::Read<FMinimalViewInfo>(cache + 0x10);

        localPawn = Memory::Read<uintptr_t>(pc + Offsets::PC_AcknowledgedPawn);
        return true;
    }
}
#pragma once
#include <cstdint>

namespace Offsets {
    constexpr uintptr_t GWorld = 0x51A4738;
    constexpr uintptr_t GNames = 0x5022C80;
    constexpr uintptr_t GObjects = 0x505F300;

    constexpr uintptr_t UWorld_OwningGameInstance = 0x180;
    constexpr uintptr_t UWorld_PersistentLevel = 0x30;

    constexpr uintptr_t GameInstance_LocalPlayers = 0x38;
    constexpr uintptr_t LocalPlayer_PlayerController = 0x30;

    constexpr uintptr_t PC_AcknowledgedPawn = 0x2A0;
    constexpr uintptr_t PC_PlayerCameraManager = 0x2B8;

    constexpr uintptr_t PCM_CameraCachePrivate = 0x1AE0;

    constexpr uintptr_t Actor_RootComponent = 0x130;
    constexpr uintptr_t Scene_RelativeLocation = 0x11C;

    constexpr uintptr_t ULevel_Actors = 0x98;

    constexpr uintptr_t Char_UserName = 0x648;
    constexpr uintptr_t Char_Mesh = 0x280;
    constexpr uintptr_t Char_Health = 0x760;
    constexpr uintptr_t Char_MaxHealth = 0x764;
    constexpr uintptr_t Char_IsDead = 0x8F6;
    constexpr uintptr_t Char_IsEnemy = 0x0CA0;

    // ------------------------------------------------------------------
    //  MERMI (PROJECTILE) --  Magic bullet icin
    //
    //  ★ KESIN DURUM (v24, RE/ce ile dogrulandi):
    //     ABP_ProjectileBase_C sinifinda UProjectileMovementComponent YOK ve
    //     "Velocity" adli bir FVector ALAN YOK. Mermi, Blueprint Tick'inde
    //     konumu ELLE hesapliyor (manuel entegrator):
    //
    //         pos += Direction * TravelspeedXY * dt          (yatay)
    //         pos.Z += TravelspeedZ * dt;  TravelspeedZ -= Droprate * dt  (yay/yercekimi)
    //         pos += AddedVelocity                            (gemiden devralinan)
    //
    //     Per-instance alanlar (ABP_ProjectileBase_C):
    //       0x260  TravelspeedXY   float   yatay skaler hiz
    //       0x264  TravelspeedZ    float   dikey skaler hiz
    //       0x268  Droprate        float   dusus katsayisi (yercekimi)
    //       0x280  Direction       FVector birim yon vektoru   <-- GERCEK SURUCU
    //       0x294  PreviousLocation FVector
    //       0x2F8  NewLocation      FVector
    //       0x304  FinalLocation    FVector
    //       0x35C  AddedVelocity    FVector (gemiden devralinan hiz)
    //
    //  ★ Magic bullet artik velocity DEGIL, Direction'i (0x280) hedefe
    //    cevirerek calisir. Eskiden 0x260'a FVector yaziliyordu; oysa orasi
    //    4 baytlik skaler TravelspeedXY idi -> mermi donmuyor, komsu alanlar
    //    (TravelspeedZ, Droprate) bozuluyordu. (Bu, "calismiyor + cokme"
    //    semptomlarinin kok nedeni.)
    // ------------------------------------------------------------------
    constexpr uintptr_t Proj_Direction    = 0x280;  // birim yon vektoru (GERCEK surucu)
    constexpr uintptr_t Proj_TravelspeedXY = 0x260; // yatay skaler hiz (vektor DEGIL)
    constexpr uintptr_t Proj_TravelspeedZ  = 0x264; // dikey skaler hiz
    constexpr uintptr_t Proj_Droprate      = 0x268; // dusus katsayisi
    constexpr uintptr_t Proj_AddedVelocity = 0x35C; // gemiden devralinan hiz

    // ----- ESKI / GECERSIZ (sakli tutuluyor, kullanmayin) -----
    //  actor+0x258 = FRepMovement.LinearVelocity (replike, surmez; bazi
    //  mermilerde inf/sacma doner). Birincil yazma hedefi DEGILDIR.
    constexpr uintptr_t Proj_Velocity = 0x258;

    constexpr uintptr_t Actor_ProjectileMovement = 0x2B8; // [X] yanlis-pozitif (SoundCue)
    constexpr uintptr_t PMC_Velocity             = 0x104; // [X] SoundCue icindeki sabit float
    constexpr uintptr_t PMC_MaxSpeed             = 0;

    constexpr uintptr_t Move_UpdatedComponent = 0;     // bilinmiyor
    constexpr uintptr_t Move_UpdatedPrimitive = 0;     // bilinmiyor
    constexpr uintptr_t Move_Velocity         = 0x104; // [X] dump prob (tek offset DEGIL)
}

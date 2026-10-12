#pragma once
#include <cstring>
#include <cstdint>

// ======================================================================
//  Item sinif haritasi (CLASS_MAP)
//
//  Aktorun UClass FName'i (ClassPrivate -> NamePrivate) bu tablodaki
//  "cls" alanlariyla BIREBIR karsilastirilir. Eslesen her aktor bir item
//  olarak isaretlenir ve menude yalnizca kullanici tarafindan ACILAN
//  turler cizilir (varsayilan: hepsi kapali).
//
//  Yeni bir item eklemek icin: asagidaki tabloya { "BP_..._C", "Etiket",
//  Group_X } satiri eklemek yeterlidir; menu ve ESP otomatik guncellenir.
// ======================================================================

namespace ItemDB {

    enum Group : int {
        Group_Weapon = 0,   // Silahlar
        Group_Crate,        // Sandiklar
        Group_Ammo,         // Mermiler
        Group_Upgrade,      // Upgradeler
        Group_Cannon,       // Toplar
        Group_Resource,     // Kaynaklar
        Group_Count
    };

    // NOT: ImGui'nin varsayilan fontu Turkce'ye ozgu harfleri icermedigi
    //      icin etiketler ASCII tutulmustur.
    inline constexpr const char* kGroupNames[Group_Count] = {
        "Silahlar",
        "Sandiklar",
        "Mermiler",
        "Upgradeler",
        "Toplar",
        "Kaynaklar"
    };

    struct ItemDef {
        const char* cls;     // UE4 sinif adi (FName), _C dahil
        const char* label;   // Menu + ESP etiketi
        int         group;   // Group
    };

    inline constexpr ItemDef kItems[] = {
        // ---------------- Silahlar ----------------
        { "BP_PickupWeaponFlintlock_C",        "Flintlock",         Group_Weapon   },
        { "BP_PickupWeaponBlunderbussNew_C",   "Blunderbuss",       Group_Weapon   },
        { "BP_PickupWeaponCrossbow_C",         "Crossbow",          Group_Weapon   },
        { "BP_PickupWeaponMusket_C",           "Musket",            Group_Weapon   },
        { "BP_PickupWeaponClub_C",             "Club",              Group_Weapon   },
        { "BP_PickupWeaponRepeater_C",         "Repeater",          Group_Weapon   },
        { "BP_PickupWeaponDoubleBarrel_C",     "DoubleBarrel",      Group_Weapon   },
        { "BP_PickupWeaponhealingCrossbow_C",  "HealingCrossbow",   Group_Weapon   },
        { "BP_PickupWeaponCutlass_C",          "Cutlass",           Group_Weapon   },
        { "BP_PickupWeaponSniper2_C",          "Sniper",            Group_Weapon   },
        { "BP_PickupWeaponPickaxe_C",          "Pickaxe",           Group_Weapon   },
        { "BP_PickupWeaponShovel_C",           "Shovel",            Group_Weapon   },
        { "BP_PickupWeaponButchersKnife_C",    "ButchersKnife",     Group_Weapon   },
        { "BP_PickupWeaponDualFlintlock_C",    "DualFlintlock",     Group_Weapon   },
        { "BP_PickupWeaponQuadFlintlock_C",    "QuadFlintlock",     Group_Weapon   },
        { "BP_PickupWeaponDualQuadFlintlock_C","DualQuadFlintlock", Group_Weapon   },
        { "BP_PickupWeaponDragonPistol_C",     "DragonPistol",      Group_Weapon   },
        { "BP_PickupWeaponHalfBarrel_C",       "HalfBarrel",        Group_Weapon   },
        { "BP_PickupWeaponBombLauncher_C",     "BombLauncher",      Group_Weapon   },
        { "BP_WeaponBrokenBottle_C",           "BrokenBottle",      Group_Weapon   },

        // ---------------- Sandiklar ----------------
        { "BP_Lootcrate_C",                    "Lootcrate",         Group_Crate    },
        { "BP_TreasureChest_C",                "TreasureChest",     Group_Crate    },
        { "BP_AmmoBox_C",                      "AmmoBox",           Group_Crate    },
        { "BP_WoodCrate_C",                    "WoodCrate",         Group_Crate    },
        { "BP_WoodBarrel_C",                   "WoodBarrel",        Group_Crate    },
        { "BP_CarryChestBase_C",               "CarryChest",        Group_Crate    },
        { "BP_PickupExplosiveBarrel_C",        "ExplosiveBarrel",   Group_Crate    },

        // ---------------- Mermiler ----------------
        { "BP_PickupArrows_C",                 "Arrows",            Group_Ammo     },
        { "BP_PickupSmallShot_C",              "SmallShot",         Group_Ammo     },
        { "BP_PickupHeavyShot_C",              "HeavyShot",         Group_Ammo     },
        { "BP_PickupCannonBalls_C",            "CannonBalls",       Group_Ammo     },
        { "BP_PickupFishAmmo_C",               "FishAmmo",          Group_Ammo     },
        { "BP_PickupAntiCannon_C",             "AntiCannon",        Group_Ammo     },
        { "BP_PickupAntiSail_C",               "AntiSail",          Group_Ammo     },

        // ---------------- Upgradeler ----------------
        { "BP_PickupCorkUpgrade_C",            "CorkUpgrade",       Group_Upgrade  },
        { "BP_PickupCannonUpgrade_C",          "CannonUpgrade",     Group_Upgrade  },
        { "BP_PickupCapstanUpgrade_C",         "CapstanUpgrade",    Group_Upgrade  },
        { "BP_PickupPumpUpgrade_C",            "PumpUpgrade",       Group_Upgrade  },
        { "BP_PickupSteeringWheelUpgrade_C",   "SteeringWheelUpgrade", Group_Upgrade },
        { "BP_PickupBackpackUpgrade_C",        "BackpackUpgrade",   Group_Upgrade  },

        // ---------------- Toplar ----------------
        { "BP_VehicleCannon_C",                "VehicleCannon",     Group_Cannon   },
        { "BP_VehicleSwivelCannon_C",          "VehicleSwivelCannon", Group_Cannon },
        { "BP_PickupBasicSwivelCannon_C",      "BasicSwivelCannon", Group_Cannon   },

        // ---------------- Kaynaklar ----------------
        { "BP_PickupWood_C",                   "Wood",              Group_Resource },
        { "BP_PickupFuseLighter_C",            "FuseLighter",       Group_Resource },
        { "BP_PickupPaddle_C",                 "Paddle",            Group_Resource },
    };

    inline constexpr int kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

    // Sonundaki "_C" eki olmadan karsilastir: bazi build'lerde FName eki
    // tasimayabilir, boylece her iki durum da dogru eslesir.
    inline bool SameClass(const char* a, const char* b) {
        if (!a || !b) return false;
        const size_t la = std::strlen(a), lb = std::strlen(b);
        const size_t ca = (la > 2 && a[la - 2] == '_' && a[la - 1] == 'C') ? la - 2 : la;
        const size_t cb = (lb > 2 && b[lb - 2] == '_' && b[lb - 1] == 'C') ? lb - 2 : lb;
        return ca == cb && std::strncmp(a, b, ca) == 0;
    }

    inline int FindByClass(const char* cls) {
        if (!cls || !*cls || *cls == '?') return -1;
        for (int i = 0; i < kItemCount; ++i)
            if (SameClass(kItems[i].cls, cls)) return i;
        return -1;
    }

    inline int CountEnabled(const bool* flags) {
        int n = 0;
        for (int i = 0; i < kItemCount; ++i) if (flags[i]) ++n;
        return n;
    }

} // namespace ItemDB
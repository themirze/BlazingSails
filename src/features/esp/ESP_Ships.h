#pragma once
#include <imgui.h>
#include <vector>
#include "sdk/Actors_World.h"
#include "sdk/UE4_Types.h"

namespace ESP {
    // Gemileri (ve bulundugu konumu) ekrana isaretler: sinif adi + mesafe.
    void DrawShips(ImDrawList* dl, const std::vector<Actors::Ship>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos);
}

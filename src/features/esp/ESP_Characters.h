#pragma once
#include <imgui.h>
#include <vector>
#include "sdk/Actors.h"
#include "sdk/UE4_Types.h"

namespace ESP {
    void DrawCharacters(ImDrawList* dl, const std::vector<Actors::Character>& list,
        const FMinimalViewInfo& pov, int sw, int sh,
        const FVector& localPos, bool hasLocalPos);
}
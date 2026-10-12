#pragma once
#include "sdk/UE4_Types.h"
#include <imgui.h>

namespace UE4 {
    inline bool WorldToScreen(const FVector& world, const FMinimalViewInfo& pov,
        int sw, int sh, ImVec2& out)
    {
        constexpr float D2R = 3.1415926535897932f / 180.0f;
        const float pitch = pov.Rotation.Pitch * D2R;
        const float yaw = pov.Rotation.Yaw * D2R;
        const float roll = pov.Rotation.Roll * D2R;

        const float SP = sinf(pitch), CP = cosf(pitch);
        const float SY = sinf(yaw), CY = cosf(yaw);
        const float SR = sinf(roll), CR = cosf(roll);

        const FVector delta = world - pov.Location;
        const FVector xAxis(CP * CY, CP * SY, SP);
        const FVector yAxis(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
        const FVector zAxis(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);

        const float x = delta.Dot(yAxis);
        const float y = delta.Dot(zAxis);
        const float z = delta.Dot(xAxis);
        if (z < 1.0f) return false;

        float fov = pov.FOV;
        if (fov <= 0.0f || fov > 180.0f) fov = 110.0f;

        const float tanHalf = tanf(fov * D2R * 0.5f);
        const float aspect = (float)sw / (float)sh;

        out.x = (sw * 0.5f) * (1.0f + x / (z * tanHalf));
        out.y = (sh * 0.5f) * (1.0f - y * aspect / (z * tanHalf));
        return true;
    }
}
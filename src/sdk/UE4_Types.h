#pragma once
#include <cmath>
#include <cstdint>

struct FVector {
    float X = 0, Y = 0, Z = 0;
    FVector() = default;
    FVector(float x, float y, float z) : X(x), Y(y), Z(z) {}

    FVector operator-(const FVector& v) const { return { X - v.X, Y - v.Y, Z - v.Z }; }
    FVector operator+(const FVector& v) const { return { X + v.X, Y + v.Y, Z + v.Z }; }
    FVector operator*(float s)          const { return { X * s, Y * s, Z * s }; }

    float Dot(const FVector& v) const { return X * v.X + Y * v.Y + Z * v.Z; }
    float Length()              const { return std::sqrt(X * X + Y * Y + Z * Z); }
};

struct FRotator {
    float Pitch = 0, Yaw = 0, Roll = 0;
};

struct FMinimalViewInfo {
    FVector  Location           = {};   // 0x00
    FRotator Rotation           = {};   // 0x0C
    float    FOV                = 0.0f; // 0x18
    float    DesiredFOV         = 0.0f; // 0x1C
    float    OrthoWidth         = 0.0f; // 0x20
    float    OrthoNearClipPlane = 0.0f; // 0x24
    float    OrthoFarClipPlane  = 0.0f; // 0x28
    float    AspectRatio        = 0.0f; // 0x2C
};
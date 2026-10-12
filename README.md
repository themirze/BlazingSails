# BlazingInternal

Internal DirectX 11 overlay for *Blazing Sails* (UE4, 64-bit).

## Features

- **ImGui overlay** — DX11 `Present` hook, windowed/borderless rendering.
- **ESP** — players, items, and ships with distance and team filtering.
- **Magic bullet** — projectile direction steering (crosshair-based targeting with nearest-target fallback).
- **FOV circle** — visual aim cone, tied to the magic-bullet target cone.
- **Clean quit** — menu button that detaches the overlay and unloads hooks without closing the game.

## Build

- **Toolchain:** Visual Studio 2022+ (v145 toolset), Windows SDK 10.0.
- **Platform:** `x64` (the game is 64-bit only).
- **Configuration:** `Release`.
- **Dependencies:** [MinHook](https://github.com/TsudaKageyu/minhook) and [Dear ImGui](https://github.com/ocornut/imgui) (already under `external/`).

Build the `BlazingInternal.vcxproj` with `Release | x64`. The output is `x64\Release\BlazingInternal.dll`.

## Usage

Inject `BlazingInternal.dll` into the game process with your preferred injector, then press `INSERT` to toggle the menu.

## Notes

- Run the game in **windowed or borderless** mode; exclusive fullscreen can hide the overlay.
- Debug logging and the console are compiled out in `Release` builds (`NDEBUG`).

#pragma once

#include <imgui.h>
#include "config/Config.h"

// Custom, animated menu widgets. All row widgets take the full available width:
// label on the left, control on the right.
namespace Widgets
{
    // Smoothly moves a per-ID value toward `target` and returns it.
    float Animate(ImGuiID id, float target, float speed = 12.0f);
    void  AnimSet(ImGuiID id, float value);

    // Sidebar / layout
    bool  SidebarTab(const char* label, const char* hint, int icon, bool selected);
    bool  Segmented(const char* id, int* current, const char* const items[], int count);
    void  BeginCard(const char* title, float height = 0.0f);
    void  EndCard();
    void  Header(const char* title, const char* subtitle);

    // Row controls
    bool  Toggle(const char* label, bool* v, Color4* color = nullptr);
    bool  Slider(const char* label, float* v, float min, float max, const char* fmt = "%.0f");
    bool  Combo(const char* label, int* current, const char* const items[], int count);
    bool  ColorRow(const char* label, Color4* color);
    bool  Keybind(const char* label, int* vk);
    bool  Button(const char* label, float width = 0.0f, bool accent = false);
    void  InfoRow(const char* label, const char* value, const ImVec4* valueColor = nullptr);
    void  Hint(const char* text);

    const char* KeyName(int vk);

    // Icons drawn with primitives (no icon font needed)
    enum Icon { Icon_Info, Icon_Eye, Icon_Crosshair, Icon_Gear };
    void  DrawIcon(ImDrawList* dl, int icon, ImVec2 center, float size, ImU32 col);
}

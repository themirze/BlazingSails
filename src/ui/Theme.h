#pragma once

#include <Windows.h>
#include <imgui.h>
#include "config/Config.h"

namespace Theme
{
    // Base palette (dark, slightly blue-tinted)
    inline constexpr ImVec4 Bg          = { 0.055f, 0.059f, 0.075f, 1.00f };
    inline constexpr ImVec4 Sidebar     = { 0.040f, 0.043f, 0.055f, 1.00f };
    inline constexpr ImVec4 Card        = { 0.078f, 0.082f, 0.102f, 1.00f };
    inline constexpr ImVec4 CardBorder  = { 0.130f, 0.137f, 0.165f, 1.00f };
    inline constexpr ImVec4 Frame       = { 0.115f, 0.120f, 0.148f, 1.00f };
    inline constexpr ImVec4 FrameHover  = { 0.145f, 0.152f, 0.185f, 1.00f };
    inline constexpr ImVec4 Text        = { 0.925f, 0.930f, 0.950f, 1.00f };
    inline constexpr ImVec4 TextDim     = { 0.520f, 0.540f, 0.600f, 1.00f };
    inline constexpr ImVec4 TextFaint   = { 0.330f, 0.345f, 0.390f, 1.00f };
    inline constexpr ImVec4 Good        = { 0.300f, 0.850f, 0.550f, 1.00f };
    inline constexpr ImVec4 Warn        = { 0.980f, 0.750f, 0.300f, 1.00f };

    inline constexpr float  Rounding    = 8.0f;

    inline ImVec4 Accent()                  { const auto& a = cfg.menu.accent; return { a.r, a.g, a.b, 1.0f }; }
    inline ImVec4 Accent(float alpha)       { const auto& a = cfg.menu.accent; return { a.r, a.g, a.b, alpha }; }
    // Both overloads respect the current style alpha so custom drawing fades with the menu.
    inline ImU32  U32(const ImVec4& c, float alphaMul);
    inline ImU32  U32(const ImVec4& c)      { return U32(c, 1.0f); }
    inline ImU32  U32(const ImVec4& c, float alphaMul)
    {
        return ImGui::ColorConvertFloat4ToU32({ c.x, c.y, c.z, c.w * alphaMul * ImGui::GetStyle().Alpha });
    }

    struct Fonts
    {
        ImFont* regular = nullptr;
        ImFont* bold    = nullptr;
    };
    inline Fonts fonts;

    // Font sizes (unscaled)
    inline constexpr float SizeSmall  = 12.0f;
    inline constexpr float SizeBody   = 14.0f;
    inline constexpr float SizeTitle  = 20.0f;
    inline constexpr float SizeLogo   = 22.0f;

    inline void LoadFonts()
    {
        // ImGui 1.92+ fonts are dynamic: any size can be requested later via PushFont(font, size)
        ImGuiIO& io = ImGui::GetIO();
        auto load = [&](const char* path) -> ImFont*
        {
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
                return nullptr;
            return io.Fonts->AddFontFromFileTTF(path, SizeBody);
        };
        fonts.regular = load("C:\\Windows\\Fonts\\segoeui.ttf");
        fonts.bold    = load("C:\\Windows\\Fonts\\seguisb.ttf");

        if (!fonts.regular) fonts.regular = io.Fonts->AddFontDefault();
        if (!fonts.bold)    fonts.bold    = fonts.regular;
        io.FontDefault = fonts.regular;
    }

    inline void Apply()
    {
        ImGuiStyle& s = ImGui::GetStyle();

        s.WindowPadding     = { 0, 0 };
        s.WindowRounding    = 12.0f;
        s.WindowBorderSize  = 1.0f;
        s.ChildRounding     = Rounding;
        s.ChildBorderSize   = 1.0f;
        s.PopupRounding     = Rounding;
        s.PopupBorderSize   = 1.0f;
        s.FramePadding      = { 10, 6 };
        s.FrameRounding     = 6.0f;
        s.FrameBorderSize   = 0.0f;
        s.ItemSpacing       = { 10, 10 };
        s.ItemInnerSpacing  = { 8, 6 };
        s.ScrollbarSize     = 6.0f;
        s.ScrollbarRounding = 6.0f;
        s.GrabMinSize       = 10.0f;
        s.GrabRounding      = 6.0f;
        s.FontSizeBase      = SizeBody;

        ImVec4* c = s.Colors;
        c[ImGuiCol_Text]                 = Text;
        c[ImGuiCol_TextDisabled]         = TextDim;
        c[ImGuiCol_WindowBg]             = Bg;
        c[ImGuiCol_ChildBg]              = Card;
        c[ImGuiCol_PopupBg]              = { 0.090f, 0.094f, 0.118f, 0.98f };
        c[ImGuiCol_Border]               = CardBorder;
        c[ImGuiCol_BorderShadow]         = { 0, 0, 0, 0 };
        c[ImGuiCol_FrameBg]              = Frame;
        c[ImGuiCol_FrameBgHovered]       = FrameHover;
        c[ImGuiCol_FrameBgActive]        = FrameHover;
        c[ImGuiCol_ScrollbarBg]          = { 0, 0, 0, 0 };
        c[ImGuiCol_ScrollbarGrab]        = CardBorder;
        c[ImGuiCol_ScrollbarGrabHovered] = Accent(0.6f);
        c[ImGuiCol_ScrollbarGrabActive]  = Accent();
        c[ImGuiCol_CheckMark]            = Accent();
        c[ImGuiCol_SliderGrab]           = Accent();
        c[ImGuiCol_SliderGrabActive]     = Accent();
        c[ImGuiCol_Button]               = Frame;
        c[ImGuiCol_ButtonHovered]        = FrameHover;
        c[ImGuiCol_ButtonActive]         = Accent(0.5f);
        c[ImGuiCol_Header]               = Accent(0.18f);
        c[ImGuiCol_HeaderHovered]        = Accent(0.28f);
        c[ImGuiCol_HeaderActive]         = Accent(0.38f);
        c[ImGuiCol_Separator]            = CardBorder;
        c[ImGuiCol_TextSelectedBg]       = Accent(0.35f);
        c[ImGuiCol_NavCursor]            = Accent();
    }
}

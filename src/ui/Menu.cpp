#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <imgui_internal.h>
#include <cstdio>
#include <cmath>

namespace
{
    constexpr ImVec2 kMenuSize  = { 800.0f, 540.0f };
    constexpr float  kSidebarW  = 210.0f;

    struct TabInfo
    {
        const char* name;
        const char* hint;
        const char* subtitle;
        int         icon;
        void      (*render)();
    };

    const TabInfo kTabs[] =
    {
        { "Info", "Overview & status",  "Session overview, hotkeys and quick toggles", Widgets::Icon_Info,      Menu::RenderInfoTab },
        { "ESP",  "Visual awareness",   "Configure players and item visuals",          Widgets::Icon_Eye,       Menu::RenderEspTab  },
        { "Aim",  "Assist & trigger",   "Aim assistance, triggerbot and filters",      Widgets::Icon_Crosshair, Menu::RenderAimTab  },
        { "Misc", "Extras & settings",  "Overlay extras, menu style and configs",      Widgets::Icon_Gear,      Menu::RenderMiscTab },
    };

    int    g_tab        = 0;
    char   g_toast[96]  = {};
    double g_toastTime  = -100.0;

    // ★ HandleToggleKey() KALDIRILDI.
    //   Onceki surum GetAsyncKeyState(cfg.menu.toggleKey) ile tusu render (RHI)
    //   thread'inden yokluyordu ve Insert pratikte hic yakalanmiyordu. Menu
    //   ac/kapa artik oyunun GERCEK mesaj kuyrugundan islenir:
    //   bkz. Hook::PumpInputQueue() (Hook_ImGui.cpp) -> WM_KEYDOWN.

    void DrawLogo(ImDrawList* dl, ImVec2 p)
    {
        // Rounded gradient badge with a stylised flame
        const ImVec2 size(36, 36);
        const ImVec4 a  = Theme::Accent();
        const ImVec4 a2 = { ImMin(a.x + 0.15f, 1.0f), ImMin(a.y + 0.25f, 1.0f), a.z, 1.0f };
        dl->AddRectFilled(p - ImVec2(3, 3), p + size + ImVec2(3, 3), Theme::U32(Theme::Accent(), 0.15f), 12.0f);
        dl->AddRectFilled(p, p + size, Theme::U32(a), 10.0f);
        dl->AddRectFilledMultiColor(p + ImVec2(4, 4), p + size - ImVec2(4, 4),
            Theme::U32(a2, 0.0f), Theme::U32(a2, 0.0f), Theme::U32(a2, 0.6f), Theme::U32(a2, 0.6f));

        const ImVec2 c = p + size * 0.5f;
        dl->PathClear();
        dl->PathLineTo({ c.x, c.y - 11 });
        dl->PathBezierQuadraticCurveTo({ c.x + 11, c.y - 1 }, { c.x + 6, c.y + 10 });
        dl->PathLineTo({ c.x - 6, c.y + 10 });
        dl->PathBezierQuadraticCurveTo({ c.x - 11, c.y - 1 }, { c.x, c.y - 11 });
        dl->PathFillConcave(IM_COL32_WHITE);
        dl->AddCircleFilled({ c.x, c.y + 4 }, 3.2f, Theme::U32(a));

        const float x = p.x + size.x + 12.0f;
        dl->AddText(Theme::fonts.bold, Theme::SizeLogo, { x, p.y - 3.0f }, Theme::U32(Theme::Text), "BLAZING");
        dl->AddText(Theme::fonts.regular, Theme::SizeSmall, { x + 1.0f, p.y + 22.0f }, Theme::U32(Theme::TextDim), "internal  |  v1.0");
    }

    void DrawSidebar(ImVec2 winPos, ImVec2 winSize)
    {
        ImGui::SetCursorPos({ 0, 0 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 14, 20 });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 6 });
        ImGui::BeginChild("##sidebar", { kSidebarW, winSize.y }, ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        DrawLogo(dl, ImGui::GetCursorScreenPos() + ImVec2(4, 0));
        ImGui::Dummy({ 0, 52 });

        ImGui::PushFont(Theme::fonts.bold, 11.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextFaint);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);
        ImGui::TextUnformatted("NAVIGATION");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        for (int i = 0; i < IM_ARRAYSIZE(kTabs); i++)
        {
            if (Widgets::SidebarTab(kTabs[i].name, kTabs[i].hint, kTabs[i].icon, g_tab == i) && g_tab != i)
            {
                g_tab = i;
                Widgets::AnimSet(ImHashStr("##page_anim"), 0.0f);
            }
        }

        // Footer: menu key hint
        const float  footerH = 46.0f;
        const ImVec2 fMin(winPos.x + 14.0f, winPos.y + winSize.y - footerH - 16.0f);
        const ImVec2 fMax(winPos.x + kSidebarW - 14.0f, fMin.y + footerH);
        dl->AddRectFilled(fMin, fMax, Theme::U32(Theme::Card), Theme::Rounding);
        dl->AddRect(fMin, fMax, Theme::U32(Theme::CardBorder), Theme::Rounding);

        const float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 3.0f);
        dl->AddCircleFilled({ fMin.x + 16, fMin.y + 16 }, 6.0f, Theme::U32(Theme::Good, 0.25f * pulse));
        dl->AddCircleFilled({ fMin.x + 16, fMin.y + 16 }, 3.5f, Theme::U32(Theme::Good));
        dl->AddText(Theme::fonts.bold, Theme::SizeBody, { fMin.x + 28, fMin.y + 7 }, Theme::U32(Theme::Text), "Active");

        char buf[64];
        snprintf(buf, sizeof(buf), "Press %s to hide", Widgets::KeyName(cfg.menu.toggleKey));
        dl->AddText(Theme::fonts.regular, Theme::SizeSmall, { fMin.x + 28, fMin.y + 25 }, Theme::U32(Theme::TextDim), buf);

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    void DrawContent(ImVec2 winSize)
    {
        const TabInfo& tab = kTabs[g_tab];

        ImGui::SetCursorPos({ kSidebarW, 0 });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 24, 20 });
        ImGui::BeginChild("##content", { winSize.x - kSidebarW, winSize.y }, ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

        // Header
        const ImVec2 headerPos = ImGui::GetCursorScreenPos();
        Widgets::Header(tab.name, tab.subtitle);

        // FPS pill (top-right)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0f FPS", ImGui::GetIO().Framerate);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            const float  right = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 24.0f;
            const ImVec2 pMin(right - ts.x - 26.0f, headerPos.y + 6.0f);
            const ImVec2 pMax(right, pMin.y + 26.0f);
            dl->AddRectFilled(pMin, pMax, Theme::U32(Theme::Card), 13.0f);
            dl->AddRect(pMin, pMax, Theme::U32(Theme::CardBorder), 13.0f);
            dl->AddCircleFilled({ pMin.x + 12, pMin.y + 13 }, 3.0f, Theme::U32(Theme::Accent()));
            dl->AddText({ pMin.x + 20, pMin.y + 13 - ts.y * 0.5f - 1 }, Theme::U32(Theme::Text), buf);
        }

        // Accent underline
        {
            ImDrawList*  dl = ImGui::GetWindowDrawList();
            const ImVec2 p  = ImGui::GetCursorScreenPos() + ImVec2(0, 6);
            const float  w  = ImGui::GetContentRegionAvail().x;
            dl->AddRectFilledMultiColor(p, p + ImVec2(w, 1),
                Theme::U32(Theme::Accent(), 0.8f), Theme::U32(Theme::Accent(), 0.0f),
                Theme::U32(Theme::Accent(), 0.0f), Theme::U32(Theme::Accent(), 0.8f));
            ImGui::Dummy({ 0, 14 });
        }

        // Page with fade + slide-in on tab switch
        const float t = Widgets::Animate(ImHashStr("##page_anim"), 1.0f, 10.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - t) * 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * t);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 14, 14 });
        ImGui::BeginChild("##page", { 0, 0 }, 0, ImGuiWindowFlags_NoBackground);
        tab.render();
        ImGui::Dummy({ 0, 4 });
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    void DrawToast(ImVec2 winPos, ImVec2 winSize)
    {
        const float age = (float)(ImGui::GetTime() - g_toastTime);
        if (age > 2.6f)
            return;

        const float  a  = ImSaturate(age * 6.0f) * ImSaturate((2.6f - age) * 4.0f);
        ImDrawList*  dl = ImGui::GetForegroundDrawList();   // above the child windows
        const ImVec2 ts = ImGui::CalcTextSize(g_toast);
        const ImVec2 sz(ts.x + 44.0f, 34.0f);
        const ImVec2 p(winPos.x + kSidebarW + (winSize.x - kSidebarW - sz.x) * 0.5f, winPos.y + winSize.y - sz.y - 18.0f + (1.0f - a) * 10.0f);

        dl->AddRectFilled(p, p + sz, Theme::U32({ 0.10f, 0.105f, 0.13f, 1.0f }, a), 17.0f);
        dl->AddRect(p, p + sz, Theme::U32(Theme::Accent(), 0.6f * a), 17.0f);
        dl->AddCircleFilled(p + ImVec2(18, 17), 4.0f, Theme::U32(Theme::Accent(), a));
        dl->AddText(p + ImVec2(30, 17 - ts.y * 0.5f - 1), Theme::U32(Theme::Text, a), g_toast);
    }

    void DrawOverlay()
    {
        ImDrawList*  dl     = ImGui::GetBackgroundDrawList();
        const ImVec2 screen = ImGui::GetIO().DisplaySize;

        if (cfg.misc.watermark)
        {
            char buf[64];
            if (cfg.misc.fpsCounter)
                snprintf(buf, sizeof(buf), "BLAZING  |  %.0f fps", ImGui::GetIO().Framerate);
            else
                snprintf(buf, sizeof(buf), "BLAZING");

            const ImVec2 ts = Theme::fonts.bold->CalcTextSizeA(Theme::SizeBody, FLT_MAX, 0.0f, buf);
            const ImVec2 p(12, 12), sz(ts.x + 30, 28);
            dl->AddRectFilled(p, p + sz, Theme::U32(Theme::Bg, 0.85f), 8.0f);
            dl->AddRectFilled(p + ImVec2(0, 4), p + ImVec2(3, sz.y - 4), Theme::U32(Theme::Accent()), 2.0f);
            dl->AddText(Theme::fonts.bold, Theme::SizeBody, p + ImVec2(16, (sz.y - ts.y) * 0.5f), Theme::U32(Theme::Text), buf);
        }

        if (cfg.misc.crosshair)
        {
            const auto&  c   = cfg.misc.colCrosshair;
            const ImU32  col = ImGui::ColorConvertFloat4ToU32({ c.r, c.g, c.b, c.a });
            const ImU32  out = IM_COL32(0, 0, 0, (int)(c.a * 160));
            const ImVec2 m   = screen * 0.5f;
            const float  s   = cfg.misc.crosshairSize;

            switch (cfg.misc.crosshairStyle)
            {
            case 0: // Cross
                dl->AddRectFilled({ m.x - s - 1, m.y - 2 }, { m.x + s + 2, m.y + 2 }, out);
                dl->AddRectFilled({ m.x - 2, m.y - s - 1 }, { m.x + 2, m.y + s + 2 }, out);
                dl->AddLine({ m.x - s, m.y }, { m.x + s + 1, m.y }, col, 2.0f);
                dl->AddLine({ m.x, m.y - s }, { m.x, m.y + s + 1 }, col, 2.0f);
                break;
            case 1: // Dot
                dl->AddCircleFilled(m, s * 0.5f + 1.0f, out);
                dl->AddCircleFilled(m, s * 0.5f, col);
                break;
            case 2: // Circle
                dl->AddCircle(m, s, out, 0, 3.5f);
                dl->AddCircle(m, s, col, 0, 1.5f);
                break;
            }
        }
    }
}

namespace Menu
{
    void Init()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;       // don't write imgui.ini into the game folder
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        Theme::LoadFonts();
        Theme::Apply();
    }

    void Notify(const char* text)
    {
        snprintf(g_toast, sizeof(g_toast), "%s", text);
        g_toastTime = ImGui::GetTime();
    }

    void BeginColumn(const char* id, int index)
    {
        constexpr float spacing = 14.0f;
        if (index > 0)
            ImGui::SameLine(0.0f, spacing);

        const float w = (index == 0) ? (ImGui::GetContentRegionAvail().x - spacing) * 0.5f
                                     : ImGui::GetContentRegionAvail().x;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0, 0, 0, 0 });
        ImGui::BeginChild(id, { w, 0 }, ImGuiChildFlags_AutoResizeY,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        ImGui::PopStyleColor();
    }

    void EndColumn()
    {
        ImGui::EndChild();
    }

    void RenderEspTab()
    {
        static int section = 0;
        const char* const sections[] = { "Players", "Items" };
        Widgets::Segmented("##esp_section", &section, sections, IM_ARRAYSIZE(sections));

        if (section == 0)
            RenderCharacterSection();
        else
            RenderItemSection();
    }

    void Render()
    {
        Theme::Apply();     // cheap; keeps accent-dependent colors in sync
        DrawOverlay();

        const float alpha = Widgets::Animate(ImHashStr("##menu_alpha"), open ? 1.0f : 0.0f, 14.0f);
        ImGui::GetIO().MouseDrawCursor = open;
        if (alpha <= 0.0f)
            return;

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(kMenuSize, ImGuiCond_Always);
        ImGui::SetNextWindowPos(display * 0.5f, ImGuiCond_FirstUseEver, { 0.5f, 0.5f });

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::Begin("##blazing_menu", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse |
            (open ? 0 : ImGuiWindowFlags_NoInputs));

        const ImVec2 pos  = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        ImDrawList*  dl   = ImGui::GetWindowDrawList();

        // Sidebar background + divider
        dl->AddRectFilled(pos, pos + ImVec2(kSidebarW, size.y), Theme::U32(Theme::Sidebar), ImGui::GetStyle().WindowRounding, ImDrawFlags_RoundCornersLeft);
        dl->AddLine(pos + ImVec2(kSidebarW, 0), pos + ImVec2(kSidebarW, size.y), Theme::U32(Theme::CardBorder));

        DrawSidebar(pos, size);
        DrawContent(size);
        DrawToast(pos, size);

        ImGui::End();
        ImGui::PopStyleVar();
    }
}

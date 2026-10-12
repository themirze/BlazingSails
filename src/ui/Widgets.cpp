#include "ui/Widgets.h"
#include "ui/Theme.h"

#include <imgui_internal.h>
#include <unordered_map>
#include <cstdio>
#include <cmath>

namespace
{
    std::unordered_map<ImGuiID, float> g_anim;

    ImGuiID SubID(ImGuiID id, int n) { return ImHashData(&n, sizeof(n), id); }

    ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) { return ImLerp(a, b, t); }
    ImU32  Col(const ImVec4& c)                            { return Theme::U32(c, 1.0f); }
    ImU32  Col(const ImVec4& c, float a)                   { return Theme::U32(c, a); }

    ImFont* Bold()    { return Theme::fonts.bold; }
    ImFont* Regular() { return Theme::fonts.regular; }

    // Label text without the "##id" suffix
    void DrawLabel(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* label)
    {
        dl->AddText(pos, col, label, ImGui::FindRenderedTextEnd(label));
    }

    bool ColorSwatch(const char* id, Color4* c)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        const ImGuiID gid = w->GetID(id);
        const ImVec2 pos  = w->DC.CursorPos;
        const float  s    = 18.0f;
        const ImRect bb(pos, pos + ImVec2(s, 22.0f));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, gid))
            return false;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, gid, &hovered, &held);
        const float th = Widgets::Animate(SubID(gid, 1), hovered ? 1.0f : 0.0f);

        const ImRect sw(ImVec2(bb.Min.x, bb.GetCenter().y - s * 0.5f), ImVec2(bb.Max.x, bb.GetCenter().y + s * 0.5f));
        ImDrawList* dl = w->DrawList;
        dl->AddRectFilled(sw.Min - ImVec2(2, 2), sw.Max + ImVec2(2, 2), Col(Theme::Accent(), 0.6f * th), 7.0f);
        dl->AddRectFilled(sw.Min, sw.Max, Col({ c->r, c->g, c->b, 1.0f }), 5.0f);
        dl->AddRect(sw.Min, sw.Max, IM_COL32(255, 255, 255, 40), 5.0f);

        ImGui::PushID(id);
        if (pressed)
            ImGui::OpenPopup("##picker");

        bool changed = false;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 10, 10 });
        if (ImGui::BeginPopup("##picker"))
        {
            ImGui::SetNextItemWidth(190.0f);
            changed = ImGui::ColorPicker4("##pick", &c->r,
                ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview |
                ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_PickerHueBar |
                ImGuiColorEditFlags_DisplayHex);
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
        return changed;
    }
}

namespace Widgets
{
    float Animate(ImGuiID id, float target, float speed)
    {
        auto [it, inserted] = g_anim.try_emplace(id, target);
        float& v = it->second;
        if (!cfg.menu.animations)
            return v = target;

        v = ImLerp(v, target, ImSaturate(ImGui::GetIO().DeltaTime * speed));
        if (std::fabs(v - target) < 0.002f)
            v = target;
        return v;
    }

    void AnimSet(ImGuiID id, float value)
    {
        g_anim[id] = value;
    }

    // ------------------------------------------------------------------ Layout

    bool SidebarTab(const char* label, const char* hint, int icon, bool selected)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id  = w->GetID(label);
        const ImVec2  pos = w->DC.CursorPos;
        const ImRect  bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, 48.0f));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        const float th = Animate(SubID(id, 1), hovered ? 1.0f : 0.0f);
        const float ts = Animate(SubID(id, 2), selected ? 1.0f : 0.0f, 10.0f);

        ImDrawList* dl = w->DrawList;

        // Background
        dl->AddRectFilled(bb.Min, bb.Max, Col(Theme::FrameHover, 0.45f * th * (1.0f - ts)), Theme::Rounding);
        dl->AddRectFilled(bb.Min, bb.Max, Col(Theme::Accent(), 0.10f * ts), Theme::Rounding);

        // Selection indicator on the sidebar edge
        if (ts > 0.0f)
        {
            const float h  = 22.0f * ts;
            const float cy = bb.GetCenter().y;
            dl->AddRectFilled({ w->Pos.x, cy - h * 0.5f }, { w->Pos.x + 3.0f, cy + h * 0.5f }, Col(Theme::Accent(), ts), 2.0f, ImDrawFlags_RoundCornersRight);
        }

        // Icon box
        const ImRect ib(bb.Min + ImVec2(8, 9), bb.Min + ImVec2(38, 39));
        dl->AddRectFilled(ib.Min, ib.Max, Col(Mix(Theme::Frame, Theme::Accent(), ts)), 8.0f);
        const ImVec4 iconCol = Mix(Mix(Theme::TextDim, Theme::Text, th), ImVec4(1, 1, 1, 1), ts);
        DrawIcon(dl, icon, ib.GetCenter(), 16.0f, Col(iconCol));

        // Texts
        const float  x       = ib.Max.x + 12.0f;
        const ImVec4 textCol = Mix(Mix(Theme::TextDim, Theme::Text, th), Theme::Text, ts);
        dl->AddText(Bold(), Theme::SizeBody, { x, bb.Min.y + 7.0f }, Col(textCol), label, ImGui::FindRenderedTextEnd(label));
        dl->AddText(Regular(), Theme::SizeSmall, { x, bb.Min.y + 25.0f }, Col(Mix(Theme::TextFaint, Theme::TextDim, ts)), hint);

        return pressed;
    }

    bool Segmented(const char* strId, int* current, const char* const items[], int count)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id  = w->GetID(strId);
        const ImVec2  pos = w->DC.CursorPos;
        const ImRect  bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, 34.0f));
        ImGui::ItemSize(bb);

        ImDrawList* dl = w->DrawList;
        dl->AddRectFilled(bb.Min, bb.Max, Col(Theme::Card), Theme::Rounding);
        dl->AddRect(bb.Min, bb.Max, Col(Theme::CardBorder), Theme::Rounding);

        const float pad  = 4.0f;
        const float segW = (bb.GetWidth() - pad * 2.0f) / count;
        const float a    = Animate(id, (float)*current, 14.0f);

        // Sliding highlight
        const ImVec2 hMin(bb.Min.x + pad + a * segW, bb.Min.y + pad);
        dl->AddRectFilled(hMin, hMin + ImVec2(segW, bb.GetHeight() - pad * 2.0f), Col(Theme::Accent()), 6.0f);

        bool changed = false;
        for (int i = 0; i < count; i++)
        {
            const ImGuiID sid = SubID(id, i + 10);
            const ImRect  sb({ bb.Min.x + pad + i * segW, bb.Min.y }, { bb.Min.x + pad + (i + 1) * segW, bb.Max.y });
            if (!ImGui::ItemAdd(sb, sid))
                continue;

            bool hovered, held;
            if (ImGui::ButtonBehavior(sb, sid, &hovered, &held) && *current != i)
            {
                *current = i;
                changed  = true;
            }

            const float  sel = 1.0f - ImSaturate(std::fabs(a - (float)i));
            const ImVec4 tc  = Mix(hovered ? Theme::Text : Theme::TextDim, ImVec4(1, 1, 1, 1), sel);
            const ImVec2 ts  = Bold()->CalcTextSizeA(Theme::SizeBody, FLT_MAX, 0.0f, items[i]);
            dl->AddText(Bold(), Theme::SizeBody, sb.GetCenter() - ts * 0.5f, Col(tc), items[i]);
        }
        return changed;
    }

    void BeginCard(const char* title, float height)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 16, 14 });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8, 9 });

        const ImGuiChildFlags flags = ImGuiChildFlags_Borders | (height <= 0.0f ? ImGuiChildFlags_AutoResizeY : 0);
        ImGui::BeginChild(title, { 0, height }, flags, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImGuiWindow* w  = ImGui::GetCurrentWindow();
        ImDrawList*  dl = w->DrawList;
        const ImVec2 p  = ImGui::GetCursorScreenPos();

        // Title with accent dot
        dl->AddCircleFilled(p + ImVec2(3.0f, 9.5f), 3.0f, Col(Theme::Accent()));
        dl->AddText(Bold(), Theme::SizeBody, p + ImVec2(14.0f, 0.0f), Col(Theme::Text), title, ImGui::FindRenderedTextEnd(title));
        ImGui::Dummy({ 0, 18.0f });

        // Divider spanning the full card width
        const float y = ImGui::GetCursorScreenPos().y;
        dl->AddLine({ w->Pos.x + 1.0f, y }, { w->Pos.x + w->Size.x - 1.0f, y }, Col(Theme::CardBorder));
        ImGui::Dummy({ 0, 2.0f });
    }

    void EndCard()
    {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }

    void Header(const char* title, const char* subtitle)
    {
        ImGui::PushFont(Bold(), Theme::SizeTitle);
        ImGui::TextUnformatted(title);
        ImGui::PopFont();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
        ImGui::PushFont(Regular(), Theme::SizeBody - 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::TextUnformatted(subtitle);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    // ------------------------------------------------------------------ Rows

    bool Toggle(const char* label, bool* v, Color4* color)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id     = w->GetID(label);
        const float   extraW = color ? 26.0f : 0.0f;
        const ImVec2  pos    = w->DC.CursorPos;
        const ImRect  bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x - extraW, 22.0f));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed)
        {
            *v = !*v;
            ImGui::MarkItemEdited(id);
        }

        const float t  = Animate(id, *v ? 1.0f : 0.0f, 14.0f);
        const float th = Animate(SubID(id, 1), hovered ? 1.0f : 0.0f);

        ImDrawList* dl = w->DrawList;

        // Label
        const ImVec4 tc = Mix(Theme::TextDim, Theme::Text, ImMax(t, th));
        DrawLabel(dl, { bb.Min.x, bb.GetCenter().y - ImGui::GetFontSize() * 0.5f - 1.0f }, Col(tc), label);

        // Switch
        const ImVec2 size(34.0f, 18.0f);
        const ImRect sw({ bb.Max.x - size.x, bb.GetCenter().y - size.y * 0.5f }, { bb.Max.x, bb.GetCenter().y + size.y * 0.5f });
        const ImVec4 off = Mix(Theme::Frame, Theme::FrameHover, th);
        dl->AddRectFilled(sw.Min, sw.Max, Col(Mix(off, Theme::Accent(), t)), size.y * 0.5f);

        const float  r    = 6.0f + (held ? 1.0f : 0.0f);
        const ImVec2 knob(ImLerp(sw.Min.x + 9.0f, sw.Max.x - 9.0f, t), sw.GetCenter().y);
        dl->AddCircleFilled(knob, r + 2.0f, Col(Theme::Accent(), 0.25f * t));
        dl->AddCircleFilled(knob, r, Col(Mix(Theme::TextDim, ImVec4(1, 1, 1, 1), t)));

        bool changed = pressed;
        if (color)
        {
            ImGui::SameLine(0.0f, 8.0f);
            changed |= ColorSwatch("##color", color);
        }
        return changed;
    }

    bool Slider(const char* label, float* v, float min, float max, const char* fmt)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id  = w->GetID(label);
        const ImVec2  pos = w->DC.CursorPos;
        const float   wid = ImGui::GetContentRegionAvail().x;
        const ImRect  bb(pos, pos + ImVec2(wid, 38.0f));
        const ImRect  track(pos + ImVec2(7.0f, 26.0f), pos + ImVec2(wid - 7.0f, 31.0f));
        const ImRect  hit(track.Min - ImVec2(7.0f, 9.0f), track.Max + ImVec2(7.0f, 7.0f));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        ImGui::ButtonBehavior(hit, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

        bool changed = false;
        if (held)
        {
            const float t  = ImSaturate((ImGui::GetIO().MousePos.x - track.Min.x) / track.GetWidth());
            const float nv = min + (max - min) * t;
            if (nv != *v)
            {
                *v      = nv;
                changed = true;
                ImGui::MarkItemEdited(id);
            }
        }

        const float t  = Animate(id, ImSaturate((*v - min) / (max - min)), 22.0f);
        const float th = Animate(SubID(id, 1), (hovered || held) ? 1.0f : 0.0f);

        ImDrawList* dl = w->DrawList;

        // Label + value
        const float ty = pos.y + 1.0f;
        DrawLabel(dl, { pos.x, ty }, Col(Mix(Theme::TextDim, Theme::Text, th)), label);

        char buf[32];
        snprintf(buf, sizeof(buf), fmt, *v);
        const ImVec2 vs = ImGui::CalcTextSize(buf);
        const ImRect pill({ bb.Max.x - vs.x - 12.0f, ty - 2.0f }, { bb.Max.x, ty + vs.y + 1.0f });
        dl->AddRectFilled(pill.Min, pill.Max, Col(Theme::Frame), 5.0f);
        dl->AddText({ pill.Min.x + 6.0f, ty - 0.5f }, Col(Mix(Theme::Text, Theme::Accent(), th)), buf);

        // Track
        const float fillX = ImLerp(track.Min.x, track.Max.x, t);
        dl->AddRectFilled(track.Min, track.Max, Col(Theme::Frame), 3.0f);
        dl->AddRectFilled(track.Min, { fillX, track.Max.y }, Col(Theme::Accent()), 3.0f);

        // Knob
        const ImVec2 knob(fillX, track.GetCenter().y);
        dl->AddCircleFilled(knob, 7.0f + 3.0f * th, Col(Theme::Accent(), 0.20f * th));
        dl->AddCircleFilled(knob, 6.5f, Col(Theme::Accent()));
        dl->AddCircleFilled(knob, 3.0f, IM_COL32_WHITE);

        return changed;
    }

    bool Combo(const char* label, int* current, const char* const items[], int count)
    {
        ImGui::PushID(label);

        const float width  = ImGui::GetContentRegionAvail().x;
        const float comboW = ImMax(120.0f, width * 0.5f);
        const float startX = ImGui::GetCursorPosX();

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
        ImGui::PopStyleColor();
        ImGui::SameLine(startX + width - comboW);

        bool changed = false;
        ImGui::SetNextItemWidth(comboW);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6, 6 });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4, 4 });
        if (ImGui::BeginCombo("##combo", items[*current]))
        {
            for (int i = 0; i < count; i++)
            {
                const bool selected = (i == *current);
                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Accent());
                if (ImGui::Selectable(items[i], selected, 0, { 0, 24 }))
                {
                    *current = i;
                    changed  = true;
                }
                if (selected)
                {
                    ImGui::PopStyleColor();
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleVar(2);

        ImGui::PopID();
        return changed;
    }

    bool ColorRow(const char* label, Color4* color)
    {
        ImGui::PushID(label);
        const float startX = ImGui::GetCursorPosX();
        const float width  = ImGui::GetContentRegionAvail().x;

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));
        ImGui::PopStyleColor();

        ImGui::SameLine(startX + width - 18.0f);
        const bool changed = ColorSwatch("##swatch", color);
        ImGui::PopID();
        return changed;
    }

    const char* KeyName(int vk)
    {
        switch (vk)
        {
        case 0:             return "None";
        case VK_LBUTTON:    return "Mouse 1";
        case VK_RBUTTON:    return "Mouse 2";
        case VK_MBUTTON:    return "Mouse 3";
        case VK_XBUTTON1:   return "Mouse 4";
        case VK_XBUTTON2:   return "Mouse 5";
        }

        static char buf[32];
        UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
        switch (vk)
        {
        case VK_INSERT: case VK_DELETE: case VK_HOME:  case VK_END:
        case VK_PRIOR:  case VK_NEXT:   case VK_LEFT:  case VK_RIGHT:
        case VK_UP:     case VK_DOWN:   case VK_RCONTROL: case VK_RMENU:
        case VK_DIVIDE: case VK_NUMLOCK:
            sc |= 0x100;
            break;
        }
        if (!GetKeyNameTextA((LONG)(sc << 16), buf, sizeof(buf)))
            snprintf(buf, sizeof(buf), "0x%02X", vk);
        return buf;
    }

    bool Keybind(const char* label, int* vk)
    {
        static ImGuiID listening = 0;
        static bool    prevDown[256];

        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id     = w->GetID(label);
        const float   width  = ImGui::GetContentRegionAvail().x;
        const ImVec2  pos    = w->DC.CursorPos;
        const ImRect  row(pos, pos + ImVec2(width, 26.0f));
        const float   btnW   = 96.0f;
        const ImRect  btn({ row.Max.x - btnW, row.Min.y }, row.Max);

        ImGui::ItemSize(row);
        if (!ImGui::ItemAdd(btn, id))
            return false;

        bool hovered, held;
        if (ImGui::ButtonBehavior(btn, id, &hovered, &held) && listening != id)
        {
            listening = id;
            for (int k = 0; k < 256; k++)
                prevDown[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
        }

        bool changed = false;
        const bool active = (listening == id);
        if (active)
        {
            for (int k = 1; k < 256; k++)
            {
                const bool down = (GetAsyncKeyState(k) & 0x8000) != 0;
                const bool press = down && !prevDown[k];
                prevDown[k] = down;
                if (!press)
                    continue;

                if (k == VK_ESCAPE)
                    listening = 0;
                else
                {
                    *vk       = (k == VK_BACK || k == VK_DELETE) ? 0 : k;
                    listening = 0;
                    changed   = true;
                }
                break;
            }
        }

        const float th = Animate(SubID(id, 1), hovered ? 1.0f : 0.0f);
        const float ta = Animate(SubID(id, 2), active ? 1.0f : 0.0f);

        ImDrawList* dl = w->DrawList;
        DrawLabel(dl, { row.Min.x, row.GetCenter().y - ImGui::GetFontSize() * 0.5f - 1.0f }, Col(Mix(Theme::TextDim, Theme::Text, th)), label);

        dl->AddRectFilled(btn.Min, btn.Max, Col(Mix(Theme::Frame, Theme::FrameHover, th)), 6.0f);
        dl->AddRect(btn.Min, btn.Max, Col(Theme::Accent(), ta), 6.0f, 0, 1.5f);

        const char*  text = active ? "Press a key" : KeyName(*vk);
        const ImVec2 ts   = ImGui::CalcTextSize(text);
        const ImVec4 tc   = active ? Theme::Accent(0.6f + 0.4f * std::fabs(std::sin((float)ImGui::GetTime() * 4.0f))) : Theme::Text;
        dl->AddText(btn.GetCenter() - ts * 0.5f - ImVec2(0, 1), Col(tc), text);

        if (hovered && !active)
            ImGui::SetTooltip("Click, then press a key.\nEsc cancels, Backspace clears.");

        return changed;
    }

    bool Button(const char* label, float width, bool accent)
    {
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        if (w->SkipItems)
            return false;

        const ImGuiID id  = w->GetID(label);
        const ImVec2  pos = w->DC.CursorPos;
        if (width <= 0.0f)
            width = ImGui::GetContentRegionAvail().x;
        const ImRect bb(pos, pos + ImVec2(width, 32.0f));

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        const float th = Animate(SubID(id, 1), hovered ? 1.0f : 0.0f);

        ImDrawList* dl = w->DrawList;
        ImVec4 bg = accent ? Theme::Accent(0.85f + 0.15f * th) : Mix(Theme::Frame, Theme::FrameHover, th);
        if (held)
            bg = Mix(bg, ImVec4(0, 0, 0, bg.w), 0.15f);
        dl->AddRectFilled(bb.Min, bb.Max, Col(bg), 7.0f);
        if (!accent)
            dl->AddRect(bb.Min, bb.Max, Col(Theme::Accent(), 0.6f * th), 7.0f);

        const ImVec2 ts = Bold()->CalcTextSizeA(Theme::SizeBody, FLT_MAX, 0.0f, label, ImGui::FindRenderedTextEnd(label));
        dl->AddText(Bold(), Theme::SizeBody, bb.GetCenter() - ts * 0.5f, accent ? IM_COL32_WHITE : Col(Theme::Text), label, ImGui::FindRenderedTextEnd(label));
        return pressed;
    }

    void InfoRow(const char* label, const char* value, const ImVec4* valueColor)
    {
        const float startX = ImGui::GetCursorPosX();
        const float width  = ImGui::GetContentRegionAvail().x;

        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::SameLine(startX + width - ImGui::CalcTextSize(value).x);
        ImGui::PushStyleColor(ImGuiCol_Text, valueColor ? *valueColor : Theme::Text);
        ImGui::TextUnformatted(value);
        ImGui::PopStyleColor();
    }

    void Hint(const char* text)
    {
        ImGui::PushFont(nullptr, Theme::SizeSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    // ------------------------------------------------------------------ Icons

    void DrawIcon(ImDrawList* dl, int icon, ImVec2 c, float size, ImU32 col)
    {
        const float r = size * 0.5f;
        const float t = 1.6f;

        switch (icon)
        {
        case Icon_Info:
            dl->AddCircle(c, r, col, 0, t);
            dl->AddCircleFilled({ c.x, c.y - r * 0.45f }, 1.3f, col);
            dl->AddLine({ c.x, c.y - r * 0.05f }, { c.x, c.y + r * 0.5f }, col, t);
            break;

        case Icon_Eye:
        {
            const ImVec2 l(c.x - r, c.y), rr(c.x + r, c.y);
            dl->AddBezierQuadratic(l, { c.x, c.y - r * 1.15f }, rr, col, t);
            dl->AddBezierQuadratic(l, { c.x, c.y + r * 1.15f }, rr, col, t);
            dl->AddCircleFilled(c, r * 0.32f, col);
            break;
        }

        case Icon_Crosshair:
            dl->AddCircle(c, r * 0.62f, col, 0, t);
            dl->AddLine({ c.x - r, c.y }, { c.x - r * 0.3f, c.y }, col, t);
            dl->AddLine({ c.x + r * 0.3f, c.y }, { c.x + r, c.y }, col, t);
            dl->AddLine({ c.x, c.y - r }, { c.x, c.y - r * 0.3f }, col, t);
            dl->AddLine({ c.x, c.y + r * 0.3f }, { c.x, c.y + r }, col, t);
            dl->AddCircleFilled(c, 1.3f, col);
            break;

        case Icon_Gear:
            dl->AddCircle(c, r * 0.58f, col, 0, t);
            dl->AddCircle(c, r * 0.22f, col, 0, t);
            for (int i = 0; i < 8; i++)
            {
                const float a = i * IM_PI / 4.0f;
                const ImVec2 d(std::cos(a), std::sin(a));
                dl->AddLine(c + d * (r * 0.62f), c + d * r, col, 2.6f);
            }
            break;
        }
    }
}

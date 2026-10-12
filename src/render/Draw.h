#pragma once
#include <imgui.h>
#include "config/Config.h"

namespace Draw {
    inline ImU32 U32(const Color4& c, float alphaMul = 1.0f) {
        return IM_COL32(
            (int)(c.r * 255.0f), (int)(c.g * 255.0f),
            (int)(c.b * 255.0f), (int)(c.a * 255.0f * alphaMul));
    }

    inline void Box(ImDrawList* dl, ImVec2 tl, ImVec2 br, ImU32 col, int style, float th) {
        if (style == 0) {
            dl->AddRect(tl, br, col, 0.0f, 0, th);
        }
        else if (style == 1) {
            float cw = (br.x - tl.x) * 0.30f;
            float ch = (br.y - tl.y) * 0.18f;
            dl->AddLine(tl, { tl.x + cw, tl.y }, col, th);
            dl->AddLine(tl, { tl.x, tl.y + ch }, col, th);
            dl->AddLine({ br.x, tl.y }, { br.x - cw, tl.y }, col, th);
            dl->AddLine({ br.x, tl.y }, { br.x, tl.y + ch }, col, th);
            dl->AddLine({ tl.x, br.y }, { tl.x + cw, br.y }, col, th);
            dl->AddLine({ tl.x, br.y }, { tl.x, br.y - ch }, col, th);
            dl->AddLine(br, { br.x - cw, br.y }, col, th);
            dl->AddLine(br, { br.x, br.y - ch }, col, th);
        }
        else if (style == 2) {
            dl->AddRectFilled(tl, br, (col & 0x00FFFFFF) | ((ImU32)(0.25f * 255.0f) << 24));
            dl->AddRect(tl, br, col, 0.0f, 0, th);
        }
    }

    inline void TextShadow(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* txt) {
        if (!txt || !*txt) return;
        dl->AddText({ pos.x + 1, pos.y + 1 }, IM_COL32(0, 0, 0, 200), txt);
        dl->AddText(pos, col, txt);
    }

    inline void HealthBar(ImDrawList* dl, ImVec2 tl, float w, float h, float ratio) {
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        ImU32 col = IM_COL32((int)(255 * (1 - ratio)), (int)(255 * ratio), 0, 255);
        dl->AddRectFilled(tl, { tl.x + w, tl.y + h }, IM_COL32(0, 0, 0, 180));
        dl->AddRectFilled(tl, { tl.x + w * ratio, tl.y + h }, col);
        dl->AddRect(tl, { tl.x + w, tl.y + h }, IM_COL32(255, 255, 255, 180));
    }

    inline void Snapline(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col) {
        dl->AddLine(from, to, col, 1.5f);
    }
}
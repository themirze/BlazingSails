#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "sdk/Actors_World.h"

#include <imgui_internal.h>
#include <cmath>
#include <utility>

namespace
{
    ImU32 ToU32(const Color4& c) { return ImGui::ColorConvertFloat4ToU32({ c.r, c.g, c.b, c.a * ImGui::GetStyle().Alpha }); }

    // Live preview of the current player ESP settings
    void DrawPreview()
    {
        ImDrawList*  dl    = ImGui::GetWindowDrawList();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 p     = ImGui::GetCursorScreenPos();
        const ImVec2 size(avail.x, 230.0f);
        ImGui::Dummy(size);

        // Subtle grid backdrop
        dl->AddRectFilled(p, p + size, Theme::U32(Theme::Bg), 6.0f);
        for (float x = p.x + 16.0f; x < p.x + size.x; x += 16.0f)
            dl->AddLine({ x, p.y + 1 }, { x, p.y + size.y - 1 }, Theme::U32(Theme::CardBorder, 0.35f));
        for (float y = p.y + 16.0f; y < p.y + size.y; y += 16.0f)
            dl->AddLine({ p.x + 1, y }, { p.x + size.x - 1, y }, Theme::U32(Theme::CardBorder, 0.35f));

        if (!cfg.esp.enabled)
        {
            const char*  msg = "ESP is disabled";
            const ImVec2 ts  = ImGui::CalcTextSize(msg);
            dl->AddText(p + (size - ts) * 0.5f, Theme::U32(Theme::TextDim), msg);
            return;
        }

        // Figure joints
        const ImVec2 c(p.x + size.x * 0.5f, p.y + size.y * 0.5f + 6.0f);
        const float  bob = std::sin((float)ImGui::GetTime() * 2.0f) * 1.5f;
        const ImVec2 head   (c.x,       c.y - 62 + bob);
        const ImVec2 neck   (c.x,       c.y - 50 + bob);
        const ImVec2 pelvis (c.x,       c.y + 4);
        const ImVec2 lHand  (c.x - 26,  c.y - 4);
        const ImVec2 rHand  (c.x + 26,  c.y - 4);
        const ImVec2 lElbow (c.x - 20,  c.y - 28 + bob);
        const ImVec2 rElbow (c.x + 20,  c.y - 28 + bob);
        const ImVec2 lShould(c.x - 14,  c.y - 46 + bob);
        const ImVec2 rShould(c.x + 14,  c.y - 46 + bob);
        const ImVec2 lKnee  (c.x - 12,  c.y + 34);
        const ImVec2 rKnee  (c.x + 12,  c.y + 34);
        const ImVec2 lFoot  (c.x - 15,  c.y + 66);
        const ImVec2 rFoot  (c.x + 15,  c.y + 66);

        // Silhouette
        const ImU32 body = Theme::U32(Theme::TextFaint, 0.55f);
        dl->AddCircleFilled(head, 9.0f, body);
        dl->AddRectFilled({ c.x - 15, neck.y + 2 }, { c.x + 15, pelvis.y }, body, 6.0f);
        for (auto [a, b] : { std::pair{ lShould, lElbow }, { lElbow, lHand }, { rShould, rElbow }, { rElbow, rHand },
                             { pelvis, lKnee }, { lKnee, lFoot }, { pelvis, rKnee }, { rKnee, rFoot } })
            dl->AddLine(a, b, body, 7.0f);

        const ImU32  col = ToU32(cfg.esp.colVisible);
        const ImU32  blk = IM_COL32(0, 0, 0, (int)(170 * ImGui::GetStyle().Alpha));
        const ImVec2 bMin(c.x - 36, head.y - 14);
        const ImVec2 bMax(c.x + 36, lFoot.y + 4);

        if (cfg.esp.skeleton)
        {
            const ImU32 sk = ToU32(cfg.esp.colSkeleton);
            for (auto [a, b] : { std::pair{ head, neck }, { neck, pelvis }, { neck, lShould }, { neck, rShould },
                                 { lShould, lElbow }, { lElbow, lHand }, { rShould, rElbow }, { rElbow, rHand },
                                 { pelvis, lKnee }, { lKnee, lFoot }, { pelvis, rKnee }, { rKnee, rFoot } })
                dl->AddLine(a, b, sk, 1.5f);
        }

        if (cfg.esp.box)
        {
            switch (cfg.esp.boxStyle)
            {
            case 0: // Full
                dl->AddRect(bMin - ImVec2(1, 1), bMax + ImVec2(1, 1), blk, 0, 0, 3.0f);
                dl->AddRect(bMin, bMax, col, 0, 0, 1.5f);
                break;
            case 1: // Corners
            {
                const float lw = (bMax.x - bMin.x) * 0.28f, lh = (bMax.y - bMin.y) * 0.18f;
                const ImVec2 corners[4] = { bMin, { bMax.x, bMin.y }, bMax, { bMin.x, bMax.y } };
                for (int i = 0; i < 4; i++)
                {
                    const float sx = (i == 0 || i == 3) ? 1.0f : -1.0f;
                    const float sy = (i < 2) ? 1.0f : -1.0f;
                    dl->AddLine(corners[i], corners[i] + ImVec2(lw * sx, 0), col, 2.0f);
                    dl->AddLine(corners[i], corners[i] + ImVec2(0, lh * sy), col, 2.0f);
                }
                break;
            }
            case 2: // Filled
                dl->AddRectFilled(bMin, bMax, ImGui::ColorConvertFloat4ToU32({ cfg.esp.colVisible.r, cfg.esp.colVisible.g, cfg.esp.colVisible.b, 0.18f * ImGui::GetStyle().Alpha }));
                dl->AddRect(bMin, bMax, col, 0, 0, 1.5f);
                break;
            }
        }

        if (cfg.esp.health)
        {
            const float hp = 0.72f;
            const ImVec2 hMin(bMin.x - 7, bMin.y), hMax(bMin.x - 4, bMax.y);
            dl->AddRectFilled(hMin - ImVec2(1, 1), hMax + ImVec2(1, 1), blk);
            dl->AddRectFilled({ hMin.x, hMax.y - (hMax.y - hMin.y) * hp }, hMax, Theme::U32(ImLerp(ImVec4(0.95f, 0.30f, 0.30f, 1), Theme::Good, hp)));
        }

        if (cfg.esp.name)
        {
            const char*  name = "Player";
            const ImVec2 ts   = ImGui::CalcTextSize(name);
            dl->AddText({ c.x - ts.x * 0.5f, bMin.y - ts.y - 3 }, Theme::U32(Theme::Text), name);
        }

        if (cfg.esp.distance)
        {
            const char*  buf = "42m";
            const ImVec2 ts  = ImGui::CalcTextSize(buf);
            dl->AddText({ c.x - ts.x * 0.5f, bMax.y + 2 }, Theme::U32(Theme::TextDim), buf);
        }

        if (cfg.esp.snaplines)
            dl->AddLine({ c.x, p.y + size.y - 1 }, { c.x, bMax.y + 18 }, col, 1.0f);
    }
}

namespace Menu
{
    void RenderCharacterSection()
    {
        BeginColumn("##esp_l", 0);
        {
            Widgets::BeginCard("General");
            Widgets::Toggle("Enable player ESP", &cfg.esp.enabled);
            ImGui::BeginDisabled(!cfg.esp.enabled);
            Widgets::Toggle("Visibility check", &cfg.esp.visibleCheck);
            ImGui::SetItemTooltip("Use a different color for players behind walls.");
            Widgets::Toggle("Ignore teammates", &cfg.esp.ignoreTeam);
            Widgets::Slider("Max distance", &cfg.esp.maxDistance, 25.0f, 1000.0f, "%.0f m");
            ImGui::EndDisabled();
            Widgets::EndCard();

            Widgets::BeginCard("Elements");
            ImGui::BeginDisabled(!cfg.esp.enabled);
            Widgets::Toggle("Bounding box", &cfg.esp.box);
            if (cfg.esp.box)
            {
                const char* const styles[] = { "Full", "Corners", "Filled" };
                Widgets::Combo("Box style", &cfg.esp.boxStyle, styles, IM_ARRAYSIZE(styles));
            }
            Widgets::Toggle("Skeleton", &cfg.esp.skeleton, &cfg.esp.colSkeleton);
            Widgets::Toggle("Name", &cfg.esp.name);
            Widgets::Toggle("Health bar", &cfg.esp.health);
            Widgets::Toggle("Distance", &cfg.esp.distance);
            Widgets::Toggle("Snaplines", &cfg.esp.snaplines);
            ImGui::EndDisabled();
            Widgets::EndCard();

            // ★ Gemi (ship) ESP + dunya sayimi.
            Widgets::BeginCard("World / Ships");
            Widgets::Toggle("Enemy ships", &cfg.esp.ships, &cfg.esp.colShips);
            ImGui::BeginDisabled(!cfg.esp.ships);
            Widgets::Slider("Ship distance", &cfg.esp.shipsDistance, 100.0f, 5000.0f, "%.0f m");
            ImGui::EndDisabled();
            if (Widgets::Button("Dump world census (F10)"))
                Actors::RequestWorldCensus();
            Widgets::Hint("Lists every actor class + ships/weapons/anchor. "
                          "Check the log [census] lines for real class names.");
            Widgets::EndCard();
        }
        EndColumn();

        BeginColumn("##esp_r", 1);
        {
            Widgets::BeginCard("Preview");
            DrawPreview();
            Widgets::EndCard();

            Widgets::BeginCard("Colors");
            Widgets::ColorRow("Visible", &cfg.esp.colVisible);
            Widgets::ColorRow("Behind wall", &cfg.esp.colHidden);
            Widgets::ColorRow("Skeleton", &cfg.esp.colSkeleton);
            Widgets::EndCard();
        }
        EndColumn();
    }
}

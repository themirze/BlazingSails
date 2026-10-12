#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "features/esp/ESP_Items.h"
#include "sdk/Actors.h"
#include "sdk/ItemClasses.h"

#include <imgui_internal.h>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstring>

namespace
{
    char g_search[64] = {};

    bool ContainsNoCase(const char* hay, const char* needle)
    {
        if (!needle || !*needle) return true;
        if (!hay) return false;

        for (const char* h = hay; *h; ++h)
        {
            const char* a = h;
            const char* b = needle;
            while (*a && *b &&
                   std::tolower((unsigned char)*a) == std::tolower((unsigned char)*b))
            {
                ++a; ++b;
            }
            if (!*b) return true;
        }
        return false;
    }

    bool ItemVisible(int i)
    {
        return ContainsNoCase(ItemDB::kItems[i].label, g_search)
            || ContainsNoCase(ItemDB::kItems[i].cls,   g_search);
    }

    void SetGroup(int group, bool on)
    {
        for (int i = 0; i < ItemDB::kItemCount; ++i)
            if (ItemDB::kItems[i].group == group)
                cfg.esp.itemOn[i] = on;
    }

    void SetAll(bool on)
    {
        for (int i = 0; i < ItemDB::kItemCount; ++i)
            cfg.esp.itemOn[i] = on;
    }

    // Grup basligi + sagindaki All / None kisayollari
    void GroupHeader(int group)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Accent(0.95f));
        ImGui::TextUnformatted(ItemDB::kGroupNames[group]);
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushID(group);
        if (Widgets::Button("All", 44.0f, true)) SetGroup(group, true);
        ImGui::SameLine(0.0f, 6.0f);
        if (Widgets::Button("None", 50.0f))      SetGroup(group, false);
        ImGui::PopID();
    }
}

namespace Menu
{
    void RenderItemSection()
    {
        // ------------------------------------------------------------------
        //  1) Ana anahtar + genel ayarlar
        // ------------------------------------------------------------------
        Widgets::BeginCard("Item ESP");
        Widgets::Toggle("Enable item ESP", &cfg.esp.items, &cfg.esp.colItems);
        ImGui::BeginDisabled(!cfg.esp.items);
        Widgets::Slider("Max distance", &cfg.esp.itemDistance, 10.0f, 500.0f, "%.0f m");
        Widgets::Toggle("Show distance", &cfg.esp.itemShowDist);
        ImGui::EndDisabled();
        Widgets::Hint("Her item turu tek tek acilir; hepsi varsayilan olarak KAPALIDIR. "
                      "Item ESP, oyuncu ESP'sinden bagimsiz calisir.");
        Widgets::EndCard();

        // ------------------------------------------------------------------
        //  2) Canli durum: "neden hicbir sey cizilmiyor?" sorusunu
        //     oyundan cikmadan yanitlar.
        // ------------------------------------------------------------------
        Widgets::BeginCard("Live");
        {
            char buf[64];

            snprintf(buf, sizeof(buf), "%d / %d", ItemDB::CountEnabled(cfg.esp.itemOn), ItemDB::kItemCount);
            Widgets::InfoRow("Enabled types", buf, &Theme::Warn);

            snprintf(buf, sizeof(buf), "%d", ESP::g_itemDiag.listed);
            Widgets::InfoRow("Items tracked", buf);
            snprintf(buf, sizeof(buf), "%d", ESP::g_itemDiag.drawn);
            Widgets::InfoRow("Drawn", buf, &Theme::Good);
            snprintf(buf, sizeof(buf), "%d", ESP::g_itemDiag.skipOff);
            Widgets::InfoRow("Skipped (type off)", buf);
            snprintf(buf, sizeof(buf), "%d", ESP::g_itemDiag.skipDist);
            Widgets::InfoRow("Skipped (distance)", buf);

            snprintf(buf, sizeof(buf), "%d", Actors::g_itemInfo.count);
            Widgets::InfoRow("Level actors scanned", buf);
            snprintf(buf, sizeof(buf), "%d", Actors::g_itemInfo.matched);
            Widgets::InfoRow("Class map hits", buf);
            snprintf(buf, sizeof(buf), "ok %d / bad %d",
                Actors::g_itemInfo.clsRead - Actors::g_itemInfo.clsBad,
                Actors::g_itemInfo.clsBad);
            Widgets::InfoRow("FName decode", buf);

            Widgets::Hint("'Level actors scanned' buyuk ama 'Class map hits' 0 ise sinif adlari "
                          "bu build ile uyusmuyor; 'FName decode' bad degeri yuksekse FName "
                          "yerlesimi yanlis demektir. Ayrintilar logs\\blazing_*.log icinde.");
        }
        Widgets::EndCard();

        // ------------------------------------------------------------------
        //  3) Turler: arama + grup grup tek tek anahtarlar
        // ------------------------------------------------------------------
        Widgets::BeginCard("Types");
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##item_search", "Search...", g_search, IM_ARRAYSIZE(g_search));

            ImGui::Dummy({ 0, 2 });
            const float half = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
            if (Widgets::Button("Enable all", half, true)) SetAll(true);
            ImGui::SameLine(0.0f, 8.0f);
            if (Widgets::Button("Disable all", half))      SetAll(false);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0, 0, 0, 0 });
            ImGui::BeginChild("##item_list", { 0, 300.0f },
                ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground);

            for (int g = 0; g < ItemDB::Group_Count; ++g)
            {
                bool any = false;
                for (int i = 0; i < ItemDB::kItemCount; ++i)
                    if (ItemDB::kItems[i].group == g && ItemVisible(i)) { any = true; break; }
                if (!any) continue;

                GroupHeader(g);

                for (int i = 0; i < ItemDB::kItemCount; ++i)
                {
                    if (ItemDB::kItems[i].group != g || !ItemVisible(i)) continue;
                    Widgets::Toggle(ItemDB::kItems[i].label, &cfg.esp.itemOn[i]);
                }
                ImGui::Dummy({ 0, 6 });
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        Widgets::EndCard();
    }
}

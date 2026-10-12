#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace Menu
{
    void RenderMiscTab()
    {
        BeginColumn("##misc_l", 0);
        {
            Widgets::BeginCard("Crosshair");
            Widgets::Toggle("Custom crosshair", &cfg.misc.crosshair, &cfg.misc.colCrosshair);
            ImGui::BeginDisabled(!cfg.misc.crosshair);
            const char* const styles[] = { "Cross", "Dot", "Circle" };
            Widgets::Combo("Style", &cfg.misc.crosshairStyle, styles, IM_ARRAYSIZE(styles));
            Widgets::Slider("Size", &cfg.misc.crosshairSize, 2.0f, 20.0f, "%.0f px");
            ImGui::EndDisabled();
            Widgets::EndCard();

            Widgets::BeginCard("Overlay");
            Widgets::Toggle("Watermark", &cfg.misc.watermark);
            ImGui::BeginDisabled(!cfg.misc.watermark);
            Widgets::Toggle("Show FPS", &cfg.misc.fpsCounter);
            ImGui::EndDisabled();
            Widgets::Toggle("Stream proof", &cfg.misc.streamProof);
            ImGui::SetItemTooltip("Hide the overlay from screen capture / streaming software.");
            Widgets::EndCard();
        }
        EndColumn();

        BeginColumn("##misc_r", 1);
        RenderSettingsCards();
        EndColumn();
    }
}

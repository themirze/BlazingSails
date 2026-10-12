#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <cstdio>

namespace Menu
{
    void RenderInfoTab()
    {
        char buf[64];

        BeginColumn("##info_l", 0);
        {
            Widgets::BeginCard("Welcome");
            ImGui::PushFont(Theme::fonts.bold, 17.0f);
            ImGui::TextUnformatted("Welcome back!");
            ImGui::PopFont();
            Widgets::Hint("Pick a category from the sidebar. Hover any option for more details, "
                          "and click a color swatch to customise it.");
            ImGui::Dummy({ 0, 2 });
            Widgets::InfoRow("Status", "Injected", &Theme::Good);
            Widgets::InfoRow("Renderer", "DirectX 11");
            Widgets::InfoRow("Build", __DATE__);
            Widgets::InfoRow("ImGui", IMGUI_VERSION);
            Widgets::EndCard();

            Widgets::BeginCard("Quick toggles");
            Widgets::Toggle("ESP", &cfg.esp.enabled);
            Widgets::Toggle("Aim assist", &cfg.aim.enabled);
            Widgets::Toggle("Triggerbot", &cfg.aim.trigger);
            Widgets::Toggle("Crosshair", &cfg.misc.crosshair);
            Widgets::EndCard();
        }
        EndColumn();

        BeginColumn("##info_r", 1);
        {
            Widgets::BeginCard("Session");
            const ImGuiIO& io = ImGui::GetIO();

            snprintf(buf, sizeof(buf), "%.0f", io.Framerate);
            Widgets::InfoRow("Frame rate", buf, &Theme::Good);

            snprintf(buf, sizeof(buf), "%.2f ms", 1000.0f / (io.Framerate > 0.0f ? io.Framerate : 1.0f));
            Widgets::InfoRow("Frame time", buf);

            snprintf(buf, sizeof(buf), "%.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
            Widgets::InfoRow("Resolution", buf);

            const int up = (int)ImGui::GetTime();
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", up / 3600, (up / 60) % 60, up % 60);
            Widgets::InfoRow("Uptime", buf);
            Widgets::EndCard();

            Widgets::BeginCard("Hotkeys");
            Widgets::InfoRow("Toggle menu", Widgets::KeyName(cfg.menu.toggleKey), &Theme::Warn);
            Widgets::InfoRow("Aim key",     Widgets::KeyName(cfg.aim.key));
            Widgets::InfoRow("Trigger key", Widgets::KeyName(cfg.aim.triggerKey));
            Widgets::Hint("Hotkeys can be changed in the Aim and Misc tabs.");
            Widgets::EndCard();

            Widgets::BeginCard("Tips");
            Widgets::Hint("- Drag anywhere on the menu background to move it.\n"
                          "- Save your setup in Misc > Config so it survives restarts.\n"
                          "- Change the accent color in Misc > Menu.");
            Widgets::EndCard();
        }
        EndColumn();
    }
}

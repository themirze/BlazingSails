#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

namespace Menu
{
    void RenderAimTab()
    {
        BeginColumn("##aim_l", 0);
        {
            Widgets::BeginCard("Magic bullet");
            Widgets::Toggle("Enable", &cfg.aim.magicBullet);

            ImGui::BeginDisabled(!cfg.aim.magicBullet);
            Widgets::Keybind("Activation key", &cfg.aim.mbKey);
            ImGui::SetItemTooltip("Empty (None) = always active.");
            Widgets::Slider("FOV size", &cfg.aim.mbConeDeg, 10.0f, 180.0f, "%.0f\xC2\xB0");
            Widgets::Toggle("Draw FOV circle", &cfg.aim.drawFov, &cfg.aim.colFov);
            ImGui::EndDisabled();

            Widgets::Hint("Turns the projectile's direction toward the crosshair target.");
            Widgets::EndCard();
        }
        EndColumn();
    }
}

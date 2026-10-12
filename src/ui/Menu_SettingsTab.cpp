#include "ui/Menu.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "core/Hook.h"

#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cmath>

namespace
{
    constexpr unsigned kConfigMagic = 0x424C5A31; // "BLZ1"

    std::string ConfigPath()
    {
        char* appdata = nullptr;
        size_t len = 0;
        std::string dir = (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata) ? appdata : ".";
        free(appdata);
        dir += "\\BlazingInternal";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\config.bin";
    }

    bool SaveConfig()
    {
        FILE* f = nullptr;
        if (fopen_s(&f, ConfigPath().c_str(), "wb") != 0 || !f)
            return false;
        const unsigned size = sizeof(Config);
        fwrite(&kConfigMagic, sizeof(kConfigMagic), 1, f);
        fwrite(&size, sizeof(size), 1, f);
        fwrite(&cfg, sizeof(Config), 1, f);
        fclose(f);
        return true;
    }

    bool LoadConfig()
    {
        FILE* f = nullptr;
        if (fopen_s(&f, ConfigPath().c_str(), "rb") != 0 || !f)
            return false;
        unsigned magic = 0, size = 0;
        Config tmp;
        const bool ok = fread(&magic, sizeof(magic), 1, f) == 1 && magic == kConfigMagic &&
                        fread(&size, sizeof(size), 1, f) == 1 && size == sizeof(Config) &&
                        fread(&tmp, sizeof(Config), 1, f) == 1;
        fclose(f);
        if (ok)
            cfg = tmp;
        return ok;
    }

    // Row of clickable accent color presets
    void AccentPresets()
    {
        static const Color4 presets[] =
        {
            { 1.00f, 0.42f, 0.21f, 1.0f },  // Blaze orange
            { 0.96f, 0.26f, 0.42f, 1.0f },  // Rose
            { 0.62f, 0.40f, 1.00f, 1.0f },  // Violet
            { 0.25f, 0.55f, 1.00f, 1.0f },  // Blue
            { 0.15f, 0.80f, 0.85f, 1.0f },  // Cyan
            { 0.30f, 0.85f, 0.50f, 1.0f },  // Green
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float r = 9.0f;
        for (int i = 0; i < IM_ARRAYSIZE(presets); i++)
        {
            const Color4& c = presets[i];
            if (i > 0)
                ImGui::SameLine(0.0f, 8.0f);

            ImGui::PushID(i);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##preset", { r * 2 + 4, r * 2 + 4 }))
                cfg.menu.accent = c;

            const ImVec2 center = p + ImVec2(r + 2, r + 2);
            const bool   active = std::abs(cfg.menu.accent.r - c.r) < 0.01f && std::abs(cfg.menu.accent.g - c.g) < 0.01f && std::abs(cfg.menu.accent.b - c.b) < 0.01f;
            const float  hover  = Widgets::Animate(ImGui::GetItemID(), ImGui::IsItemHovered() || active ? 1.0f : 0.0f);

            dl->AddCircleFilled(center, r - 1.0f + hover, Theme::U32({ c.r, c.g, c.b, 1.0f }));
            if (active)
                dl->AddCircle(center, r + 2.5f, Theme::U32({ c.r, c.g, c.b, 1.0f }, 0.7f), 0, 1.5f);
            ImGui::PopID();
        }
    }
}

namespace Menu
{
    void RenderSettingsCards()
    {
        Widgets::BeginCard("Menu");
        Widgets::Keybind("Menu key", &cfg.menu.toggleKey);
        Widgets::ColorRow("Accent color", &cfg.menu.accent);
        AccentPresets();
        Widgets::Toggle("Animations", &cfg.menu.animations);
        Widgets::EndCard();

        Widgets::BeginCard("Config");
        const float half = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
        if (Widgets::Button("Save", half, true))
            Notify(SaveConfig() ? "Config saved" : "Failed to save config");
        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::Button("Load", half))
            Notify(LoadConfig() ? "Config loaded" : "No saved config found");
        if (Widgets::Button("Reset to defaults"))
        {
            const int menuKey = cfg.menu.toggleKey;
            cfg = Config{};
            cfg.menu.toggleKey = menuKey;
            Notify("Settings reset");
        }
        Widgets::Hint("Saved to %APPDATA%\\BlazingInternal\\config.bin");
        Widgets::EndCard();

        // ★ Session: oyundan cikmadan DLL'i guvenle ayir (update dongusu).
        //   Buton yalnizca Hook::RequestEject() bayragini kurar; gercek
        //   unload dllmain MainThread'inde, END tusuyla AYNI kanitlanmis
        //   yoldan yapilir (Uninstall -> FreeLibraryAndExitThread).
        Widgets::BeginCard("Session");
        {
            static bool s_confirmEject = false;
            const float half = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;

            if (!s_confirmEject)
            {
                if (Widgets::Button("Clean quit (eject DLL)"))
                {
                    s_confirmEject = true;
                    Notify("Press again to confirm eject");
                }
            }
            else
            {
                if (Widgets::Button("Confirm eject", half, true))
                {
                    Hook::RequestEject();
                    Notify("Ejecting DLL...");
                    s_confirmEject = false;
                }
                ImGui::SameLine(0.0f, 8.0f);
                if (Widgets::Button("Cancel", half))
                    s_confirmEject = false;
            }
            Widgets::Hint("Unloads the cheat without closing the game. Re-inject after updating.");
        }
        Widgets::EndCard();
    }
}

#pragma once

namespace Menu
{
    inline bool open = true;

    // Call once after ImGui::CreateContext() and the platform/renderer backends are initialised.
    void Init();

    // Call every frame between ImGui::NewFrame() and ImGui::Render().
    void Render();

    // Shows a short toast at the bottom of the menu.
    void Notify(const char* text);

    // Two-column page layout helpers
    void BeginColumn(const char* id, int index);
    void EndColumn();

    // Tab pages
    void RenderInfoTab();
    void RenderEspTab();
    void RenderAimTab();
    void RenderMiscTab();

    // Sub-sections
    void RenderCharacterSection();  // ESP -> Players
    void RenderItemSection();       // ESP -> Items
    void RenderSettingsCards();     // Misc -> Menu / Config
}

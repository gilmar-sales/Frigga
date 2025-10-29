#include "PreferencesLayer.hpp"

#include <Frigga/Gui/Styles/Styles.hpp>
#include <imgui.h>

bool PreferencesLayer::IsOpen = false;

void PreferencesLayer::onGui()
{
    if(IsOpen)
    {
        if(ImGui::Begin("Preferences ", &IsOpen,
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            static bool VSync = false;
            if(ImGui::Checkbox("VSync", &VSync))
            {
                // mWindow->SetVSync(VSync);
            }

            static int StyleIndex = 0;
            if(ImGui::Combo("Theme", &StyleIndex,
                            "PhantomDark\0PhantomLight\0Dark\0Light\0Classic\0"))
            {
                switch(StyleIndex)
                {
                case 0:
                    fg::StylePhantomDark();
                    break;
                case 1:
                    fg::StylePhantomLight();
                    break;
                case 2:
                    ImGui::StyleColorsDark();
                    break;
                case 3:
                    ImGui::StyleColorsLight();
                    break;
                case 4:
                    ImGui::StyleColorsClassic();
                    break;
                }
            }

            ImGui::End();
        }
    }
}
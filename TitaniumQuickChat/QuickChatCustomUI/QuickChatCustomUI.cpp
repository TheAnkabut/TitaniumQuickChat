#include "pch.h"
#include "QuickChatCustomUI.h"
#include "Overlay/Media.h"
#include "Overlay/Text.h"
#include "RL/BlockUI.h"

#include "UI/Overlay/Settings.h"
#include "Overlay/Fonts/FontSystem.h"
#include "Persistence/Presets.h"
#include "Persistence/DirectoryWatcher.h"
#include "Utils/Localization.h"

namespace QuickChatCustomUI
{
    static const char* PLUGIN_NAME = "TitaniumQuickChat";

    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        // Base path is TitaniumQuickChat root; actual data goes to profiles/{name}/QuickChatCustomUI/
        std::string dataFolder = wrapper->GetDataFolder().string() + "\\TitaniumQuickChat";
        // Folder creation deferred to Presets::SwitchProfile()

        Localization::Initialize(wrapper);
        FontSystem::Initialize(wrapper, PLUGIN_NAME);
        Media::Initialize(wrapper);
        Presets::Initialize(dataFolder);
        Settings::Initialize(wrapper);
        BlockUI::Initialize(wrapper);


        LOG("QuickChatCustomUI initialized");
    }

    void Shutdown()
    {
        DirectoryWatcher::Stop();
        BlockUI::Shutdown();
        LOG("QuickChatCustomUI shut down");
    }

    void OnProfileChanged(const std::string& profileName)
    {
        Presets::SwitchProfile(profileName);
    }

    void RenderSettings()
    {
        if (!showWindow) return;
        ImGui::SetNextWindowSize(ImVec2(750, 550), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Quick Chat Custom UI", &showWindow))
        {
            ImGui::End();
            return;
        }

        Settings::Render();
        ImGui::End();
    }

    void RenderOverlay()
    {


        if (!Settings::showOverlay)
        {
            return;
        }

        Media::RenderAll();
        Text::Render();
    }
}

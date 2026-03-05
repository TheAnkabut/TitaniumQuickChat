#pragma once
#include <memory>

namespace QuickChatCustomUI
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown();
    void OnProfileChanged(const std::string& profileName);
    void RenderSettings();      
    void RenderOverlay();       // On-screen overlay (media + text, called from Render())

    inline bool showWindow = false;
}

#pragma once
#include <memory>
#include <string>

namespace TitleReplacer
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown();
    
    void SetTitle(const std::string& title);
    void TriggerScan(); // Called by InGameReplacer when UpdateChatGroups fires
    
    // Reading title is done directly via QuickChatData::titleData.customText
}

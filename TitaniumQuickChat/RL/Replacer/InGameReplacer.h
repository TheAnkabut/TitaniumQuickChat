#pragma once
#include <memory>

namespace InGameReplacer
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown(std::shared_ptr<GameWrapper> wrapper);
    void HandleChatPreset();  // Called from BlockUI on ChatPreset events
    void ForceRefresh();
    void ForceRefreshDelayed();
    void NotifySent();
    void MarkDirty();
    void ResetMatchEnded();
}

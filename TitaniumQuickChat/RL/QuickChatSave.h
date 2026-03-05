#pragma once
#include <memory>
#include <string>

namespace QuickChatSave
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Save(int32_t index, const std::string& fnameName);
    void SaveNoRefresh(int32_t index, const std::string& fnameName);
    void SaveDelayed(int32_t index, const std::string& fnameName);
    void SaveNoRefreshDelayed(int32_t index, const std::string& fnameName);
}

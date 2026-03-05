#pragma once
#include <string>
#include <functional>

namespace QuickChatCustomUI {

namespace DirectoryWatcher
{
    void Start(const std::string& path, std::function<void()> callback);
    void Stop();
    
    // Also triggers pending callbacks
    bool IsRunning();
    
    // Call before saving to ignore the resulting file change event
    void IgnoreNextChange();
}
} // namespace QuickChatCustomUI
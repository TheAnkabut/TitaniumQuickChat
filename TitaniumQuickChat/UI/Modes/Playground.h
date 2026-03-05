#pragma once

#include <string>
#include <vector>

namespace Playground
{
    struct UserKey
    {
        std::string name;
        std::string content;
        bool shuffle = false;
    };

    inline std::vector<UserKey> userKeys;
    inline float randomizeAmount = 50.0f;
    inline float waitSeconds = 5.0f;
    inline bool showWindow = false;

    void Render();
    std::string ProcessText(const std::string& text, bool forSending = false, const std::string& fallbackText = "");
    void UpdateAllProcessedText();
}

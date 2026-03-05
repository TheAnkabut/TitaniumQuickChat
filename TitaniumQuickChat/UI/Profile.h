#pragma once
#include <string>

namespace Profile
{
    void Render();
    void Tick();
    void AddProfile(const std::string& name);
    std::string GetCurrentProfileName();
    void InitializeProfiles();
    std::string GetSwitchBind();
    void SetSwitchBind(const std::string& bind);
    bool AnyProfileHasTitle();
}

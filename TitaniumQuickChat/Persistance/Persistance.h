#pragma once
#include <memory>
#include <string>
#include <vector>

namespace Persistance
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    std::string GetTransparentImagePath();
    std::string GetBasePath();
    std::string GetProfilePath(const std::string& profileName);

    // Profile persistence
    void SaveProfile(const std::string& profileName);
    bool LoadProfile(const std::string& profileName);
    std::string ReadProfileTitle(const std::string& profileName);
    std::vector<std::string> GetProfileList();
    void DeleteProfileFolder(const std::string& profileName);

    // Global settings
    void SaveGlobalSettings();
    void LoadGlobalSettings();
    std::string GetSavedSelectedProfile();
}

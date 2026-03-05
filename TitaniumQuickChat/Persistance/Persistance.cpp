#include "pch.h"
#include "Persistance.h"
#include "../logging.h"
#include "../RL/QuickChatData.h"
#include "../UI/Header.h"
#include "../UI/Profile.h"
#include "../UI/Modes/Playground.h"
#include "../RL/MemoryScanner.h"
#include "../Utils/json.hpp"
#include <filesystem>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../Utils/stb_image_write.h"

namespace Persistance
{
    static std::string basePath;
    static std::string transparentImagePath;
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        basePath = wrapper->GetDataFolder().string() + "\\TitaniumQuickChat";
        std::filesystem::create_directories(basePath);
        
        // Ensure profiles directory exists
        std::filesystem::create_directories(basePath + "\\profiles");
        
        transparentImagePath = basePath + "\\transparent.png";
        if (!std::filesystem::exists(transparentImagePath))
        {
            unsigned char pixel[4] = {0, 0, 0, 0};
            stbi_write_png(transparentImagePath.c_str(), 1, 1, 4, pixel, 4);
            LOG("Persistance: Generated transparent.png");
        }
        
        LoadGlobalSettings();
        LOG("Persistance: Initialized");
    }
    
    std::string GetTransparentImagePath()
    {
        return transparentImagePath;
    }
    
    std::string GetBasePath()
    {
        return basePath;
    }
    
    std::string GetProfilePath(const std::string& profileName)
    {
        return basePath + "\\profiles\\" + profileName;
    }

    // ==================== Profile List ====================

    std::vector<std::string> GetProfileList()
    {
        std::vector<std::string> profiles;
        std::string profilesDir = basePath + "\\profiles";
        
        if (!std::filesystem::exists(profilesDir))
        {
            return profiles;
        }
        
        for (auto& entry : std::filesystem::directory_iterator(profilesDir))
        {
            if (entry.is_directory())
            {
                profiles.push_back(entry.path().filename().string());
            }
        }
        
        return profiles;
    }

    // ==================== Save Profile ====================

    void SaveProfile(const std::string& profileName)
    {
        if (profileName.empty())
        {
            return;
        }
        
        std::string profileDir = GetProfilePath(profileName);
        std::filesystem::create_directories(profileDir);
        
        nlohmann::ordered_json root;
        root["name"] = profileName;
        
        // Title
        root["title"] = QuickChatData::titleData.customText;
        
        // 24 bindings
        root["bindings"] = nlohmann::json::array();
        for (int i = 0; i < 24; i++)
        {
            nlohmann::ordered_json binding;
            
            auto* qc = QuickChatData::userBindings[i];
            binding["text"] = qc ? qc->text : "";
            binding["channel"] = static_cast<int>(QuickChatData::slotChannels[i]);
            binding["customText"] = qc ? qc->customText : "";
            
            root["bindings"].push_back(binding);
        }
        
        // Settings categories (6)
        root["settingsCategories"] = nlohmann::json::array();
        for (size_t i = 0; i < QuickChatData::settingsCategories.size() && i < 6; i++)
        {
            root["settingsCategories"].push_back(QuickChatData::settingsCategories[i].customText);
        }
        
        // In-game categories (6)
        root["inGameCategories"] = nlohmann::json::array();
        for (size_t i = 0; i < QuickChatData::inGameCategories.size() && i < 6; i++)
        {
            root["inGameCategories"].push_back(QuickChatData::inGameCategories[i].customText);
        }
        
        // Custom texts for non-bound QCs (available section)
        root["customTexts"] = nlohmann::json::object();
        for (const auto& qc : QuickChatData::allChats)
        {
            if (!qc.customText.empty())
            {
                root["customTexts"][qc.text] = qc.customText;
            }
        }
        
        // Playground keys
        root["playgroundKeys"] = nlohmann::json::array();
        for (const auto& key : Playground::userKeys)
        {
            nlohmann::ordered_json keyObj;
            keyObj["name"] = key.name;
            keyObj["content"] = key.content;
            keyObj["shuffle"] = key.shuffle;
            root["playgroundKeys"].push_back(keyObj);
        }
        
        // Playground settings
        root["randomizeAmount"] = Playground::randomizeAmount;
        root["waitSeconds"] = Playground::waitSeconds;
        
        // View mode (per-profile)
        root["viewMode"] = Header::GetViewMode();
        
        // Write file
        std::string jsonPath = profileDir + "\\quickchats.json";
        std::ofstream file(jsonPath);
        if (file)
        {
            file << root.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
            LOG("Persistance: Saved profile '{}'", profileName);
        }
    }

    // ==================== Read Profile Title ====================

    std::string ReadProfileTitle(const std::string& profileName)
    {
        std::string jsonPath = GetProfilePath(profileName) + "\\quickchats.json";
        if (!std::filesystem::exists(jsonPath)) return "";
        
        std::ifstream file(jsonPath);
        if (!file) return "";
        
        nlohmann::json root;
        try { root = nlohmann::json::parse(file); }
        catch (...) { return ""; }
        
        return root.value("title", "");
    }

    // ==================== Load Profile ====================

    bool LoadProfile(const std::string& profileName)
    {
        if (profileName.empty())
        {
            return false;
        }
        
        std::string jsonPath = GetProfilePath(profileName) + "\\quickchats.json";
        if (!std::filesystem::exists(jsonPath))
        {
            return false;
        }
        
        std::ifstream file(jsonPath);
        if (!file)
        {
            return false;
        }
        
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(file);
        }
        catch (...)
        {
            LOG("Persistance: Failed to parse quickchats.json for '{}'", profileName);
            return false;
        }
        
        // Title
        QuickChatData::titleData.customText = root.value("title", "");
        
        // Clear all custom texts first so they don't bleed between profiles
        for (auto& qc : QuickChatData::allChats)
        {
            qc.customText.clear();
        }
        
        // Load customTexts map first (covers all QCs including available)
        if (root.contains("customTexts") && root["customTexts"].is_object())
        {
            for (auto& [text, customText] : root["customTexts"].items())
            {
                for (auto& qc : QuickChatData::allChats)
                {
                    if (qc.text == text)
                    {
                        qc.customText = customText.get<std::string>();
                        break;
                    }
                }
            }
        }
        
        // Bindings
        if (root.contains("bindings") && root["bindings"].is_array())
        {
            int index = 0;
            for (const auto& binding : root["bindings"])
            {
                if (index >= 24) break;
                
                std::string text = binding.value("text", "");
                int channel = binding.value("channel", 0);
                std::string customText = binding.value("customText", "");
                
                // Find QC by original text
                QuickChatData::QuickChat* found = nullptr;
                for (auto& qc : QuickChatData::allChats)
                {
                    if (qc.text == text)
                    {
                        found = &qc;
                        break;
                    }
                }
                
                QuickChatData::userBindings[index] = found;
                QuickChatData::slotChannels[index] = static_cast<QuickChatData::Channel>(channel);
                
                if (found && !customText.empty())
                {
                    found->customText = customText;
                }
                
                index++;
            }
        }
        
        // Settings categories
        if (root.contains("settingsCategories") && root["settingsCategories"].is_array())
        {
            size_t i = 0;
            for (const auto& cat : root["settingsCategories"])
            {
                if (i >= QuickChatData::settingsCategories.size() || i >= 6) break;
                std::string text = cat.get<std::string>();
                QuickChatData::settingsCategories[i].customText = text;
                
                // Sync UI buffer
                strncpy_s(QuickChatData::settingsBuf[i], text.c_str(), sizeof(QuickChatData::settingsBuf[i]) - 1);
                
                // Write to game memory if address is valid
                auto& sc = QuickChatData::settingsCategories[i];
                if (!text.empty() && sc.address != 0 && sc.maxChars > 0)
                {
                    std::wstring wtext(text.begin(), text.end());
                    MemoryUtils::WriteWideString(sc.address, wtext, sc.maxChars);
                }
                else if (text.empty() && sc.address != 0 && sc.maxChars > 0)
                {
                    // Restore original text
                    std::wstring woriginal(sc.text.begin(), sc.text.end());
                    MemoryUtils::WriteWideString(sc.address, woriginal, sc.maxChars);
                }
                
                i++;
            }
        }
        
        // In-game categories
        if (root.contains("inGameCategories") && root["inGameCategories"].is_array())
        {
            size_t i = 0;
            for (const auto& cat : root["inGameCategories"])
            {
                if (i >= QuickChatData::inGameCategories.size() || i >= 6) break;
                QuickChatData::inGameCategories[i].customText = cat.get<std::string>();
                i++;
            }
        }
        
        // Playground keys
        Playground::userKeys.clear();
        if (root.contains("playgroundKeys") && root["playgroundKeys"].is_array())
        {
            for (const auto& keyObj : root["playgroundKeys"])
            {
                Playground::UserKey key;
                key.name = keyObj.value("name", "");
                key.content = keyObj.value("content", "");
                key.shuffle = keyObj.value("shuffle", false);
                Playground::userKeys.push_back(key);
            }
        }
        
        // Playground settings
        Playground::randomizeAmount = root.value("randomizeAmount", 50.0f);
        Playground::waitSeconds = root.value("waitSeconds", 5.0f);
        
        // View mode (per-profile, backward compat: default Normal)
        Header::SetViewMode(root.value("viewMode", 0));
        
        QuickChatData::buffersDirty = true;
        LOG("Persistance: Loaded profile '{}'", profileName);
        return true;
    }

    // ==================== Delete Profile ====================

    void DeleteProfileFolder(const std::string& profileName)
    {
        if (profileName.empty()) return;
        
        std::string profileDir = GetProfilePath(profileName);
        if (std::filesystem::exists(profileDir))
        {
            std::error_code ec;
            std::filesystem::remove_all(profileDir, ec);
            LOG("Persistance: Deleted profile folder '{}'", profileName);
        }
    }

    // ==================== Global Settings ====================

    void SaveGlobalSettings()
    {
        nlohmann::ordered_json root;
        root["selectedProfile"] = Profile::GetCurrentProfileName();
        root["switchBind"] = Profile::GetSwitchBind();

        root["bindMode"] = Header::GetBindMode();
        root["comboTiming"] = Header::GetComboTiming();
        root["progressiveTiming"] = Header::GetProgressiveTiming();
        root["customChannelPerQC"] = Header::GetCustomChannelPerQC();
        
        std::string settingsPath = basePath + "\\settings.json";
        std::ofstream file(settingsPath);
        if (file)
        {
            file << root.dump(2);
        }
    }

    void LoadGlobalSettings()
    {
        std::string settingsPath = basePath + "\\settings.json";
        if (!std::filesystem::exists(settingsPath))
        {
            return;
        }
        
        std::ifstream file(settingsPath);
        if (!file) return;
        
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(file);
        }
        catch (...)
        {
            return;
        }
        

        Header::SetBindMode(root.value("bindMode", 0));
        Header::SetComboTiming(root.value("comboTiming", 0.2f));
        Header::SetProgressiveTiming(root.value("progressiveTiming", 0.15f));
        Header::SetCustomChannelPerQC(root.value("customChannelPerQC", false));
        Profile::SetSwitchBind(root.value("switchBind", ""));
    }
    
    std::string GetSavedSelectedProfile()
    {
        std::string settingsPath = basePath + "\\settings.json";
        if (!std::filesystem::exists(settingsPath)) return "";
        
        std::ifstream file(settingsPath);
        if (!file) return "";
        
        nlohmann::json root;
        try { root = nlohmann::json::parse(file); }
        catch (...) { return ""; }
        
        return root.value("selectedProfile", "");
    }
}

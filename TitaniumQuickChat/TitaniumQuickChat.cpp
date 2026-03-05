#include "pch.h"
#include "TitaniumQuickChat.h"
#include "UI/Welcome.h"
#include "UI/Header.h"
#include "UI/Layout.h"
#include "UI/Profile.h"
#include "UI/Modes/Playground.h"
#include "RL/Internals.h"
#include "RL/QuickChatBindingsOnShellSet.h"
#include "RL/QuickChatConfig.h"
#include "RL/QuickChatData.h"
#include "RL/MemoryScanner.h"
#include "RL/Localize.h"
#include "RL/QuickChatSave.h"
#include "RL/RefreshSettings.h"
#include "RL/HandleKeyPress.h"
#include "RL/Replacer/SettingsReplacer.h"
#include "RL/Replacer/InGameReplacer.h"
#include "RL/Replacer/TitleReplacer.h"
#include "RL/ChatSender.h"
#include "RL/Data/MatchData.h"
#include "RL/Notification.h"
#include "Persistance/Persistance.h"
#include "QuickChatCustomUI/QuickChatCustomUI.h"
#include "Utils/Localization.h"

BAKKESMOD_PLUGIN(TitaniumQuickChat, "Titanium Quick Chat", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void TitaniumQuickChat::onLoad()
{
    _globalCvarManager = cvarManager;
    LOG("Titanium Quick Chat loaded!");

    // Count
    SendCount();

    Welcome::Initialize(gameWrapper);
    Persistance::Initialize(gameWrapper);
    TitaniumLocalization::Initialize(gameWrapper);
    Internals::Initialize();  
    
    if (Internals::IsInitialized())
    {
        LOG("Internals initialized");
        QuickChatBindingsOnShellSet::Initialize();
        QuickChatConfig::Initialize(gameWrapper);
        Localize::Initialize();
        
        SettingsScanner::Initialize(gameWrapper);
        SettingsScanner::Scan();
        TitleScanner::Initialize(gameWrapper);
        
        QuickChatSave::Initialize(gameWrapper);
        RefreshSettings::Initialize(gameWrapper);
        HandleKeyPress::Initialize(gameWrapper);
        SettingsReplacer::Initialize(gameWrapper);
        InGameReplacer::Initialize(gameWrapper);
        TitleReplacer::Initialize(gameWrapper);

        ChatSender::Initialize(gameWrapper);
        MatchData::Initialize(gameWrapper);
    }
    else
    {
       LOG("Internals failed");
    }

    QuickChatCustomUI::Initialize(gameWrapper);
    
    gameWrapper->RegisterDrawable([](CanvasWrapper) { Profile::Tick(); });
    
    // Discover and load saved profiles from disk
    Profile::InitializeProfiles();
    if (!Profile::GetCurrentProfileName().empty())
    {
        Welcome::SetHasProfile(true);
        
        // Process custom texts so they're ready when SettingsReplacer fires
        Playground::UpdateAllProcessedText();
    }
    
    Notification::Initialize(gameWrapper);
}

void TitaniumQuickChat::onUnload()
{
    QuickChatCustomUI::Shutdown();
    TitleReplacer::Shutdown();
    ChatSender::Shutdown();
    InGameReplacer::Shutdown(gameWrapper);
    SettingsReplacer::Shutdown(gameWrapper);
    HandleKeyPress::Shutdown(gameWrapper);
    LOG("Titanium Quick Chat unloaded.");
}

void TitaniumQuickChat::RenderSettings()
{
    if (!Welcome::HasProfile())
    {
        Welcome::Render();
        return;
    }
    
    Header::Render();
    Layout::Render();
    
    // Render Playground window (separate ImGui window)
    Playground::Render();
    // Render QuickChatCustomUI window (separate ImGui window)
    QuickChatCustomUI::RenderSettings();
}

void TitaniumQuickChat::Render()
{
    Profile::Tick();
    QuickChatCustomUI::RenderOverlay();
}

// ==================== Count ====================

void TitaniumQuickChat::SendCount()
{
    cvarManager->registerCvar("titanium_counted", "0", "Count flag", true, true, 0, true, 1, true);

    gameWrapper->SetTimeout([this](GameWrapper* gw) {
        auto sentCvar = cvarManager->getCvar("titanium_counted");
        if (!sentCvar || sentCvar.getIntValue() == 1)
        {
            LOG("[Count] Already sent, skipping.");
            return;
        }

        // Player ID
        std::string idRL;
        if (gameWrapper->IsUsingEpicVersion()) {
            idRL = gameWrapper->GetEpicID();
        } else {
            idRL = std::to_string(gameWrapper->GetSteamID());
        }

        // Player name
        std::string playerName = gameWrapper->GetPlayerName().ToString();

        // Request body
        std::string body = "{\"pluginName\":\"Titanium Quick Chat\",\"idRL\":\"" + idRL + "\",\"playerName\":\"" + playerName + "\"}";

        CurlRequest req;
        req.url = "https://script.google.com/macros/s/AKfycbyudCL3iGwkhkoCGVDpIL7FOOkXrgTkDjd8g6yJtIz7qwiQ11IOoebbwD5YFL4ctayU/exec";
        req.body = body;

        HttpWrapper::SendCurlJsonRequest(req, [this](int code, std::string result) {
            if (code == 0 || result == "error") {
                LOG("[Count] Failed, will retry next load.");
                return;
            }
            auto sentCvar = cvarManager->getCvar("titanium_counted");
            if (sentCvar) {
                sentCvar.setValue(1);
                cvarManager->executeCommand("writeconfig", false);
                LOG("[Count] Sent successfully. Total: {}", result);
            }
        });
    }, 3.0f);
}

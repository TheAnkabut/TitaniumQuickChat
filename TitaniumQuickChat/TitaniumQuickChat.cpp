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
static bool pluginDisabled = false;

static bool ValidateOffsets()
{
    UObject* qcSave = Internals::FindObject("ProfileQuickChatSave_TA Transient.SaveData_TA.ProfileQuickChatSave_TA");
    if (qcSave) {
        auto* bindings = reinterpret_cast<TArray<FName>*>(reinterpret_cast<uint8_t*>(qcSave) + 0xD0);
        LOG("CHECK Save.Bindings = {}", bindings->Count);
        if (bindings->Count != 24) { LOG("FAILED Save"); return false; }
    }

    UObject* gfxChat = Internals::FindObject("GFxData_Chat_TA Transient.GFxData_Chat_TA");
    if (gfxChat) {
        int32_t maxGroups = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(gfxChat) + 0xE0);
        LOG("CHECK GfxChat.MaxGroups = {}", maxGroups);
        if (maxGroups != 0 && maxGroups != 4) { LOG("FAILED GfxChat"); return false; }
    }

    UObject* notifSave = Internals::FindObject("NotificationSave_TA Transient.SaveData_TA.NotificationSave_TA");
    if (notifSave) {
        uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(notifSave) + 0xE0);
        LOG("CHECK NotifSave.Flags = {}", flags);
        if (flags > 15) { LOG("FAILED NotifSave"); return false; }
    }

    UObject* notifMgr = Internals::FindObject("GFxData_NotificationManager_TA Transient.GFxData_NotificationManager_TA");
    if (notifMgr) {
        uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(notifMgr) + 0xA8);
        LOG("CHECK NotifMgr.Flags = {}", flags);
        if (flags > 3) { LOG("FAILED NotifMgr"); return false; }
    }

    UFunction* testFunc = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
    if (testFunc) {
        uint64_t fflags = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(testFunc) + 0x130);
        LOG("CHECK UFunction.Flags = {}", fflags);
        if (fflags == 0 || fflags > 0xFFFFFFFF) { LOG("FAILED UFunction"); return false; }
    }

    UObject* statEvent = Internals::FindObject("StatEvent_TA StatEvents.Events.TurtleGoal");
    if (statEvent) {
        int32_t pts = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(statEvent) + 0x60);
        uint32_t flgs = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(statEvent) + 0x68);
        LOG("CHECK StatEvent.Points = {} Flags = 0x{:X}", pts, flgs);
        if (pts != 20 || flgs != 0xD) { LOG("FAILED StatEvent"); return false; }
    }

    return true;
}

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

        if (ValidateOffsets())
        {
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

            Notification::Initialize(gameWrapper);
        }
        else
        {
            LOG("Plugin disabled, offset mismatch");
            pluginDisabled = true;
        }
    }
    else
    {
       LOG("Internals failed");
       pluginDisabled = true;
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
}

void TitaniumQuickChat::onUnload()
{
    if (!pluginDisabled)
    {
        QuickChatCustomUI::Shutdown();
        TitleReplacer::Shutdown();
        ChatSender::Shutdown();
        InGameReplacer::Shutdown(gameWrapper);
        SettingsReplacer::Shutdown(gameWrapper);
        HandleKeyPress::Shutdown(gameWrapper);
    }
    LOG("Titanium Quick Chat unloaded.");
}

void TitaniumQuickChat::RenderSettings()
{
    if (pluginDisabled)
    {
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Titanium Quick Chat needs an update.\n\n"
            "Try restarting Rocket League first.\n"
            "If the issue persists, check Discord or GitHub for a newer version."
        );
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImVec4 linkColor = ImVec4(0.30f, 0.85f, 1.0f, 1.0f);
        ImGui::Text("Discord: "); ImGui::SameLine();
        ImGui::TextColored(linkColor, "https://discord.gg/FPvkjaBPEA");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), ImGui::GetColorU32(linkColor), 1.0f);
        }
        if (ImGui::IsItemClicked())
        {
            ShellExecuteA(nullptr, "open", "https://discord.gg/FPvkjaBPEA", nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::Text("GitHub: "); ImGui::SameLine();
        ImGui::TextColored(linkColor, "https://github.com/TheAnkabut/TitaniumQuickChat");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), ImGui::GetColorU32(linkColor), 1.0f);
        }
        if (ImGui::IsItemClicked())
        {
            ShellExecuteA(nullptr, "open", "https://github.com/TheAnkabut/TitaniumQuickChat", nullptr, nullptr, SW_SHOWNORMAL);
        }
        return;
    }

    if (!Welcome::HasProfile())
    {
        Welcome::Render();
        return;
    }
    
    Header::Render();
    Layout::Render();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    bool showNotif = Header::GetShowMatchNotif();
    if (ImGui::Checkbox(TL("show_match_notif"), &showNotif))
    {
        Header::SetShowMatchNotif(showNotif);
        Persistance::SaveGlobalSettings();
    }
    
    // Render Playground window (separate ImGui window)
    Playground::Render();
    // Render QuickChatCustomUI window (separate ImGui window)
    QuickChatCustomUI::RenderSettings();
}

void TitaniumQuickChat::Render()
{
    if (pluginDisabled) return;
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

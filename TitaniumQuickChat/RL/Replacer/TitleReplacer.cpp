// TitleReplacer.cpp - Manages Quick Chat title replacement logic

#include "pch.h"
#include "TitleReplacer.h"
#include "../QuickChatData.h"
#include "../MemoryScanner.h" 
#include "../InvisibleHandleStatEvent.h"
#include "../Notification.h"
#include "../../UI/Profile.h"
#include "../../UI/Header.h"
#include "../../logging.h"
#include "../../Utils/Localization.h"
#include <windows.h>

namespace TitleReplacer
{
    static std::shared_ptr<GameWrapper> gameWrapper;
    
    // ==================== Internal Logic ====================
    
    // Performs the actual memory write using data from QuickChatData
    static void WriteCurrentTitle()
    {
        LOG("TitleReplacer: WriteCurrentTitle called");
        
        // Skip entirely if no profile uses a custom title
        if (!Profile::AnyProfileHasTitle()) {
            LOG("TitleReplacer: No profile has custom title, skipping scan");
            return;
        }
        
        // Re-scan always
        // We scan even if current profile's customText is empty,
        // cuz a profile switch mid-match may need the address
        QuickChatData::titleData.foundAddress = 0;
        LOG("TitleReplacer: Triggering fresh scan");
        TitleScanner::ScanAsync();
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        
        // InitializeQuickChat fires once per match reliably
        wrapper->HookEventPost(
            "Function TAGame.GFxData_Chat_TA.InitializeQuickChat",
            [](std::string)
            {
                InvisibleHandleStatEvent::ResetCache();
                WriteCurrentTitle();
                
                std::string profileName = Profile::GetCurrentProfileName();
                if (!profileName.empty() && Header::GetShowMatchNotif())
                {
                    Notification::Show(profileName, TL("profile_active"));
                }
            });
        
        // Initialize InvisibleHandleStatEvent
        InvisibleHandleStatEvent::Initialize(wrapper);
        
        LOG("TitleReplacer initialized");
    }
    
    void Shutdown()
    {
        LOG("TitleReplacer shutdown");
    }
    
    void SetTitle(const std::string& title)
    {
        // Update centralized data
        QuickChatData::titleData.customText = title;
        
        // Write directly if address is already known (mid-match)
        if (QuickChatData::titleData.foundAddress != 0)
        {
            if (!title.empty())
            {
                std::wstring wtext(title.begin(), title.end());
                MemoryUtils::WriteWideString(
                    QuickChatData::titleData.foundAddress,
                    wtext,
                    QuickChatData::titleData.maxWriteLen
                );
            }
            else
            {
                // Restore original title ("QUICK CHAT" / "CHAT RÁPIDO")
                MemoryUtils::WriteWideString(
                    QuickChatData::titleData.foundAddress,
                    QuickChatData::titleData.originalText,
                    QuickChatData::titleData.maxWriteLen
                );
            }
            InvisibleHandleStatEvent::Trigger();
        }
    }
    
    void TriggerScan()
    {
        WriteCurrentTitle();
    }
}

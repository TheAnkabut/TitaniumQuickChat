// SettingsReplacer.cpp - Replaces quick chat strings in Settings UI

#include "pch.h"
#include "SettingsReplacer.h"
#include "ReplacerUtils.h"
#include "../QuickChatData.h"
#include "../Internals.h"
#include "../../UI/Header.h"

namespace SettingsReplacer
{
    static std::shared_ptr<GameWrapper> gameWrapper;
    
    // ==================== Structures ====================
    
    struct FLocalizedQuickChat
    {
        FString MessageId;                       
        FString Message;                         
        uint8_t QuickChatState;                  
        uint8_t MinAlignmentPadding[0x7];        
    };
    static_assert(sizeof(FLocalizedQuickChat) == 0x28, "size");
    
    // ==================== Helpers ====================
    
    static void ModifySettingsQC(uintptr_t callerAddress)
    {
        if (!callerAddress) return;
        
        // In Simple mode, don't apply customText
        if (Header::GetViewMode() == 1) return;
        
        auto* allQC = reinterpret_cast<TArray<FLocalizedQuickChat>*>(
            reinterpret_cast<uint8_t*>(callerAddress) + 0x98
        );
        
        if (!allQC->Data || allQC->Count <= 0) return;
        
        int count = (std::min)(allQC->Count, static_cast<int>(QuickChatData::allChats.size()));
        int replacedCount = 0;
        
        for (int index = 0; index < count; ++index)
        {
            const std::string& processedText = QuickChatData::allChats[index].processedText;
            if (!processedText.empty())
            {
                ReplacerUtils::WriteFString(&(*allQC)[index].Message, processedText);
                replacedCount++;
            }
        }
        
        if (replacedCount > 0)
        {
            LOG("SettingsReplacer: Replaced {}/{}", replacedCount, count);
        }
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        
        wrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxData_QuickChatBindings_TA.OnShellSet",
            [](ActorWrapper caller, void*, std::string)
            {
                if (caller.memory_address == 0) return;
                ModifySettingsQC(caller.memory_address);
            });
        
        wrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxData_QuickChatBindings_TA.GenerateBindings",
            [](ActorWrapper caller, void*, std::string)
            {
                if (caller.memory_address == 0) return;
                ModifySettingsQC(caller.memory_address);
            });
        
        LOG("SettingsReplacer initialized");
    }
    
    void Shutdown(std::shared_ptr<GameWrapper> wrapper)
    {
        wrapper->UnhookEventPost("Function TAGame.GFxData_QuickChatBindings_TA.OnShellSet");
        wrapper->UnhookEventPost("Function TAGame.GFxData_QuickChatBindings_TA.GenerateBindings");
    }
}

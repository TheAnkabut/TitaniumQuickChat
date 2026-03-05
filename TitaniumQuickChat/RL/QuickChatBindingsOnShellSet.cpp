// QuickChatBindingsOnShellSet.cpp - Populates allChats via OnShellSet

#include "pch.h"
#include "QuickChatBindingsOnShellSet.h"
#include "QuickChatData.h"
#include "Internals.h"

namespace QuickChatBindingsOnShellSet
{
    // ==================== Structures ====================
    
    struct FLocalizedQuickChat
    {
        FString MessageId;                       
        FString Message;                         
        uint8_t QuickChatState;                  
        uint8_t MinAlignmentPadding[0x7];        
    };
    static_assert(sizeof(FLocalizedQuickChat) == 0x28, "size");
    
    // ==================== Public API ====================
    
    void Initialize()
    {
        // Find objects and functions
        UObject* bindingsObj = Internals::FindObject(
            "GFxData_QuickChatBindings_TA TAGame.Default__GFxData_QuickChatBindings_TA"
        );
        if (!bindingsObj)
        {
            LOG("QuickChatBindingsOnShellSet: BindingsObj not found");
            return;
        }
        
        UFunction* onShellSetFunc = UFunction::FindFunction(
            "Function TAGame.GFxData_QuickChatBindings_TA.OnShellSet"
        );
        if (!onShellSetFunc)
        {
            LOG("QuickChatBindingsOnShellSet: OnShellSet not found");
            return;
        }
        
        // Call OnShellSet to populate allQC
        struct UGFxData_QuickChatBindings_TA_OnShellSet_Params { };
        UGFxData_QuickChatBindings_TA_OnShellSet_Params params = {};
        bindingsObj->ProcessEvent(onShellSetFunc, &params, nullptr);
        
        // Read allQC at offset 0x98
        auto* allQC = reinterpret_cast<TArray<FLocalizedQuickChat>*>(
            reinterpret_cast<uint8_t*>(bindingsObj) + 0x98
        );
        
        if (allQC->Count <= 0 || !allQC->Data)
        {
            LOG("QuickChatBindingsOnShellSet: allQC empty");
            return;
        }
        
        // Populate allChats
        QuickChatData::allChats.clear();
        for (int index = 0; index < allQC->Count; ++index)
        {
            FLocalizedQuickChat& localizedQC = (*allQC)[index];
            
            QuickChatData::QuickChat quickChat;
            
            std::string messageIdStr = localizedQC.MessageId.ToUTF8();
            if (!Internals::GetFNameByString(messageIdStr, quickChat.id))
            {
                quickChat.id.FNameEntryId = 0;
                quickChat.id.InstanceNumber = 0;
            }
            
            quickChat.text = localizedQC.Message.ToUTF8();
            quickChat.processedText = quickChat.text;  // Initialize processedText
            QuickChatData::allChats.push_back(quickChat);
        }
        
        QuickChatData::RefreshAvailable();
        LOG("QuickChatBindingsOnShellSet: {} chats", QuickChatData::allChats.size());
    }
}

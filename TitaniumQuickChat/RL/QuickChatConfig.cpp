// QuickChatConfig.cpp - Captures user's 24 configured quick chats

#include "pch.h"
#include "QuickChatConfig.h"
#include "QuickChatData.h"
#include "ChatFilter.h"
#include "Internals.h"
#include "bakkesmod/wrappers/Engine/ActorWrapper.h"
#include <functional>
#include <memory>

namespace QuickChatConfig
{
    // ==================== Structures ====================
    
    struct UGFxData_QuickChatBindings_TA_TranslateQuickChatID_Params
    {
        FName   QID;                             
        FString MessageId;                       
        FString Message;                         
        uint8_t QuickChatState;                  
        uint8_t MinAlignmentPadding[0x7];        
    };
    
    struct UGFxData_QuickChatBindings_TA_GenerateBindings_Params
    {
        FName           QID;                     
        uint8_t         Binding[0x38];           
        TArray<FName>   CurrentBindings;         
        int32_t         I;                       
    };
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        UFunction* translateFunc = UFunction::FindFunction(
            "Function TAGame.GFxData_QuickChatBindings_TA.__GFxData_QuickChatBindings_TA__OnShellSet_0x1"
        );
        if (!translateFunc) return;
        
        // Hook GenerateBindings to capture user's 24 bindings
        wrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxData_QuickChatBindings_TA.GenerateBindings",
            [translateFunc](ActorWrapper caller, void* params, std::string)
            {
                if (!params) return;
                
                auto* bindingsParams = static_cast<UGFxData_QuickChatBindings_TA_GenerateBindings_Params*>(params);
                if (!bindingsParams->CurrentBindings.Data || bindingsParams->CurrentBindings.Count < 24) return;
                
                UObject* qcBindings = reinterpret_cast<UObject*>(caller.memory_address);
                if (!qcBindings) return;
                
                for (int index = 0; index < 24; index++)
                {
                    UGFxData_QuickChatBindings_TA_TranslateQuickChatID_Params translateParams = {};
                    translateParams.QID = bindingsParams->CurrentBindings[index];
                    qcBindings->ProcessEvent(translateFunc, &translateParams, nullptr);
                    
                    QuickChatData::userBindings[index] = QuickChatData::FindByFNameId(translateParams.QID.FNameEntryId);
                }
                
                QuickChatData::RefreshAvailable();
                
                static bool firstCapture = true;
                if (firstCapture)
                {
                    ChatFilter::PopulateDefaultChannels();
                    firstCapture = false;
                }
                LOG("QuickChatConfig: Captured 24");
            });
        
        // Read bindings directly from save (ProfileQuickChatSave_TA.QuickChatBindings at 0xD0)
        auto directRead = []() {
            UObject* save = Internals::FindObject(
                "ProfileQuickChatSave_TA Transient.SaveData_TA.ProfileQuickChatSave_TA"
            );
            if (!save) return false;
            
            auto* bindings = reinterpret_cast<TArray<FName>*>(
                reinterpret_cast<uint8_t*>(save) + 0xD0
            );
            
            if (!bindings->Data || bindings->Count < 24) return false;
            
            for (int i = 0; i < 24; i++)
            {
                FName qcId = (*bindings)[i];
                QuickChatData::userBindings[i] = QuickChatData::FindByFNameId(qcId.FNameEntryId);
            }
            
            QuickChatData::RefreshAvailable();
            ChatFilter::PopulateDefaultChannels();
            return true;
        };
        
        if (!directRead())
        {
            auto retryFunc = std::make_shared<std::function<void(GameWrapper*)>>();
            *retryFunc = [wrapper, directRead, retryFunc](GameWrapper*) {
                static int retryCount = 0;
                if (directRead())
                {
                    retryCount = 0;
                    return;
                }
                if (++retryCount < 5)
                {
                    wrapper->SetTimeout(*retryFunc, 2.0f);
                }
                else
                {
                    retryCount = 0;
                }
            };
            wrapper->SetTimeout(*retryFunc, 2.0f);
        }
        
        // Recapture when user changes bindings in settings
        wrapper->HookEventPost(
            "Function TAGame.GFxData_QuickChatBindings_TA.ChangeBinding",
            [directRead](std::string)
            {
                if (directRead())
                {
                    QuickChatData::buffersDirty = true;
                }
            });
    }
}

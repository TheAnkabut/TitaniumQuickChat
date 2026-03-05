// InGameReplacer.cpp - Replaces quick chat strings in-game (QC wheel)

#include "pch.h"
#include "InGameReplacer.h"
#include "ReplacerUtils.h"
#include "../QuickChatData.h"
#include "../Internals.h"
#include "../../UI/Header.h"

#include "../ChatSender.h"
#include "../Data/MatchData.h"
#include "../../UI/Modes/Playground.h"

namespace InGameReplacer
{
    // ==================== State ====================
    
    static std::shared_ptr<GameWrapper> gameWrapper;
    static uintptr_t lastCallerAddress = 0;
    static uint8_t cachedParams[0x40] = {0};
    static bool cachedParamsValid = false;
    static bool matchEnded = false;
    
    // ChatPreset timing state
    static double lastChatPresetTime = 0.0;
    static bool needsProcess = true;  // Reset by NotifySent when QC is sent
    
    // ==================== Structures ====================
    
    struct FChatPresetMessage
    {
        int32_t  GroupIndex;                     
        FName    Id;                             
        uint32_t bTeam : 1;                      
        FString  Label;                          
    };
    
    struct FChatPresetMessageGroup
    {
        uint32_t bTeam : 1;                      
        uint8_t  UnknownData00[0x4];             
        FString  Label;                          
    };
    
    // ==================== Helpers ====================
    
    static void ModifyLabels(uintptr_t callerAddress)
    {
        LOG("ModifyLabels: START addr={}", callerAddress);
        if (!callerAddress) return;
        
        bool isSimple = (Header::GetViewMode() == 1);
        
        // In Simple mode, skip QC message overrides but still handle PostGame category
        if (!isSimple)
        {
            LOG("ModifyLabels: Getting PresetMessages");
            
            // PresetMessages (24 QCs) at offset 0xB0
            auto* messages = reinterpret_cast<TArray<FChatPresetMessage>*>(
                reinterpret_cast<uint8_t*>(callerAddress) + 0xB0
            );
            
            LOG("ModifyLabels: messages ptr={} Data={} Count={}", (void*)messages, (void*)messages->Data, messages->Count);
            
            if (messages->Data && messages->Count > 0)
            {
                int written = 0;
                for (int index = 0; index < messages->Count && index < 24; ++index)
                {
                    // PostGame override: group 1 (slots 0-3) reads from group 5 (bindings 16-19)
                    int bindingIndex = index;
                    if (matchEnded && index < 4)
                    {
                        bindingIndex = 16 + index;
                    }
                    
                    auto* qc = QuickChatData::userBindings[bindingIndex];
                    if (qc && !qc->processedText.empty())
                    {
                        ReplacerUtils::WriteFString(&(*messages)[index].Label, qc->processedText);
                        written++;
                        if (!qc->customText.empty())
                        {
                            LOG("Slot {}: ptr={} writing processedText='{}'", index, (void*)qc, qc->processedText);
                        }
                    }
                }
                LOG("ModifyLabels: Wrote {} QCs", written);
            }
            
            // PresetGroups (6 categories) at offset 0xA0
            auto* groups = reinterpret_cast<TArray<FChatPresetMessageGroup>*>(
                reinterpret_cast<uint8_t*>(callerAddress) + 0xA0
            );
            
            if (groups->Data && groups->Count > 0)
            {
                for (int index = 0; index < groups->Count && index < static_cast<int>(QuickChatData::inGameCategories.size()); ++index)
                {
                    // PostGame override: category 0 reads from category 4
                    int catSourceIndex = index;
                    if (matchEnded && index == 0 && static_cast<int>(QuickChatData::inGameCategories.size()) > 4)
                    {
                        catSourceIndex = 4;
                    }
                    
                    const auto& cat = QuickChatData::inGameCategories[catSourceIndex];
                    if (!cat.customText.empty())
                    {
                        ReplacerUtils::WriteFString(&(*groups)[index].Label, cat.customText);
                    }
                    else if (matchEnded && catSourceIndex == 4 && !cat.text.empty())
                    {
                        // No custom text for PostGame category, write original 
                        ReplacerUtils::WriteFString(&(*groups)[index].Label, cat.text);
                    }
                }
            }
        }
        else if (matchEnded)
        {
            // Simple mode + PostGame: still need to write category 0 label to prevent corruption 
            auto* groups = reinterpret_cast<TArray<FChatPresetMessageGroup>*>(
                reinterpret_cast<uint8_t*>(callerAddress) + 0xA0
            );
            
            if (groups->Data && groups->Count > 0 && static_cast<int>(QuickChatData::inGameCategories.size()) > 4)
            {
                const auto& cat = QuickChatData::inGameCategories[4];
                const std::string& text = !cat.customText.empty() ? cat.customText : cat.text;
                if (!text.empty())
                {
                    ReplacerUtils::WriteFString(&(*groups)[0].Label, text);
                }
            }
        }
    }
    
    // ==================== ChatPreset Handler ====================
    
    void HandleChatPreset()
    {
        if (!gameWrapper || !gameWrapper->IsInOnlineGame()) return;
        
        double currentTime = ImGui::GetTime();
        double timeSinceLast = currentTime - lastChatPresetTime;
        lastChatPresetTime = currentTime;
        
        bool shouldRefreshWheel = needsProcess || (timeSinceLast > 1.75);
        
        LOG("ChatPreset: needsProcess={} timeSinceLast={:.2f}s shouldRefreshWheel={}", 
            needsProcess, timeSinceLast, shouldRefreshWheel);
        
        if (!shouldRefreshWheel) return;
        
        needsProcess = false;
        
        MatchData::RefreshAll();
        Playground::UpdateAllProcessedText();
        ForceRefresh(); 
    } 
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        
        // Use pre-hook captures before event runs
        wrapper->HookEventWithCaller<ActorWrapper>(
            "Function TAGame.GFxData_Chat_TA.UpdateChatGroups",
            [](ActorWrapper caller, void* params, std::string)
            {
                // Restore original pointers before UpdateChatGroups runs!
                ReplacerUtils::RestoreOriginalPointers();
                
                if (caller.memory_address == 0) return;
                lastCallerAddress = caller.memory_address;
                if (params)    
                {
                    std::memcpy(cachedParams, params, sizeof(cachedParams));
                    cachedParamsValid = true;
                }
            }); 
        
        // Hook post to apply modifications after UpdateChatGroups finishes
        wrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxData_Chat_TA.UpdateChatGroups",
            [](ActorWrapper caller, void* params, std::string)
            {
                if (caller.memory_address == 0) return;
                ModifyLabels(caller.memory_address);
            });
        
        // ChatPreset is hooked by BlockUI, which calls HandleChatPreset()
        
        // PostGame override: group 1 shows PostGame QCs when match ends
        wrapper->HookEvent(
            "Function TAGame.GFxData_Chat_TA.HandleGameEnded",
            [](std::string) {
                matchEnded = true;
            }
        );
        
        // InitializeQuickChat is hooked by BlockUI, which calls ResetMatchEnded()
        
        LOG("InGameReplacer initialized");
    }
    
    void ForceRefresh()
    {
        if (lastCallerAddress == 0 || !cachedParamsValid) return;
        if (!gameWrapper || !gameWrapper->IsInOnlineGame()) return;
        
        // Don't refresh if ChatSender is currently sending 
        if (ChatSender::isSending) {
            LOG("ForceRefresh: Skipped - ChatSender is sending");
            return;
        }
        
        UFunction* updateFunc = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.UpdateChatGroups");
        if (!updateFunc) return;
        
        LOG("ForceRefresh: Calling ProcessEvent directly");
        
        // Call ProcessEvent directly
        reinterpret_cast<UObject*>(lastCallerAddress)->ProcessEvent(updateFunc, cachedParams, nullptr);
        
        LOG("ForceRefresh: Done");
    }
    
    void ForceRefreshDelayed()
    {
        gameWrapper->Execute([](GameWrapper*) {
            ForceRefresh();
        });
    }
    
    void Shutdown(std::shared_ptr<GameWrapper> wrapper)
    {
        wrapper->UnhookEvent("Function TAGame.GFxData_Chat_TA.UpdateChatGroups");
        wrapper->UnhookEventPost("Function TAGame.GFxData_Chat_TA.UpdateChatGroups");
        wrapper->UnhookEvent("Function TAGame.GFxData_Chat_TA.HandleGameEnded");
        matchEnded = false;
    }
    
    void NotifySent()
    {
        // Reset flag so next ChatPreset will process (user sent QC, wheel closed)
        needsProcess = true;
    }
    
    void MarkDirty()
    {
        // Force next ChatPreset to do a full refresh (mode changed, profile switched, etc.)
        needsProcess = true;
    }
    
    void ResetMatchEnded()
    {
        matchEnded = false;
    }
}

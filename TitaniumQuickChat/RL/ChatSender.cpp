// ChatSender.cpp - Send chat messages with interception and filter bypass
//
// HOW IT WORKS:
// 1. Disable SendChatPresetMessage function flags (offset 0x130)
// 2. Hook the event to intercept all QC sends
// 3. On each call: check QuickChatData for custom text
//    - If has custom: send custom via SendChatChannelMessage with custom channel
//    - If no custom: send preset via SendChatPresetMessage with custom channel

#include "pch.h" 
#include "ChatSender.h" 
#include "Internals.h"
#include "../Utils/RLUtils.h"
#include "QuickChatData.h"
#include "Replacer/InGameReplacer.h"
#include "../UI/Header.h"
#include "../UI/Modes/Playground.h"
#include <queue>

namespace ChatSender
{
    // ==================== Structures ====================
    
    struct UGFxData_Chat_TA_execSendChatPresetMessage_Params
    {
        FName    MessageId;                         
        uint32_t bTeam;                             
        uint8_t  padding0[4];                      
        uint8_t  EmptyID[0x48];                   
    };
    
    struct UGFxData_Chat_TA_execSendChatChannelMessage_Params
    {
        FString  Message;                           
        uint8_t  ChatChannel;                       
        uint8_t  padding0[7];                       
        uint8_t  Recipient[0x48];                   
    };
    
    // ==================== State ====================
    
    static std::shared_ptr<GameWrapper> gameWrapper;
    
    static constexpr size_t FLAGS_OFFSET = 0x130;
    static uint64_t originalFlags = 0;
    static bool functionDisabled = false;
    
    static constexpr uintptr_t OFFSET_QUICKCHAT_FILTER = 0x09E1;
    static constexpr uintptr_t OFFSET_MATCH_FILTER     = 0x09E2;
    
    // Message splitting
    static constexpr size_t MAX_MESSAGE_LENGTH = 128;
    static constexpr float DEFAULT_DELAY = 0.1f;  // seconds between chunks
    
    struct PendingMessage {
        std::string text;
        Channel channel;
        float delay;  // seconds to wait before sending this message
    };
    static std::queue<PendingMessage> pendingMessages;
    
    // ==================== Function flag manipulation ====================
    
    static void DisablePresetFunction()
    {
        if (functionDisabled) return;
        
        UFunction* presetFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
        auto* flagsPtr = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(presetFunction) + FLAGS_OFFSET);
        
        originalFlags = *flagsPtr;
        *flagsPtr = 0;
        functionDisabled = true;
    }
    
    static void RestorePresetFunction()
    {
        if (!functionDisabled) return;
        
        UFunction* presetFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
        auto* flagsPtr = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(presetFunction) + FLAGS_OFFSET);
        
        *flagsPtr = originalFlags;
        functionDisabled = false;
    }
    

    static int FindSlotByFNameId(int32_t fnameId)
    {
        for (int i = 0; i < 24; i++)
        {
            auto* qc = QuickChatData::userBindings[i];
            if (qc && qc->id.FNameEntryId == fnameId) return i;
        }
        return -1;
    }
    

    static void ProcessPendingQueue()
    {
        if (pendingMessages.empty()) return;
        
        PendingMessage msg = pendingMessages.front();
        pendingMessages.pop();
        
        SendText(msg.text, msg.channel);
        
        // If more messages, schedule next with its delay
        if (!pendingMessages.empty())
        {
            float nextDelay = pendingMessages.front().delay;
            gameWrapper->SetTimeout([](GameWrapper*) {
                ProcessPendingQueue();
            }, nextDelay);
        }
    }
    
    // ==================== Hook handler ====================
    
    static void OnPresetMessage(ActorWrapper caller, void* params, std::string)
    {
        isSending = true;  // Block ForceRefresh during send
        
        auto* presetParams = reinterpret_cast<UGFxData_Chat_TA_execSendChatPresetMessage_Params*>(params);
        int32_t fnameId = presetParams->MessageId.FNameEntryId;
        
        int slot = FindSlotByFNameId(fnameId);
        QuickChatData::QuickChat* quickChat = (slot >= 0) ? QuickChatData::userBindings[slot] : nullptr;
        
        // Use channel from slotChannels (per-slot, not per-QC)
        uint8_t channel = (slot >= 0) ? static_cast<uint8_t>(QuickChatData::slotChannels[slot]) : 0;
        
        // Bypass filters
        auto playerController = gameWrapper->GetPlayerController();
        if (playerController.IsNull())
        {
            LOG("ChatSender: playerController is null, skipping");
            isSending = false;
            return;
        }
        auto* quickFilterPtr = reinterpret_cast<uint8_t*>(playerController.memory_address + OFFSET_QUICKCHAT_FILTER);
        auto* matchFilterPtr = reinterpret_cast<uint8_t*>(playerController.memory_address + OFFSET_MATCH_FILTER);
        uint8_t originalQuickFilter = *quickFilterPtr;
        uint8_t originalMatchFilter = *matchFilterPtr;
        *quickFilterPtr = 0;
        *matchFilterPtr = 0;
        
        UObject* gfxChat = Internals::FindObject("GFxData_Chat_TA Transient.GFxData_Chat_TA");
        
        // In Simple mode, always send native preset (FName-based)
        bool isSimpleMode = (Header::GetViewMode() == 1);
        
        if (!isSimpleMode && quickChat && !quickChat->customText.empty())
        {
            std::string fullMessage = quickChat->processedText;
            Playground::ProcessText(quickChat->customText, true);
            Playground::UpdateAllProcessedText();
            Channel ch = static_cast<Channel>(channel);
            
            // Helper lambda to send a single chunk
            // If matches qc send as preset
            auto sendChunk = [&](const std::string& msg) {

                for (const auto& qc : QuickChatData::allChats)
                {
                    if (qc.text == msg)
                    {

                        RestorePresetFunction();
                        
                        UGFxData_Chat_TA_execSendChatPresetMessage_Params params = {};
                        params.MessageId = qc.id;
                        params.bTeam = (channel == 1) ? 1 : 0;
                        
                        UFunction* presetFunc = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
                        gfxChat->ProcessEvent(presetFunc, &params, nullptr);
                        
                        DisablePresetFunction();
                        return;
                    }
                }
                

                std::wstring wideMessage = RLUtils::Utf8ToWide(msg);
                FString messageString;
                messageString.Data = const_cast<wchar_t*>(wideMessage.c_str());
                messageString.Count = static_cast<int32_t>(wideMessage.length() + 1);
                messageString.Max = messageString.Count;
                
                UGFxData_Chat_TA_execSendChatChannelMessage_Params channelParams = {};
                channelParams.Message = messageString;
                channelParams.ChatChannel = channel;
                
                UFunction* channelFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatChannelMessage");
                gfxChat->ProcessEvent(channelFunction, &channelParams, nullptr);
            };
            // Parse wait tokens and split into chunks with delays
            // [wait:X] (original) and ·Xs· (processed format)
            std::vector<std::pair<std::string, float>> chunks;  
            std::string currentText;
            float currentDelay = DEFAULT_DELAY;
            size_t i = 0;
            
            while (i < fullMessage.length())
            {
                // Check for [wait:X] token (original)
                if (i + 6 <= fullMessage.length() && fullMessage.substr(i, 6) == "[wait:")
                {
                    size_t endBracket = fullMessage.find(']', i);
                    if (endBracket != std::string::npos)
                    {
                        while (!currentText.empty() && currentText.back() == ' ') {
                            currentText.pop_back();
                        }
                        
                        if (!currentText.empty())
                        {
                            chunks.push_back({currentText, currentDelay});
                            currentText.clear();
                        }
                        
                        std::string delayStr = fullMessage.substr(i + 6, endBracket - i - 6);
                        try {
                            currentDelay = std::stof(delayStr);
                        } catch (...) {
                            currentDelay = DEFAULT_DELAY;
                        }
                        
                        i = endBracket + 1;
                        while (i < fullMessage.length() && fullMessage[i] == ' ') {
                            i++;
                        }
                        continue;
                    }
                }
                
                // Check for ·Xs· format (processed, · = 0xC2 0xB7 in UTF-8)
                // Pattern: space + 0xC2 0xB7 + digits + 's' + 0xC2 0xB7 + space
                if (i + 3 <= fullMessage.length() && 
                    fullMessage[i] == ' ' &&
                    (unsigned char)fullMessage[i+1] == 0xC2 && 
                    (unsigned char)fullMessage[i+2] == 0xB7)
                {
                    // Find the closing ·
                    size_t numStart = i + 3;
                    size_t j = numStart;
                    while (j < fullMessage.length() && (isdigit(fullMessage[j]) || fullMessage[j] == '.'))
                    {
                        j++;
                    }
                    
                    // Check for 's' and closing · and space
                    if (j < fullMessage.length() && fullMessage[j] == 's' &&
                        j + 3 <= fullMessage.length() &&
                        (unsigned char)fullMessage[j+1] == 0xC2 &&
                        (unsigned char)fullMessage[j+2] == 0xB7 &&
                        fullMessage[j+3] == ' ')
                    {
                        while (!currentText.empty() && currentText.back() == ' ') {
                            currentText.pop_back();
                        }
                        
                        if (!currentText.empty())
                        {
                            chunks.push_back({currentText, currentDelay});
                            currentText.clear();
                        }
                        
                        std::string delayStr = fullMessage.substr(numStart, j - numStart);
                        try {
                            currentDelay = std::stof(delayStr);
                        } catch (...) {
                            currentDelay = DEFAULT_DELAY;
                        }
                        
                        i = j + 4;  // Skip past " ·Xs· "
                        continue;
                    }
                }
                
                currentText += fullMessage[i];
                i++;
            }
            
            // Add remaining text
            if (!currentText.empty())
            {
                chunks.push_back({currentText, currentDelay});
            }
            
            // Now split each chunk into 128-char pieces and send
            bool first = true;
            for (auto& [text, delay] : chunks)
            {
                size_t pos = 0;
                bool firstPieceOfChunk = true;
                
                while (pos < text.length())
                {
                    std::string piece = text.substr(pos, MAX_MESSAGE_LENGTH);
                    pos += MAX_MESSAGE_LENGTH;
                    
                    if (first)
                    {
                        // Very first piece - send immediately
                        sendChunk(piece);
                        first = false;
                    }
                    else
                    {
                        // Queue with delay
                        // First piece of a chunk uses its custom delay
                        // Subsequent pieces use DEFAULT_DELAY
                        float pieceDelay = firstPieceOfChunk ? delay : DEFAULT_DELAY;
                        pendingMessages.push({piece, ch, pieceDelay});
                    }
                    firstPieceOfChunk = false;
                }
            }
            
            // Start processing queue
            if (!pendingMessages.empty())
            {
                float firstDelay = pendingMessages.front().delay;
                gameWrapper->SetTimeout([](GameWrapper*) {
                    ProcessPendingQueue();
                }, firstDelay);
            }
        }
        else
        {
            RestorePresetFunction();
            presetParams->bTeam = (channel == 1) ? 1 : 0;
            UFunction* presetFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
            reinterpret_cast<UObject*>(caller.memory_address)->ProcessEvent(presetFunction, params, nullptr);
            DisablePresetFunction();
        }
        
        *quickFilterPtr = originalQuickFilter;
        *matchFilterPtr = originalMatchFilter;
        
        // Notify InGameReplacer that QC was sent - next ChatPreset will process
        InGameReplacer::NotifySent();
        
        
        isSending = false;  // Allow ForceRefresh again
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        
        DisablePresetFunction();
        
        gameWrapper->HookEventWithCaller<ActorWrapper>(
            "Function TAGame.GFxData_Chat_TA.SendChatPresetMessage",
            OnPresetMessage
        );
        
        LOG("ChatSender: Initialized");
    }
    
    void Shutdown()
    {
        RestorePresetFunction();
        gameWrapper->UnhookEvent("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
        LOG("ChatSender: Shutdown");
    }
    
    void SendText(const std::string& message, Channel channel)
    {
        UObject* gfxChat = Internals::FindObject("GFxData_Chat_TA Transient.GFxData_Chat_TA");
        
        gameWrapper->Execute([message, channel, gfxChat](GameWrapper* wrapper) {
            auto playerController = wrapper->GetPlayerController();
            auto* filterPtr = reinterpret_cast<uint8_t*>(playerController.memory_address + OFFSET_MATCH_FILTER);
            uint8_t originalFilter = *filterPtr;
            *filterPtr = 0;
            
            std::wstring wideMessage = RLUtils::Utf8ToWide(message);
            
            FString messageString;
            messageString.Data = const_cast<wchar_t*>(wideMessage.c_str());
            messageString.Count = static_cast<int32_t>(wideMessage.length() + 1);
            messageString.Max = messageString.Count;
            
            UGFxData_Chat_TA_execSendChatChannelMessage_Params params = {};
            params.Message = messageString;
            params.ChatChannel = static_cast<uint8_t>(channel);
            
            UFunction* channelFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatChannelMessage");
            gfxChat->ProcessEvent(channelFunction, &params, nullptr);
            
            *filterPtr = originalFilter;
        });
    }
    
    void SendPreset(int32_t fnameId, bool isTeam)
    {
        UObject* gfxChat = Internals::FindObject("GFxData_Chat_TA Transient.GFxData_Chat_TA");
        
        gameWrapper->Execute([fnameId, isTeam, gfxChat](GameWrapper* wrapper) {
            auto playerController = wrapper->GetPlayerController();
            auto* filterPtr = reinterpret_cast<uint8_t*>(playerController.memory_address + OFFSET_QUICKCHAT_FILTER);
            uint8_t originalFilter = *filterPtr;
            *filterPtr = 0;
            
            RestorePresetFunction();
            
            UGFxData_Chat_TA_execSendChatPresetMessage_Params params = {};
            params.MessageId.FNameEntryId = fnameId;
            params.MessageId.InstanceNumber = 0;
            params.bTeam = isTeam ? 1 : 0;
            
            UFunction* presetFunction = UFunction::FindFunction("Function TAGame.GFxData_Chat_TA.SendChatPresetMessage");
            gfxChat->ProcessEvent(presetFunction, &params, nullptr);
            
            DisablePresetFunction();
            
            *filterPtr = originalFilter;
        });
    }
}

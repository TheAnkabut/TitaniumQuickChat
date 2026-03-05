#pragma once
#include "Internals.h"
#include <string>
#include <array>
#include <vector>
#include <unordered_set>

// ==================== Centralized Quick Chat data ====================

namespace QuickChatData
{
    enum class Channel : uint8_t
    {
        Match = 0,
        Team  = 1,
        Party = 2
    };
    
    struct QuickChat
    {
        FName id = {};
        std::string text;
        std::string customText;      // Raw text with tokens: "[mynickname]"
        std::string processedText;   // Processed text: "PlayerName" (updated in real-time)
        // channel removed - stored per-slot in slotChannels
    };
    
    struct SettingsCategory
    {
        std::string text;
        std::string customText;
        uintptr_t address = 0;
        size_t maxChars = 0;  // Max writable chars before overlapping next data
    };

    struct InGameCategory
    {
        std::string text;
        std::string customText;
    };
    
    // Quick Chat Wheel title data
    struct TitleData
    {
        std::string customText;      // User types this
        std::wstring originalText;   // Original text ("QUICK CHAT" / "CHAT RÁPIDO")
        uintptr_t foundAddress = 0;  // Address found by scanner
        size_t maxWriteLen = 11;     // 11 EN, 12 ES
    };
    
    inline TitleData titleData;
    
    // All quick chats (single source of truth)
    inline std::vector<QuickChat> allChats;
    
    // 24 user quick chats (pointers to items in allChats)
    inline std::array<QuickChat*, 24> userBindings = {};
    
    // Channel per slot (independent of QC)
    inline std::array<Channel, 24> slotChannels = {};
    
    // Available = pointers to items in allChats not in userBindings
    inline std::vector<QuickChat*> availableChats;
    
    // 6 categories
    inline std::vector<SettingsCategory> settingsCategories;
    inline std::vector<InGameCategory> inGameCategories;
    
    // Flag indicating UI buffers need refreshing
    inline bool buffersDirty = true;
    
    // Shared buffers for settings categories InputText
    inline char settingsBuf[6][128] = {};
    
    // Find QuickChat in allChats by FName ID
    inline QuickChat* FindByFNameId(int32_t fnameId)
    {
        for (auto& qc : allChats)
        {
            if (qc.id.FNameEntryId == fnameId) return &qc;
        }
        return nullptr;
    }
    
    // Check if a text appears more than once in userBindings
    inline bool IsDuplicate(const std::string& text)
    {
        if (text.empty()) return false;
        int count = 0;
        for (auto* qc : userBindings)
        {
            if (qc && qc->text == text)
            {
                count++;
                if (count > 1) return true;
            }
        }
        return false;
    }
    
    // Recalculates availableChats (pointers to unused items in allChats)
    inline void RefreshAvailable()
    {
        availableChats.clear();
        
        if (allChats.empty()) return;
        
        std::unordered_set<int32_t> userSet;
        for (auto* qc : userBindings)
        {
            if (qc && qc->id.FNameEntryId != 0)
            {
                userSet.insert(qc->id.FNameEntryId);
            }
        }
        
        for (auto& qc : allChats)
        {
            if (userSet.find(qc.id.FNameEntryId) == userSet.end())
            {
                availableChats.push_back(&qc);
            }
        }
    }
}

// ChatFilter.cpp - Populates channel data based on user's filter settings

#include "pch.h"
#include "ChatFilter.h"
#include "Internals.h"
#include "QuickChatData.h"

namespace ChatFilter
{
    static constexpr uintptr_t OFFSET_QUICKCHAT_FILTER = 0x00D4;
    
    void PopulateDefaultChannels()
    {
        // Read user's filter setting
        UObject* settings = Internals::FindObject(
            "GameplaySettingsSave_TA Transient.SaveData_TA.GameplaySettingsSave_TA"
        );
        
        uint8_t filter = 0;  // Default to All
        if (settings)
        {
            filter = *reinterpret_cast<uint8_t*>(
                reinterpret_cast<uintptr_t>(settings) + OFFSET_QUICKCHAT_FILTER
            );
        }
        
        // Team=1, TeamPreset=4 -> all Team
        bool teamOnly = (filter == 1 || filter == 4);
        
        // Populate per-slot channels
        for (int i = 0; i < 24; i++)
        {
            // TeamOnly: all Team
            // Otherwise: slots 0-3 = Team, rest = Match
            QuickChatData::slotChannels[i] = (teamOnly || i < 4) 
                ? QuickChatData::Channel::Team 
                : QuickChatData::Channel::Match;
        }
    }
}

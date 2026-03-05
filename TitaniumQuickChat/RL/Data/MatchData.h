#pragma once
#include <string>
#include <array>
#include <memory>

namespace MatchData
{
    struct Player
    {
        std::string name;
        int score = 0;
        bool valid = false;
    };

    // Local player
    inline Player me;

    // Teammates (sorted by score)
    inline std::array<Player, 3> teammates;
    inline int teammateCount = 0;

    // Opponents (sorted by score)
    inline std::array<Player, 4> opponents;
    inline int opponentCount = 0;

    // Last events
    inline std::string lastGoalBy;
    inline std::string lastGoalSpeed;
    inline std::string lastSaveBy;
    
    inline std::string goalDiff = "0";
    
    // Last chat message
    inline std::string lastMessage;
    inline std::string lastPlayerName;
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    
    void UpdatePlayers();
    
    std::string GetTimeRemaining();
    
    std::string GetOvertimeTime();
    
    // Called from game thread only
    void RefreshAll();
    
    // Clear all data
    inline void Clear()
    {
        me = Player{};
        teammateCount = 0;
        opponentCount = 0;
        for (auto& t : teammates) t = Player{};
        for (auto& o : opponents) o = Player{};
    }
}


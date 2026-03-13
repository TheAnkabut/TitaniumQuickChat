// MatchData.cpp - Player info, time, events from the game (online only)

#include "pch.h"
#include "MatchData.h"
#include "../Internals.h"
#include "../../UI/Modes/Playground.h"
#include "../Replacer/InGameReplacer.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace MatchData
{
    static std::shared_ptr<GameWrapper> gameWrapper;

    struct StatTickerParams
    {
        uintptr_t Receiver;
        uintptr_t Victim;
        uintptr_t StatEvent;
    };

    // ==================== Handle stat events (goals, saves) ====================
    static void HandleStatEvent(void* params)
    {
        auto* p = reinterpret_cast<StatTickerParams*>(params);
        
        PriWrapper receiver(p->Receiver);
        StatEventWrapper statEvent(p->StatEvent);
        
        std::string playerName = receiver.GetPlayerName().ToString();
        std::string eventType = statEvent.GetEventName();

        if (eventType == "Goal")
        {
            lastGoalBy = playerName;
            
            float speedUU = 0.0f;
            ServerWrapper server = gameWrapper->GetCurrentGameState();
            if (server) {
                BallWrapper ball = server.GetBall();
                if (ball) {
                    Vector v = ball.GetReplicatedRBState().LinearVelocity;
                    speedUU = std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
                }
            }
            
            bool isMetric = gameWrapper->GetbMetric();
            float converted = speedUU * (isMetric ? 0.036f : 0.022369f);
            lastGoalSpeed = std::to_string(static_cast<int>(std::round(converted))) + (isMetric ? " KMH" : " MPH");
        }
        else if (eventType == "Save" || eventType == "EpicSave")
        {
            lastSaveBy = playerName;
        }
        
        Playground::UpdateAllProcessedText();
        InGameReplacer::ForceRefreshDelayed();
    }

    // ==================== Handle chat messages ====================
    static void HandleChatMessage(void* params)
    {
        uintptr_t base = reinterpret_cast<uintptr_t>(params);
        
        // 0x08: FString PlayerName
        // 0x18: FString Message
        FString* playerNamePtr = reinterpret_cast<FString*>(base + 0x08);
        FString* messagePtr = reinterpret_cast<FString*>(base + 0x18);
        
        if (playerNamePtr->Data && playerNamePtr->Count > 0)
        {
            lastPlayerName = playerNamePtr->ToUTF8();
        }
        
        if (messagePtr->Data && messagePtr->Count > 0)
        {
            lastMessage = messagePtr->ToUTF8();
        }
        
        LOG("CHAT: [{}] {}", lastPlayerName, lastMessage);
        
        Playground::UpdateAllProcessedText();
    }

    // ==================== Initialize hooks ====================
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;

        gameWrapper->HookEvent(
            "Function TAGame.GameEvent_TA.EventPlayerAdded",
            [](std::string) {
                gameWrapper->SetTimeout([](GameWrapper*) { UpdatePlayers(); }, 0.3f);
            }
        );

        gameWrapper->HookEvent(
            "Function TAGame.GameEvent_TA.EventPlayerRemoved",
            [](std::string) {
                gameWrapper->SetTimeout([](GameWrapper*) { UpdatePlayers(); }, 0.3f);
            }
        );

        gameWrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
            [](ActorWrapper, void* params, std::string) {
                HandleStatEvent(params);
            }
        );

        gameWrapper->HookEventWithCallerPost<ActorWrapper>(
            "Function TAGame.GFxData_Chat_TA.OnChatMessage",
            [](ActorWrapper, void* params, std::string) {
                if (params) HandleChatMessage(params);
            }
        );
    }

    // ==================== Update player data ====================
    void UpdatePlayers()
    {
        Clear();
        
        if (!gameWrapper->IsInOnlineGame()) return;

        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server) return;
        
        PlayerControllerWrapper pc = gameWrapper->GetPlayerController();
        if (pc.IsNull()) return;
        
        PriWrapper localPRI = pc.GetPRI();
        if (localPRI.IsNull()) return;
        
        me.name = localPRI.GetPlayerName().ToString();
        me.score = localPRI.GetMatchScore();
        me.valid = true;
        
        int myTeam = localPRI.GetTeamNum();

        std::vector<Player> teamList;
        std::vector<Player> oppList;
        
        ArrayWrapper<PriWrapper> pris = server.GetPRIs();
        for (int i = 0; i < pris.Count(); i++)
        {
            PriWrapper pri = pris.Get(i);
            
            std::string name = pri.GetPlayerName().ToString();
            int team = pri.GetTeamNum();
            int score = pri.GetMatchScore();

            if (name == me.name || team == 255) continue;

            Player player;
            player.name = name;
            player.score = score;
            player.valid = true;

            if (team == myTeam)
            {
                teamList.push_back(player);
            }
            else
            {
                oppList.push_back(player);
            }
        }

        // Sort by score (highest first)
        auto sortByScore = [](const Player& a, const Player& b) {
            return a.score > b.score;
        };
        std::sort(teamList.begin(), teamList.end(), sortByScore);
        std::sort(oppList.begin(), oppList.end(), sortByScore);

        // Copy to arrays
        teammateCount = std::min(static_cast<int>(teamList.size()), 3);
        for (int i = 0; i < teammateCount; i++)
        {
            teammates[i] = teamList[i];
        }

        opponentCount = std::min(static_cast<int>(oppList.size()), 4);
        for (int i = 0; i < opponentCount; i++)
        {
            opponents[i] = oppList[i];
        }
        
        Playground::UpdateAllProcessedText();
    }

    // ==================== Get time remaining ====================
    std::string GetTimeRemaining()
    {
        if (!gameWrapper || !gameWrapper->IsInOnlineGame()) return "";
        
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server) return "";
        
        int secs = server.GetSecondsRemaining();
        if (secs < 0) secs = 0;
        
        int mins = secs / 60;
        int sec = secs % 60;

        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d", mins, sec);
        return buf;
    }

    // ==================== Get overtime time ====================
    std::string GetOvertimeTime()
    {
        if (!gameWrapper || !gameWrapper->IsInOnlineGame()) return "";
        
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server) return "";
        
        if (!server.GetbOverTime()) return "";
        
        int secs = static_cast<int>(server.GetOvertimeTimePlayed());
        if (secs < 0) secs = 0;
        
        int mins = secs / 60;
        int sec = secs % 60;

        char buf[16];
        snprintf(buf, sizeof(buf), "+%d:%02d", mins, sec);
        return buf;
    }
    
    // ==================== Update goal difference ====================
    void UpdateGoalDiff()
    {
        if (!gameWrapper || !gameWrapper->IsInOnlineGame())
        {
            goalDiff = "0";
            return;
        }
        
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server)
        {
            goalDiff = "0";
            return;
        }
        
        ArrayWrapper<TeamWrapper> teams = server.GetTeams();
        if (teams.Count() < 2)
        {
            goalDiff = "0";
            return;
        }
        
        // Get local player's team
        PlayerControllerWrapper pc = gameWrapper->GetPlayerController();
        if (pc.IsNull())
        {
            goalDiff = "0";
            return;
        }
        
        PriWrapper pri = pc.GetPRI();
        if (pri.IsNull())
        {
            goalDiff = "0";
            return;
        }
        
        int myTeamNum = pri.GetTeamNum();
        
        int myTeamScore = 0;
        int oppTeamScore = 0;
        
        for (int i = 0; i < teams.Count(); i++)
        {
            TeamWrapper team = teams.Get(i);
            if (team.GetTeamNum() == myTeamNum)
            {
                myTeamScore = team.GetScore();
            }
            else
            {
                oppTeamScore = team.GetScore();
            }
        }
        
        int diff = myTeamScore - oppTeamScore;
        
        if (diff > 0)
            goalDiff = "+" + std::to_string(diff);
        else
            goalDiff = std::to_string(diff);
    }
    
    // ==================== Refresh all (game thread only) ====================
    void RefreshAll()
    {
        if (!gameWrapper) return;
        if (!gameWrapper->IsInOnlineGame()) return;
        
        UpdatePlayers();
        UpdateGoalDiff();
    }
}

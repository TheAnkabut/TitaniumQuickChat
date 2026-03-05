#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <atomic>

namespace ChatSender {

    enum class Channel : uint8_t {
        Match = 0,
        Team  = 1,
        Party = 2
    };

    // Flag to indicate if a send is in progress (to prevent ForceRefresh)
    inline std::atomic<bool> isSending{false};

    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown();

    // Send custom text message (bypasses MatchChatFilter)
    void SendText(const std::string& message, Channel channel);

    // Send quick chat preset by FName ID (bypasses QuickChatFilter)
    void SendPreset(int32_t fnameId, bool bTeam);

} // namespace ChatSender

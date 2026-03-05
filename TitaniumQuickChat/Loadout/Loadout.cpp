// Loadout.cpp - Encode/decode profiles as shareable codes

#include "pch.h"
#include "Loadout.h"
#include "../RL/QuickChatData.h"
#include "../UI/Header.h"
#include "../Persistance/Persistance.h"
#include "../UI/Profile.h"
#include "../RL/Notification.h"
#include "../RL/RefreshSettings.h"
#include "../RL/Replacer/InGameReplacer.h"
#include "../RL/Replacer/TitleReplacer.h"
#include "../UI/Modes/Playground.h"
#include "../Utils/Miniz/miniz.h"
#include "../Utils/Localization.h"
#include "../logging.h"
#include <vector>
#include <string>
#include <algorithm>

// ==================== Base64 ====================

namespace
{
    static const char base64Chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string Base64Encode(const std::vector<uint8_t>& data)
    {
        std::string out;
        int val = 0, valb = -6;
        for (uint8_t c : data)
        {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0)
            {
                out.push_back(base64Chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6)
            out.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4)
            out.push_back('=');
        return out;
    }
    
    std::vector<uint8_t> Base64Decode(const std::string& in)
    {
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++)
            T[base64Chars[i]] = i;
        
        std::vector<uint8_t> out;
        int val = 0, valb = -8;
        for (char c : in)
        {
            if (T[static_cast<unsigned char>(c)] == -1) break;
            val = (val << 6) + T[static_cast<unsigned char>(c)];
            valb += 6;
            if (valb >= 0)
            {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
    
    // ==================== Binary Writer/Reader ====================
    
    class BinaryWriter
    {
    public:
        std::vector<uint8_t> data;
        
        void WriteByte(uint8_t v) { data.push_back(v); }
        
        void WriteU16(uint16_t v)
        {
            data.push_back(static_cast<uint8_t>(v & 0xFF));
            data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        }
        
        void WriteFloat(float v)
        {
            uint8_t bytes[4];
            std::memcpy(bytes, &v, 4);
            for (int i = 0; i < 4; i++) data.push_back(bytes[i]);
        }
        
        void WriteString(const std::string& s)
        {
            // Length-prefixed: 2 bytes (supports up to 65535 chars)
            uint16_t len = static_cast<uint16_t>(std::min(s.size(), (size_t)65535));
            WriteU16(len);
            for (uint16_t i = 0; i < len; i++)
                data.push_back(static_cast<uint8_t>(s[i]));
        }
    };
    
    class BinaryReader
    {
    public:
        const uint8_t* data;
        size_t size;
        size_t pos = 0;
        bool error = false;
        
        BinaryReader(const uint8_t* d, size_t s) : data(d), size(s) {}
        
        uint8_t ReadByte()
        {
            if (pos >= size) { error = true; return 0; }
            return data[pos++];
        }
        
        uint16_t ReadU16()
        {
            if (pos + 1 >= size) { error = true; return 0; }
            uint16_t v = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            return v;
        }
        
        float ReadFloat()
        {
            if (pos + 3 >= size) { error = true; return 0.0f; }
            float v;
            std::memcpy(&v, data + pos, 4);
            pos += 4;
            return v;
        }
        
        std::string ReadString()
        {
            uint16_t len = ReadU16();
            if (error || pos + len > size) { error = true; return ""; }
            std::string s(reinterpret_cast<const char*>(data + pos), len);
            pos += len;
            return s;
        }
    };
    
    // ==================== Compression helpers ====================
    
    std::vector<uint8_t> Compress(const std::vector<uint8_t>& input)
    {
        mz_ulong compressedSize = mz_compressBound(static_cast<mz_ulong>(input.size()));
        std::vector<uint8_t> compressed(compressedSize);
        
        int result = mz_compress(compressed.data(), &compressedSize,
                                  input.data(), static_cast<mz_ulong>(input.size()));
        if (result != MZ_OK)
            return {};
        
        compressed.resize(compressedSize);
        return compressed;
    }
    
    std::vector<uint8_t> Decompress(const std::vector<uint8_t>& input, size_t maxOutput = 1024 * 64)
    {
        mz_ulong outputSize = static_cast<mz_ulong>(maxOutput);
        std::vector<uint8_t> output(outputSize);
        
        int result = mz_uncompress(output.data(), &outputSize,
                                    input.data(), static_cast<mz_ulong>(input.size()));
        if (result != MZ_OK)
            return {};
        
        output.resize(outputSize);
        return output;
    }
    
    // Find allChats index by original text
    int FindQCIndex(const std::string& text)
    {
        for (int i = 0; i < static_cast<int>(QuickChatData::allChats.size()); i++)
        {
            if (QuickChatData::allChats[i].text == text)
                return i;
        }
        return -1;
    }
}

// ==================== Public API ====================

namespace Loadout
{
    static constexpr uint8_t LOADOUT_VERSION = 1;
    static bool showLoadModal = false;
    static char loadBuffer[4096] = {};
    static std::string loadStatus;
    
    std::string Encode()
    {
        if (QuickChatData::allChats.empty())
            return "";
        
        BinaryWriter w;
        
        // Header
        w.WriteByte(LOADOUT_VERSION);
        w.WriteByte(static_cast<uint8_t>(Header::GetViewMode()));
        w.WriteByte(static_cast<uint8_t>(Header::GetCustomChannelPerQC() ? 1 : 0));
        
        // Title
        w.WriteString(QuickChatData::titleData.customText);
        
        // 24 bindings
        for (int i = 0; i < 24; i++)
        {
            auto* qc = QuickChatData::userBindings[i];
            if (qc)
            {
                int idx = FindQCIndex(qc->text);
                w.WriteByte(static_cast<uint8_t>(idx >= 0 ? idx : 0xFF));
                w.WriteByte(static_cast<uint8_t>(QuickChatData::slotChannels[i]));
                w.WriteString(qc->customText);
            }
            else
            {
                w.WriteByte(0xFF); // empty slot
                w.WriteByte(0);
                w.WriteString("");
            }
        }
        
        // 6 settings categories
        for (size_t i = 0; i < 6; i++)
        {
            if (i < QuickChatData::settingsCategories.size())
                w.WriteString(QuickChatData::settingsCategories[i].customText);
            else
                w.WriteString("");
        }
        
        // 6 in-game categories
        for (size_t i = 0; i < 6; i++)
        {
            if (i < QuickChatData::inGameCategories.size())
                w.WriteString(QuickChatData::inGameCategories[i].customText);
            else
                w.WriteString("");
        }
        
        // Available QCs with custom text
        std::vector<std::pair<uint8_t, std::string>> availCustoms;
        for (int i = 0; i < static_cast<int>(QuickChatData::allChats.size()); i++)
        {
            auto& qc = QuickChatData::allChats[i];
            if (!qc.customText.empty())
            {
                // Check if this QC is already in a binding (custom text already encoded above)
                bool isBound = false;
                for (auto* b : QuickChatData::userBindings)
                {
                    if (b && b->text == qc.text) { isBound = true; break; }
                }
                if (!isBound)
                {
                    availCustoms.push_back({ static_cast<uint8_t>(i), qc.customText });
                }
            }
        }
        
        w.WriteByte(static_cast<uint8_t>(availCustoms.size()));
        for (auto& [idx, text] : availCustoms)
        {
            w.WriteByte(idx);
            w.WriteString(text);
        }
        
        // Playground keys
        w.WriteByte(static_cast<uint8_t>(std::min(Playground::userKeys.size(), (size_t)255)));
        for (const auto& key : Playground::userKeys)
        {
            w.WriteString(key.name);
            w.WriteString(key.content);
            w.WriteByte(key.shuffle ? 1 : 0);
        }
        
        // Playground settings
        w.WriteFloat(Playground::randomizeAmount);
        w.WriteFloat(Playground::waitSeconds);
        
        // Compress + base64
        auto compressed = Compress(w.data);
        if (compressed.empty())
        {
            LOG("Loadout: compression failed");
            return "";
        }
        
        // Prepend original size (4 bytes LE) for decompression
        std::vector<uint8_t> payload;
        uint32_t origSize = static_cast<uint32_t>(w.data.size());
        payload.push_back(origSize & 0xFF);
        payload.push_back((origSize >> 8) & 0xFF);
        payload.push_back((origSize >> 16) & 0xFF);
        payload.push_back((origSize >> 24) & 0xFF);
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        
        return Base64Encode(payload);
    }
    
    bool Decode(const std::string& code)
    {
        if (code.empty() || QuickChatData::allChats.empty())
            return false;
        
        // Base64 decode
        auto payload = Base64Decode(code);
        if (payload.size() < 5)
            return false;
        
        // Read original size
        uint32_t origSize = payload[0] | (payload[1] << 8) |
                            (payload[2] << 16) | (payload[3] << 24);
        if (origSize > 1024 * 64)
            return false;
        
        // Decompress
        std::vector<uint8_t> compressed(payload.begin() + 4, payload.end());
        auto raw = Decompress(compressed, origSize);
        if (raw.empty())
            return false;
        
        BinaryReader r(raw.data(), raw.size());
        
        // Header
        uint8_t version = r.ReadByte();
        if (version != 1 || r.error)
            return false;
        
        uint8_t viewMode = r.ReadByte();
        if (r.error) return false;
        
        bool customChannelPerQC = r.ReadByte() != 0;
        if (r.error) return false;
        
        // Title
        std::string titleCustom = r.ReadString();
        if (r.error) return false;
        
        // 24 bindings
        struct BindingData {
            uint8_t qcIndex;
            uint8_t channel;
            std::string customText;
        };
        std::array<BindingData, 24> bindings;
        
        for (int i = 0; i < 24; i++)
        {
            bindings[i].qcIndex = r.ReadByte();
            bindings[i].channel = r.ReadByte();
            bindings[i].customText = r.ReadString();
            if (r.error) return false;
        }
        
        // 6 settings categories
        std::array<std::string, 6> settingsCats;
        for (int i = 0; i < 6; i++)
        {
            settingsCats[i] = r.ReadString();
            if (r.error) return false;
        }
        
        // 6 in-game categories
        std::array<std::string, 6> inGameCats;
        for (int i = 0; i < 6; i++)
        {
            inGameCats[i] = r.ReadString();
            if (r.error) return false;
        }
        
        // Available customs
        uint8_t availCount = r.ReadByte();
        if (r.error) return false;
        
        struct AvailData {
            uint8_t qcIndex;
            std::string customText;
        };
        std::vector<AvailData> availCustoms;
        for (int i = 0; i < availCount; i++)
        {
            AvailData ad;
            ad.qcIndex = r.ReadByte();
            ad.customText = r.ReadString();
            if (r.error) return false;
            availCustoms.push_back(ad);
        }
        
        // ==================== Apply to current profile ====================
        
        int chatCount = static_cast<int>(QuickChatData::allChats.size());
        
        // Clear all custom texts first
        for (auto& qc : QuickChatData::allChats)
            qc.customText.clear();
        
        // View mode
        Header::SetViewMode(viewMode);
        
        // Custom channel per QC
        Header::SetCustomChannelPerQC(customChannelPerQC);
        
        // Title
        QuickChatData::titleData.customText = titleCustom;
        
        // Bindings
        for (int i = 0; i < 24; i++)
        {
            auto& bd = bindings[i];
            if (bd.qcIndex < chatCount)
            {
                QuickChatData::userBindings[i] = &QuickChatData::allChats[bd.qcIndex];
                QuickChatData::slotChannels[i] = static_cast<QuickChatData::Channel>(bd.channel);
                if (!bd.customText.empty())
                    QuickChatData::allChats[bd.qcIndex].customText = bd.customText;
            }
            else
            {
                QuickChatData::userBindings[i] = nullptr;
                QuickChatData::slotChannels[i] = QuickChatData::Channel::Match;
            }
        }
        
        // Settings categories
        for (size_t i = 0; i < 6 && i < QuickChatData::settingsCategories.size(); i++)
        {
            QuickChatData::settingsCategories[i].customText = settingsCats[i];
            strncpy_s(QuickChatData::settingsBuf[i], settingsCats[i].c_str(),
                      sizeof(QuickChatData::settingsBuf[i]) - 1);
        }
        
        // In-game categories
        for (size_t i = 0; i < 6 && i < QuickChatData::inGameCategories.size(); i++)
        {
            QuickChatData::inGameCategories[i].customText = inGameCats[i];
        }
        
        // Available customs
        for (auto& ad : availCustoms)
        {
            if (ad.qcIndex < chatCount)
                QuickChatData::allChats[ad.qcIndex].customText = ad.customText;
        }
        
        // Playground keys
        uint8_t keyCount = r.ReadByte();
        if (r.error) return false;
        
        Playground::userKeys.clear();
        for (int i = 0; i < keyCount && !r.error; i++)
        {
            Playground::UserKey key;
            key.name = r.ReadString();
            key.content = r.ReadString();
            key.shuffle = r.ReadByte() != 0;
            if (!r.error)
                Playground::userKeys.push_back(key);
        }
        
        // Playground settings
        Playground::randomizeAmount = r.ReadFloat();
        Playground::waitSeconds = r.ReadFloat();
        if (r.error) return false;
        
        QuickChatData::buffersDirty = true;
        
        // Save + refresh
        Persistance::SaveProfile(Profile::GetCurrentProfileName());
        Playground::UpdateAllProcessedText();
        RefreshSettings::RefreshDelayed();
        InGameReplacer::ForceRefreshDelayed();
        TitleReplacer::SetTitle(titleCustom);
        
        LOG("Loadout: decoded and applied to profile '{}'", Profile::GetCurrentProfileName());
        return true;
    }
    
    void OpenLoadModal()
    {
        showLoadModal = true;
        loadBuffer[0] = '\0';
        loadStatus.clear();
    }
    
    void RenderLoadModal()
    {
        if (showLoadModal)
        {
            ImGui::OpenPopup(TL("insert_share_code"));
            showLoadModal = false;
        }
        
        // Center the popup
        ImVec2 center = ImGui::GetIO().DisplaySize;
        center.x *= 0.5f;
        center.y *= 0.5f;
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        bool popupOpen = true;
        if (ImGui::BeginPopupModal(TL("insert_share_code"), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            ImGui::SetNextItemWidth(400);
            ImGui::InputText(TL("code"), loadBuffer, sizeof(loadBuffer));
            
            if (ImGui::Button(TL("load")))
            {
                std::string code(loadBuffer);
                code.erase(0, code.find_first_not_of(" \t\r\n"));
                code.erase(code.find_last_not_of(" \t\r\n") + 1);
                
                if (code.empty())
                {
                    loadStatus = TL("paste_first");
                }
                else if (Decode(code))
                {
                    Notification::Show(TL("loadout_loaded"), std::string(TL("profile_prefix")) + Profile::GetCurrentProfileName());
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    loadStatus = TL("invalid_code");
                }
            }
            
            if (!loadStatus.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "%s", loadStatus.c_str());
            }
            
            ImGui::EndPopup();
        }
    }
}

#include "pch.h"
#include "Normal.h"
#include "Playground.h"
#include "../../QuickChatCustomUI/QuickChatCustomUI.h"
#include "../../QuickChatCustomUI/UI/Overlay/Settings.h"
#include "../Header.h"
#include "../Profile.h"
#include "../../RL/QuickChatData.h"
#include "../../Persistance/Persistance.h"
#include "../../RL/MemoryScanner.h"
#include "../../RL/RefreshSettings.h"
#include "../../RL/Replacer/InGameReplacer.h"
#include "../../Utils/Localization.h"
#include "../../Utils/RLUtils.h"

namespace NormalMode
{
    // Colors
    const ImVec4 COLOR_CAT_INGAME   = {1.0f, 1.0f, 1.0f, 1.0f};
    const ImVec4 COLOR_CAT_GLOW     = {0.2f, 0.8f, 1.0f, 1.0f};
    const ImVec4 COLOR_CAT_SETTINGS = {0.7f, 0.9f, 1.0f, 1.0f};
    
    void TextWithGlow(const ImVec4& textColor, const ImVec4& glowColor, float glowIntensity, const char* text) 
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        constexpr float offset = 1.0f;
        float alpha = glowIntensity * 0.4f;
        ImU32 glow = ImGui::ColorConvertFloat4ToU32(
            ImVec4(glowColor.x, glowColor.y, glowColor.z, alpha)
        );
        
        drawList->AddText(ImVec2(pos.x + offset, pos.y), glow, text);
        drawList->AddText(ImVec2(pos.x - offset, pos.y), glow, text);
        drawList->AddText(ImVec2(pos.x, pos.y + offset), glow, text);
        drawList->AddText(ImVec2(pos.x, pos.y - offset), glow, text);
        
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::Text("%s", text);
        ImGui::PopStyleColor();
    }

    // Buffers for ImGui InputText
    static char userInputBuffers[24][256] = {};
    static std::vector<std::array<char, 256>> availableInputBuffers;
    static char inGameBuf[6][128] = {};
    // Callback to insert § every 128 chars
    static constexpr const char* SEP = "\xC2\xA7";  // § in UTF-8
    
    static int SeparatorCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
        {
            std::string input(data->Buf, data->BufTextLen);
            
            // Remove all § (2-byte sequence)
            std::string clean;
            for (size_t i = 0; i < input.size(); i++)
            {
                // Check for § (0xC2 0xA7)
                if (i + 1 < input.size() && 
                    (unsigned char)input[i] == 0xC2 && 
                    (unsigned char)input[i+1] == 0xA7)
                {
                    i++;  // Skip both bytes
                    continue;
                }
                clean += input[i];
            }
            
            // Build new string with separators every 128 chars
            std::string withSep;
            size_t count = 0;
            for (char c : clean)
            {
                if (count > 0 && count % 128 == 0)
                {
                    withSep += SEP;
                }
                withSep += c;
                count++;
            }
            
            // Only update if different
            if (input != withSep)
            {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, withSep.c_str());
            }
        }
        return 0;
    }
    
    static void RefreshBuffersIfNeeded()
    {
        if (!QuickChatData::buffersDirty) return;
        
        // Sync quickchat buffers
        for (int i = 0; i < 24; i++)
        {
            auto* qc = QuickChatData::userBindings[i];
            if (qc)
            {
                strncpy_s(userInputBuffers[i], qc->customText.c_str(), 255);
            }
            else
            {
                userInputBuffers[i][0] = '\0';
            }
        }
        
        // Sync settings category buffers (same pattern as quickchats)
        for (int i = 0; i < 6; i++)
        {
            if (i < static_cast<int>(QuickChatData::settingsCategories.size()))
            {
                strncpy_s(QuickChatData::settingsBuf[i], QuickChatData::settingsCategories[i].customText.c_str(), 127);
            }
            else
            {
                QuickChatData::settingsBuf[i][0] = '\0';
            }
        }
        
        // Sync in-game category buffers (same pattern as quickchats)
        for (int i = 0; i < 6; i++)
        {
            if (i < static_cast<int>(QuickChatData::inGameCategories.size()))
            {
                strncpy_s(inGameBuf[i], QuickChatData::inGameCategories[i].customText.c_str(), 127);
            }
            else
            {
                inGameBuf[i][0] = '\0';
            }
        }
        
        availableInputBuffers.clear();
        QuickChatData::buffersDirty = false;
    }
    
    // Helper: set channel for all 4 slots in a category
    static void SetCategoryChannel(int cat, QuickChatData::Channel ch)
    {
        int baseIdx = cat * 4;
        for (int i = 0; i < 4; i++)
        {
            QuickChatData::slotChannels[baseIdx + i] = ch;
        }
    }
    
    // Helper: get channel from first slot in category
    static QuickChatData::Channel GetCategoryChannel(int cat)
    {
        int baseIdx = cat * 4;
        return QuickChatData::slotChannels[baseIdx];
    }
    
    void Render()
    {
        RefreshBuffersIfNeeded();
        
        ImVec4 green(0.4f, 0.8f, 0.4f, 1.0f);
        bool customChannelPerQC = Header::GetCustomChannelPerQC();
        
        // Configured (24 user)
        const auto& cats = QuickChatData::settingsCategories;
        const char* channelLabels[] = {"M", "T", "P"};

        if (SettingsScanner::IsUnsupportedLanguage())
        {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Settings/Title editing only available in English/Spanish)");
            ImGui::Spacing();
        }
        
        for (int cat = 0; cat < 6; cat++)
        {
            int baseIdx = cat * 4;
            
            // Category channel button (only when OFF)
            if (!customChannelPerQC)
            {
                auto ch = GetCategoryChannel(cat);
                std::string btnId = channelLabels[static_cast<int>(ch)] + std::string("##cat_chan_") + std::to_string(cat);
                if (ImGui::SmallButton(btnId.c_str()))
                {
                    auto newCh = static_cast<QuickChatData::Channel>((static_cast<int>(ch) + 1) % 3);
                    SetCategoryChannel(cat, newCh);
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", TL("channel_cat_tooltip"));
                }
                ImGui::SameLine();
            }
            
            // Settings category name
            if (cat < static_cast<int>(cats.size()) && !cats[cat].text.empty())
            {
                ImGui::TextColored(COLOR_CAT_SETTINGS, "%s", cats[cat].text.c_str());
            }
            else
            {
                ImGui::TextColored(COLOR_CAT_SETTINGS, TL("category_n"), cat + 1);
            }
            
            if (!SettingsScanner::IsUnsupportedLanguage() && cat < static_cast<int>(cats.size()) && !cats[cat].text.empty())
            {
                ImGui::SameLine();
                size_t maxInput = cats[cat].maxChars > 0 ? cats[cat].maxChars : 50;
                ImGui::PushItemWidth(150.0f);
                if (ImGui::InputText(("##settings_custom_" + std::to_string(cat)).c_str(), 
                                      QuickChatData::settingsBuf[cat], maxInput + 1)) // +1 for null
                {
                    QuickChatData::settingsCategories[cat].customText = QuickChatData::settingsBuf[cat];
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    auto& sc = QuickChatData::settingsCategories[cat];
                    if (sc.address != 0 && sc.maxChars > 0)
                    {
                        std::wstring wtext = RLUtils::Utf8ToWide(sc.customText);
                        MemoryUtils::WriteWideString(sc.address, wtext, sc.maxChars);
                        LOG("Wrote settings category {} to memory: '{}' (maxChars={})", cat, sc.customText, sc.maxChars);
                    }
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
                ImGui::PopItemWidth();
            }
            
            ImGui::Spacing();

            // In-game category name
            const auto& inGameCats = QuickChatData::inGameCategories;
            if (cat < static_cast<int>(inGameCats.size()) && !inGameCats[cat].text.empty())
            {
                TextWithGlow(COLOR_CAT_INGAME, COLOR_CAT_GLOW, 0.6f, inGameCats[cat].text.c_str());
                ImGui::SameLine();
                
                ImGui::PushItemWidth(150.0f);
                if (ImGui::InputText(("##ingame_custom_" + std::to_string(cat)).c_str(), 
                                      inGameBuf[cat], 128))
                {
                    QuickChatData::inGameCategories[cat].customText = inGameBuf[cat];
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Playground::UpdateAllProcessedText();
                    RefreshSettings::RefreshDelayed();
                    InGameReplacer::ForceRefresh();
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
                ImGui::PopItemWidth();
            }
            

            
            // 4 QCs in this category
            for (int i = 0; i < 4; i++)
            {
                int idx = baseIdx + i;
                auto* qc = QuickChatData::userBindings[idx];
                if (!qc) continue;
                
                const std::string& thisText = qc->text;
                
                // Check for duplicate
                int firstIdx = -1;
                for (int k = 0; k < 24; k++)
                {
                    auto* other = QuickChatData::userBindings[k];
                    if (other && other->text == thisText)
                    {
                        firstIdx = k;
                        break;
                    }
                }
                
                bool isFirst = (firstIdx == idx);
                bool isDuplicate = QuickChatData::IsDuplicate(thisText);
                bool showAsRef = isDuplicate && !isFirst;
                
                if (!showAsRef)
                {
                    // Individual channel button (only when checkbox is ON)
                    if (customChannelPerQC)
                    {
                        auto& slotCh = QuickChatData::slotChannels[idx];
                        if (ImGui::SmallButton((channelLabels[static_cast<int>(slotCh)] + std::string("##ch") + std::to_string(idx)).c_str()))
                        {
                            slotCh = static_cast<QuickChatData::Channel>((static_cast<int>(slotCh) + 1) % 3);
                        Persistance::SaveProfile(Profile::GetCurrentProfileName());
                        }
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("%s", TL("channel_slot_tooltip"));
                        }
                        ImGui::SameLine();
                    }
                    
                    ImGui::Text("[%d] %s", i + 1, thisText.c_str());
                    ImGui::SameLine();
                    
                    ImGui::PushItemWidth(200.0f);
                    if (ImGui::InputText(("##input_user_" + std::to_string(idx)).c_str(), 
                                         userInputBuffers[idx], 256,
                                         ImGuiInputTextFlags_CallbackAlways, SeparatorCallback))
                    {
                        qc->customText = userInputBuffers[idx];
                        
                        // Sync to CustomUI slot custom text (right panel input)
                        if (idx < 20)
                        {
                            strncpy_s(QuickChatCustomUI::Settings::quickChatSlots[idx].customText,
                                      sizeof(QuickChatCustomUI::Settings::quickChatSlots[idx].customText),
                                      userInputBuffers[idx], _TRUNCATE);
                        }
                        
                        // Sync other duplicate userBindings buffers
                        for (int k = 0; k < 24; k++)
                        {
                            auto* other = QuickChatData::userBindings[k];
                            if (k != idx && other && other->text == thisText)
                            {
                                strncpy_s(userInputBuffers[k], userInputBuffers[idx], 255);
                                if (k < 20)
                                {
                                    strncpy_s(QuickChatCustomUI::Settings::quickChatSlots[k].customText,
                                              sizeof(QuickChatCustomUI::Settings::quickChatSlots[k].customText),
                                              userInputBuffers[idx], _TRUNCATE);
                                }
                            }
                        }
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        Playground::UpdateAllProcessedText();
                        RefreshSettings::RefreshDelayed();
                        InGameReplacer::ForceRefresh();
                        Persistance::SaveProfile(Profile::GetCurrentProfileName());
                    }
                    ImGui::PopItemWidth();
                }
                else
                {
                    auto* firstQc = QuickChatData::userBindings[firstIdx];
                    const std::string& displayText = firstQc->customText.empty() ? thisText : firstQc->customText;
                    ImGui::TextDisabled("[%d] %s <- %s", i + 1, thisText.c_str(), displayText.c_str());
                }
            }
            
            ImGui::Spacing();
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Available
        ImGui::TextColored(green, "%s", TL("available"));
        ImGui::Spacing();
        
        // Resize buffers if needed
        if (availableInputBuffers.size() != QuickChatData::availableChats.size())
        {
            size_t oldSize = availableInputBuffers.size();
            availableInputBuffers.resize(QuickChatData::availableChats.size());
            
            for (size_t i = oldSize; i < availableInputBuffers.size(); i++)
            {
                auto* qc = QuickChatData::availableChats[i];
                if (qc)
                {
                    strncpy_s(availableInputBuffers[i].data(), 256, qc->customText.c_str(), 255);
                }
            }
        }
        
        for (size_t i = 0; i < QuickChatData::availableChats.size(); i++)
        {
            auto* qc = QuickChatData::availableChats[i];
            if (!qc) continue;
            
            // No channel button for available - they don't have assigned slots
            
            ImGui::Text("%s", qc->text.c_str());
            ImGui::SameLine();
            
            ImGui::PushItemWidth(200.0f);
            if (ImGui::InputText(("##input_avail_" + std::to_string(i)).c_str(), 
                                 availableInputBuffers[i].data(), 256))
            {
                qc->customText = availableInputBuffers[i].data();
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                Playground::UpdateAllProcessedText();
                RefreshSettings::RefreshDelayed();
                InGameReplacer::ForceRefresh();
                Persistance::SaveProfile(Profile::GetCurrentProfileName());
            }
            ImGui::PopItemWidth();
        }
    }
}

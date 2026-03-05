#include "pch.h"
#include "Playground.h"
#include "../../IMGUI/imgui_internal.h"
#include "../../RL/Data/MatchData.h"
#include "../../RL/QuickChatData.h"
#include "../Profile.h"
#include "../../Persistance/Persistance.h"
#include "../../Utils/Localization.h"
#include "../../QuickChatCustomUI/UI/Overlay/Settings.h"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <map>

namespace Playground
{
    static const ImVec4 COLOR_TITLE = ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
    static const ImVec4 COLOR_CAT = ImVec4(0.7f, 0.9f, 1.0f, 1.0f);

    static int selectedKeyIndex = -1;
    static char newKeyNameBuf[64] = "";
    static char editContentBuf[1024] = "";

    // Token info for UI display
    struct TokenInfo
    {
        const char* category;
        const char* token;
        const char* description;
    };

    static const TokenInfo matchTokens[] = {
        {"Player", "[mynickname]", "desc_mynickname"},
        {"Player", "[myscore]", "desc_myscore"},
        {"Player", "[mate1]", "desc_mate_name"},
        {"Player", "[mate1s]", "desc_mate_score"},
        {"Player", "[mate2]", "desc_mate_name"},
        {"Player", "[mate2s]", "desc_mate_score"},
        {"Player", "[mate3]", "desc_mate_name"},
        {"Player", "[mate3s]", "desc_mate_score"},
        {"Opponent", "[opp1]", "desc_opp_name"},
        {"Opponent", "[opp1s]", "desc_opp_score"},
        {"Opponent", "[opp2]", "desc_opp_name"},
        {"Opponent", "[opp2s]", "desc_opp_score"},
        {"Opponent", "[opp3]", "desc_opp_name"},
        {"Opponent", "[opp3s]", "desc_opp_score"},
        {"Opponent", "[opp4]", "desc_opp_name"},
        {"Opponent", "[opp4s]", "desc_opp_score"},
        {"Events", "[lastgoalby]", "desc_lastgoalby"},
        {"Events", "[lastgoalspeed]", "desc_lastgoalspeed"},
        {"Events", "[lastsaveby]", "desc_lastsaveby"},
        {"Events", "[goaldiff]", "desc_goaldiff"},
        {"Chat", "[lastmessage]", "desc_lastmessage"},
        {"Chat", "[lastplayername]", "desc_lastplayername"},
        {"Time", "[remaining]", "desc_remaining"},
        {"Time", "[overtime]", "desc_overtime"},
        {"Time", "[wait:X]", "desc_wait"},
        {"Transform", "[upper]", "desc_upper"},
        {"Transform", "[lower]", "desc_lower"},
        {"Transform", "[randomize]", "desc_randomize"}
    };
    
    // Callback to insert § every 128 chars per line
    static constexpr const char* SEP = "\xC2\xA7";  // § in UTF-8
    
    static int KeySeparatorCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
        {
            std::string input(data->Buf, data->BufTextLen);
            
            // Process each line separately
            std::string result;
            size_t lineStart = 0;
            
            for (size_t pos = 0; pos <= input.size(); pos++)
            {
                bool isEnd = (pos == input.size());
                bool isNewline = (!isEnd && input[pos] == '\n');
                
                if (isEnd || isNewline)
                {
                    std::string line = input.substr(lineStart, pos - lineStart);
                    
                    // Remove all § from this line
                    std::string clean;
                    for (size_t i = 0; i < line.size(); i++)
                    {
                        if (i + 1 < line.size() && 
                            (unsigned char)line[i] == 0xC2 && 
                            (unsigned char)line[i+1] == 0xA7)
                        {
                            i++;
                            continue;
                        }
                        clean += line[i];
                    }
                    
                    // Add § every 128 chars
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
                    
                    result += withSep;
                    if (isNewline) result += '\n';
                    lineStart = pos + 1;
                }
            }
            
            if (input != result)
            {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, result.c_str());
            }
        }
        return 0;
    }

    void Render()
    {
        if (!showWindow) return;
        
        ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
        
        if (!ImGui::Begin(TL("playground"), &showWindow))
        { 
            ImGui::End();
            return;
        }
        
        // Calculate heights
        float totalHeight = ImGui::GetContentRegionAvail().y;
        float splitterH = 8.0f;
        static float keysHeight = 200.0f;
        float tokensHeight = totalHeight - keysHeight - splitterH;
        if (tokensHeight < 100.0f) tokensHeight = 100.0f;
        
        // ==================== USER KEYS SECTION ====================
        ImGui::BeginChild("##keysSection", ImVec2(0, keysHeight), true);
        
        ImGui::TextColored(COLOR_TITLE, "%s", TL("user_keys"));
        
        // Create + Input (Enter also creates)
        bool createPressed = ImGui::Button(TL("create"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        bool enterPressed = ImGui::InputText("##createInput", newKeyNameBuf, sizeof(newKeyNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
        
        if ((createPressed || enterPressed) && newKeyNameBuf[0] != '\0')
        {
            userKeys.push_back({newKeyNameBuf, ""});
            selectedKeyIndex = static_cast<int>(userKeys.size()) - 1;
            editContentBuf[0] = '\0';
            newKeyNameBuf[0] = '\0';
            Persistance::SaveProfile(Profile::GetCurrentProfileName());
        }
        
        if (!userKeys.empty())
        {
            // Left panel: Copy + list
            ImGui::BeginChild("##keysLeft", ImVec2(160, 0), false);
            
            if (ImGui::Button(TL("copy")) && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(userKeys.size()))
            {
                std::string toCopy = "{" + userKeys[selectedKeyIndex].name + "}";
                ImGui::SetClipboardText(toCopy.c_str());
            }
            ImGui::SameLine();
            if (ImGui::Button(TL("delete")) && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(userKeys.size()))
            {
                userKeys.erase(userKeys.begin() + selectedKeyIndex);
                selectedKeyIndex = std::min(selectedKeyIndex, static_cast<int>(userKeys.size()) - 1);
                if (selectedKeyIndex >= 0)
                {
                    strncpy_s(editContentBuf, userKeys[selectedKeyIndex].content.c_str(), sizeof(editContentBuf));
                }
                Persistance::SaveProfile(Profile::GetCurrentProfileName());
            }
            
            ImGui::Separator();
            
            for (int i = 0; i < static_cast<int>(userKeys.size()); i++)
            {
                std::string label = "{" + userKeys[i].name + "}";
                if (ImGui::Selectable(label.c_str(), selectedKeyIndex == i))
                {
                    selectedKeyIndex = i;
                    strncpy_s(editContentBuf, userKeys[i].content.c_str(), sizeof(editContentBuf));
                }
            }
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            // Vertical separator
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(p.x, p.y),
                ImVec2(p.x, p.y + ImGui::GetContentRegionAvail().y),
                IM_COL32(100, 100, 100, 255), 1.0f);
            ImGui::Dummy(ImVec2(5, 0));
            ImGui::SameLine();
            
            // Right panel: edit content
            ImGui::BeginChild("##keysRight", ImVec2(0, 0), false);
            if (selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(userKeys.size()))
            {
                ImGui::Checkbox(TL("shuffle"), &userKeys[selectedKeyIndex].shuffle);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
                
                ImVec2 availSize = ImGui::GetContentRegionAvail();
                if (ImGui::InputTextMultiline("##editContent", editContentBuf, sizeof(editContentBuf), availSize,
                                              ImGuiInputTextFlags_CallbackAlways, KeySeparatorCallback))
                {
                    userKeys[selectedKeyIndex].content = editContentBuf;
                }
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
            }
            ImGui::EndChild();
        }
        
        ImGui::EndChild();
        
        // ==================== HORIZONTAL SPLITTER ====================
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            ImRect bb(ImVec2(p.x, p.y), ImVec2(p.x + width, p.y + splitterH));
            
            ImGui::SplitterBehavior(bb, ImGui::GetID("##hSplitter"), ImGuiAxis_Y,
                &keysHeight, &tokensHeight, 80.0f, 80.0f, 0.f);
            
            bool hovered = ImGui::IsItemHovered() || ImGui::IsItemActive();
            ImU32 col = hovered ? IM_COL32(80, 150, 220, 255) : IM_COL32(90, 90, 90, 200);
            ImGui::GetWindowDrawList()->AddRectFilled(bb.Min, bb.Max, col, 4.0f);
            
            if (hovered)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            
            ImGui::Dummy(ImVec2(0, splitterH));
        }
        
        // ==================== MATCH DATA TOKENS SECTION ====================
        ImGui::BeginChild("##tokensSection", ImVec2(0, 0), true);
        
        ImGui::TextColored(COLOR_TITLE, "%s", TL("match_data_tokens"));
        
        // Copy button
        static int selectedTokenIndex = -1;
        if (ImGui::Button(TL("copy")) && selectedTokenIndex >= 0)
        {
            if (strcmp(matchTokens[selectedTokenIndex].token, "[wait:X]") == 0)
            {
                char waitBuf[32];
                snprintf(waitBuf, sizeof(waitBuf), "[wait:%.0f]", waitSeconds);
                ImGui::SetClipboardText(waitBuf);
            }
            else
            {
                ImGui::SetClipboardText(matchTokens[selectedTokenIndex].token);
            }
        }
        
        ImGui::Separator();
        
        // Left panel: categorized list
        ImGui::BeginChild("##tokensLeft", ImVec2(180, 0), false);
        
        const char* lastCategory = nullptr;
        for (int i = 0; i < static_cast<int>(sizeof(matchTokens) / sizeof(matchTokens[0])); i++)
        {
            if (lastCategory == nullptr || strcmp(matchTokens[i].category, lastCategory) != 0)
            {
                if (lastCategory != nullptr) ImGui::Dummy(ImVec2(0, 3));
                ImGui::TextColored(COLOR_CAT, "%s:", matchTokens[i].category);
                lastCategory = matchTokens[i].category;
            }
            
            ImGui::Indent(10);
            
            // For [wait:X], show the configured value
            const char* displayToken = matchTokens[i].token;
            char waitTokenBuf[32];
            if (strcmp(matchTokens[i].token, "[wait:X]") == 0)
            {
                snprintf(waitTokenBuf, sizeof(waitTokenBuf), "[wait:%.0f]", waitSeconds);
                displayToken = waitTokenBuf;
            }
            
            if (ImGui::Selectable(displayToken, selectedTokenIndex == i))
            {
                selectedTokenIndex = i;
            }
            ImGui::Unindent(10);
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        // Vertical separator
        ImVec2 p2 = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(p2.x, p2.y),
            ImVec2(p2.x, p2.y + ImGui::GetContentRegionAvail().y),
            IM_COL32(100, 100, 100, 255), 1.0f);
        ImGui::Dummy(ImVec2(5, 0));
        ImGui::SameLine();
        
        // Right panel: description + controls for Transform tokens
        ImGui::BeginChild("##tokensRight", ImVec2(0, 0), false);
        if (selectedTokenIndex >= 0)
        {
            ImGui::TextWrapped("%s", TL(matchTokens[selectedTokenIndex].description));
            ImGui::Spacing();
            
            // Special controls for Transform tokens
            const char* token = matchTokens[selectedTokenIndex].token;
            
            if (strcmp(token, "[randomize]") == 0)
            {
                ImGui::Text("%s", TL("amount"));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                ImGui::SliderFloat("##randomizeSlider", &randomizeAmount, 0.0f, 100.0f, "%.0f%%");
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
            }
            else if (strcmp(token, "[wait:X]") == 0)
            {
                ImGui::Text("%s", TL("seconds"));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::InputFloat("##waitSeconds", &waitSeconds, 1.0f, 10.0f, "%.0f");
                if (waitSeconds < 0.1f) waitSeconds = 0.1f;
                if (waitSeconds > 3600.0f) waitSeconds = 3600.0f;
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
            }
        }
        ImGui::EndChild();
        
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    // ==================== ProcessText ====================
    std::string ProcessText(const std::string& text, bool forSending, const std::string& fallbackText)
    {
        if (text.empty()) return text;
        
        // Do not call RefreshMyScore here - this runs on render thread
        
        // Remove § separator (visual feedback only, not sent)
        // § in UTF-8 = 0xC2 0xA7 (2 bytes)
        std::string result;
        for (size_t i = 0; i < text.size(); i++)
        {
            if (i + 1 < text.size() && 
                (unsigned char)text[i] == 0xC2 && 
                (unsigned char)text[i+1] == 0xA7)
            {
                i++;  // Skip both bytes
                continue;
            }
            result += text[i];
        }
        
        // Replace [wait:X] with ·Xs· for visual feedback
        size_t waitPos = result.find("[wait:");
        while (waitPos != std::string::npos)
        {
            size_t endPos = result.find(']', waitPos);
            if (endPos != std::string::npos)
            {
                // Extract the number
                std::string numStr = result.substr(waitPos + 6, endPos - waitPos - 6);
                std::string replacement = " \xC2\xB7" + numStr + "s\xC2\xB7 ";  // ·Xs· with spaces
                result.replace(waitPos, endPos - waitPos + 1, replacement);
            }
            else
            {
                break;
            }
            waitPos = result.find("[wait:", waitPos + 1);
        }
        
        // --- Replace MatchData tokens ---
        auto replaceToken = [&result](const std::string& token, const std::string& value) {
            size_t pos = result.find(token);
            while (pos != std::string::npos)
            {
                result.replace(pos, token.length(), value);
                pos = result.find(token, pos + value.length());
            }
        };
        
        // Player tokens
        replaceToken("[mynickname]", MatchData::me.name);
        replaceToken("[myscore]", std::to_string(MatchData::me.score));
        
        // Teammates
        for (int i = 0; i < 3; i++)
        {
            std::string idx = std::to_string(i + 1);
            replaceToken("[mate" + idx + "]", i < MatchData::teammateCount ? MatchData::teammates[i].name : "");
            replaceToken("[mate" + idx + "s]", i < MatchData::teammateCount ? std::to_string(MatchData::teammates[i].score) : "");
        }
        
        // Opponents
        for (int i = 0; i < 4; i++)
        {
            std::string idx = std::to_string(i + 1);
            replaceToken("[opp" + idx + "]", i < MatchData::opponentCount ? MatchData::opponents[i].name : "");
            replaceToken("[opp" + idx + "s]", i < MatchData::opponentCount ? std::to_string(MatchData::opponents[i].score) : "");
        }
        
        // Events
        replaceToken("[lastgoalby]", MatchData::lastGoalBy);
        replaceToken("[lastgoalspeed]", MatchData::lastGoalSpeed);
        replaceToken("[lastsaveby]", MatchData::lastSaveBy);
        replaceToken("[goaldiff]", MatchData::goalDiff);
        replaceToken("[lastmessage]", MatchData::lastMessage);
        replaceToken("[lastplayername]", MatchData::lastPlayerName);
        
        // Time
        replaceToken("[remaining]", MatchData::GetTimeRemaining());
        replaceToken("[overtime]", MatchData::GetOvertimeTime());
        
        // --- Replace User Keys ---
        for (auto& key : userKeys)
        {
            std::string token = "{" + key.name + "}";
            size_t pos = result.find(token);
            if (pos != std::string::npos)
            {
                // Split content into lines
                std::vector<std::string> lines;
                std::istringstream stream(key.content);
                std::string line;
                while (std::getline(stream, line))
                {
                    if (!line.empty())
                    {
                        std::string clean;
                        for (size_t ci = 0; ci < line.size(); ci++)
                        {
                            if (ci + 1 < line.size() &&
                                (unsigned char)line[ci] == 0xC2 &&
                                (unsigned char)line[ci+1] == 0xA7)
                            {
                                ci++;
                                continue;
                            }
                            clean += line[ci];
                        }
                        if (!clean.empty())
                            lines.push_back(clean);
                    }
                }
                
                std::string selectedLine;
                if (lines.empty())
                {
                    selectedLine = "";
                }
                else if (lines.size() == 1)
                {
                    selectedLine = lines[0];
                }
                else
                {
                    // Multiple lines - pick one
                    if (key.shuffle)
                    {
                        // Random selection
                        selectedLine = lines[std::rand() % lines.size()];
                    }
                    else
                    {
                        static std::map<std::string, size_t> keyIndices;
                        size_t& idx = keyIndices[key.name];
                        selectedLine = lines[idx % lines.size()];
                        if (forSending) idx++;
                    }
                }
                
                // Replace all occurrences of this token
                while (pos != std::string::npos)
                {
                    result.replace(pos, token.length(), selectedLine);
                    pos = result.find(token, pos + selectedLine.length());
                }
            }
        }
        
        // --- Apply Transforms ---
        // [upper]
        if (result.find("[upper]") != std::string::npos)
        {
            replaceToken("[upper]", "");
            if (result.empty() && !fallbackText.empty()) result = fallbackText;
            for (char& c : result) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        
        // [lower]
        if (result.find("[lower]") != std::string::npos)
        {
            replaceToken("[lower]", "");
            if (result.empty() && !fallbackText.empty()) result = fallbackText;
            for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        
        // [randomize]
        if (result.find("[randomize]") != std::string::npos)
        {
            replaceToken("[randomize]", "");
            if (result.empty() && !fallbackText.empty()) result = fallbackText;
            for (char& c : result)
            {
                if (std::isalpha(static_cast<unsigned char>(c)))
                {
                    if ((std::rand() % 100) < static_cast<int>(randomizeAmount))
                    {
                        c = (std::rand() % 2) ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                                              : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                }
            }
        }
        

        
        return result;
    }
    
    // ==================== UpdateAllProcessedText ====================
    void UpdateAllProcessedText()
    {
        int processed = 0;
        for (auto& qc : QuickChatData::allChats)
        {
            if (!qc.customText.empty())
            {
                qc.processedText = ProcessText(qc.customText, false, qc.text);
                LOG("Processed: ptr={} customText='{}' -> processedText='{}'", (void*)&qc, qc.customText, qc.processedText);
                processed++;
            }
            else
            {
                qc.processedText = qc.text;
            }
        }
        LOG("UpdateAllProcessedText: {} QCs with customText processed", processed);
        
        QuickChatCustomUI::Settings::SyncSlotBuffers();
        
        if (!QuickChatCustomUI::Settings::overlayFrozen)
        {
            for (auto& textObj : QuickChatCustomUI::Settings::textObjects)
            {
                textObj.processedContent = (textObj.content[0] != '\0')
                    ? ProcessText(textObj.content) : std::string();
            }
            
            for (auto& slot : QuickChatCustomUI::Settings::quickChatSlots)
            {
                slot.processedContent = (slot.customText[0] != '\0')
                    ? ProcessText(slot.customText) : std::string();
            }
        }
    }
}

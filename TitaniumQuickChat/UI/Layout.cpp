#include "pch.h"
#include "Layout.h"
#include "Profile.h"
#include "Modes/Normal.h"
#include "Modes/Simple.h"
#include "Modes/Playground.h"
#include "Header.h"
#include "../QuickChatCustomUI/QuickChatCustomUI.h"
#include "../Utils/Localization.h"
#include "../IMGUI/imgui_internal.h"
#include "../IMGUI/imgui_stdlib.h"
#include "../RL/MemoryScanner.h"
#include "../RL/Replacer/TitleReplacer.h"
#include "../RL/QuickChatData.h"
#include "../Persistance/Persistance.h"

namespace Layout
{
    // State
    static float profWidth = 220.f;
    static float qcWidth = 0.f;
    static bool initialized = false;
    
    void Render()
    {
        // Dimensions
        float totalW = ImGui::GetContentRegionAvail().x;
        float totalH = ImGui::GetContentRegionAvail().y;
        float titleH = ImGui::GetTextLineHeightWithSpacing();
        float bottomReserve = ImGui::GetTextLineHeightWithSpacing() * 2.5f;
        float panelH = totalH - titleH - bottomReserve;
        constexpr float splitterW = 8.0f;
        
        // Min
        constexpr float profMinW = 220.f;
        constexpr float qcMinW = 250.f;
        
        // Ini
        if (!initialized)
        {
            qcWidth = totalW - profWidth - splitterW;
            initialized = true;
        }
        
        // Clamp values
        profWidth = std::max(profWidth, profMinW);
        qcWidth = std::max(qcWidth, qcMinW);
        
        ImVec2 startPos = ImGui::GetCursorScreenPos();
        float panelY = startPos.y + titleH;
        
        // Titles
        ImVec4 green(0.4f, 0.8f, 0.4f, 1.0f);
        
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y));
        ImGui::TextColored(green, "%s", TL("tab_quickchats"));
        
        float profX = startPos.x + qcWidth + splitterW;
        ImGui::SetCursorScreenPos(ImVec2(profX + 4, startPos.y));
        ImGui::TextColored(green, "%s", TL("tab_profiles"));
        
        // Splitter
        float s1X = startPos.x + qcWidth;
        ImRect bb1(ImVec2(s1X, panelY), ImVec2(s1X + splitterW, panelY + panelH));
        ImGui::SplitterBehavior(bb1, ImGui::GetID("##s1"), ImGuiAxis_X, 
            &qcWidth, &profWidth, qcMinW, profMinW, 0.f);
        
        bool splitterActive = ImGui::IsItemHovered() || ImGui::IsItemActive();
        
        // Panel QC
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, panelY));
        ImGui::BeginChild("QC", ImVec2(qcWidth, panelH), false);

        // Quick Chat Title input 
        ImGui::Text("%s", TL("title_label"));
        ImGui::SameLine();
        
        if (!SettingsScanner::IsUnsupportedLanguage())
        {
            ImGui::SetNextItemWidth(100);
            
            // Use logic from centralized data
            size_t maxLen = QuickChatData::titleData.maxWriteLen;
            static char titleBuffer[13] = {};
            
            // Sync buffer from centralized data
            strncpy_s(titleBuffer, QuickChatData::titleData.customText.c_str(), maxLen);
            
            if (ImGui::InputText("##qcTitle", titleBuffer, maxLen + 1))
            {
                 TitleReplacer::SetTitle(titleBuffer);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                Persistance::SaveProfile(Profile::GetCurrentProfileName());
            }
        }
        else
        {
            ImGui::TextDisabled("------------");
        }

        
        ImGui::Spacing();

        // Open Playground / Custom UI buttons 
        if (ImGui::Button(TL("open_playground")))
        {
            Playground::showWindow = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(TL("open_custom_ui")))
        {
            QuickChatCustomUI::showWindow = true;
        }
        ImGui::Spacing();
        
        if (Header::GetViewMode() == 0) // Normal
        {
            NormalMode::Render();
        }
        else // Simple
        {
            SimpleMode::Render();
        }
        
        ImGui::EndChild();
        
        // Draw splitter
        ImU32 col = splitterActive 
            ? IM_COL32(80, 150, 220, 255) 
            : IM_COL32(90, 90, 90, 200);
        ImGui::GetWindowDrawList()->AddRectFilled(bb1.Min, bb1.Max, col, 4.0f);
        
        if (splitterActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        
        // Panel Profiles
        ImGui::SetCursorScreenPos(ImVec2(profX + 4, panelY));
        ImGui::BeginChild("Profiles", ImVec2(profWidth - 4.f, panelH), false);
        Profile::Render();
        ImGui::EndChild();
    }
}


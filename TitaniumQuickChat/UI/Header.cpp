#include "pch.h"
#include "Header.h"
#include "Profile.h"
#include "../RL/HandleKeyPress.h"
#include "../RL/RefreshSettings.h"
#include "../RL/Replacer/InGameReplacer.h"
#include "../RL/QuickChatData.h"
#include "../QuickChatCustomUI/UI/Overlay/Settings.h"
#include "../Persistance/Persistance.h"
#include "../Utils/Localization.h"

namespace Header
{
    static int viewMode = 0; // 0=Normal, 1=Simple
    static int bindMode = 0; // 0=Combine, 1=Progressive, 2=Sequence
    static float comboTiming = 0.2f;
    static float progressiveTiming = 0.15f;
    static bool customChannelPerQC = false;
    static bool showMatchNotif = true;

    void Render()
    {
        // Discord link
        ImVec4 discordColor = ImVec4(0.30f, 0.85f, 1.0f, 1.0f); 
        ImGui::TextColored(discordColor, TL("join_discord"));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), ImGui::GetColorU32(discordColor), 1.0f);
        }
        if (ImGui::IsItemClicked())
        {
            ShellExecuteA(nullptr, "open", "https://discord.gg/FPvkjaBPEA", nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::SameLine();
        ImGui::Text("·");
        ImGui::SameLine();
        ImGui::Text("Made by The Ankabut");
        ImGui::Spacing();

        //  Bind mode controls 
        if (ImGui::RadioButton(TL("combine"), &bindMode, 0))
        {
            HandleKeyPress::SetBindMode(HandleKeyPress::BindMode::Combine);
            Persistance::SaveGlobalSettings();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderFloat("##comboTiming", &comboTiming, 0.1f, 0.5f, "%.2fs"))
        {
            HandleKeyPress::SetComboTiming(comboTiming);
            Persistance::SaveGlobalSettings();
        }
        
        if (ImGui::RadioButton(TL("progressive"), &bindMode, 1))
        {
            HandleKeyPress::SetBindMode(HandleKeyPress::BindMode::Progressive);
            Persistance::SaveGlobalSettings();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::SliderFloat("##progressiveTiming", &progressiveTiming, 0.5f, 10.0f, "%.1fs"))
        {
            HandleKeyPress::SetProgressiveTiming(progressiveTiming);
            Persistance::SaveGlobalSettings();
        }
        
        if (ImGui::RadioButton(TL("sequence"), &bindMode, 2))
        {
            HandleKeyPress::SetBindMode(HandleKeyPress::BindMode::Sequence);
            Persistance::SaveGlobalSettings();
        }
        
        ImGui::Spacing();
        
        // View mode
        ImGui::Text("%s", TL("mode"));
        ImGui::SameLine();
        
        int prevMode = viewMode;
        ImGui::RadioButton(TL("normal"), &viewMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton(TL("simple"), &viewMode, 1);
        
        if (viewMode != prevMode)
        {
            QuickChatCustomUI::Settings::cachedViewMode = viewMode;
            
            // Sync CustomUI slot texts based on mode
            for (int i = 0; i < 20 && i < 24; i++)
            {
                auto* qc = QuickChatData::userBindings[i];
                if (!qc) continue;
                
                if (viewMode == 1) // Switched to Simple: use original QC text
                {
                    strncpy_s(QuickChatCustomUI::Settings::quickChatSlots[i].customText,
                              sizeof(QuickChatCustomUI::Settings::quickChatSlots[i].customText),
                              qc->text.c_str(), _TRUNCATE);
                }
                else // Switched to Normal: restore custom text
                {
                    strncpy_s(QuickChatCustomUI::Settings::quickChatSlots[i].customText,
                              sizeof(QuickChatCustomUI::Settings::quickChatSlots[i].customText),
                              qc->customText.c_str(), _TRUNCATE);
                }
            }
            
            RefreshSettings::RefreshDelayed();
            InGameReplacer::MarkDirty();
            InGameReplacer::ForceRefreshDelayed();
            Persistance::SaveProfile(Profile::GetCurrentProfileName());
        }
        
        // Custom channel checkbox
        if (ImGui::Checkbox(TL("custom_channel_per_qc"), &customChannelPerQC))
        {
            // When disabling per-QC channels, apply category global to all 4 slots
            if (!customChannelPerQC)
            {
                for (int cat = 0; cat < 6; cat++)
                {
                    int baseIdx = cat * 4;
                    auto globalCh = QuickChatData::slotChannels[baseIdx];
                    for (int i = 1; i < 4; i++)
                    {
                        QuickChatData::slotChannels[baseIdx + i] = globalCh;
                    }
                }
            }
            Persistance::SaveGlobalSettings();
            Persistance::SaveProfile(Profile::GetCurrentProfileName());
        }
    }

    int GetViewMode() { return viewMode; }
    void SetViewMode(int mode) { viewMode = mode; QuickChatCustomUI::Settings::cachedViewMode = mode; }
    
    int GetBindMode() { return bindMode; }
    void SetBindMode(int mode)
    {
        bindMode = mode;
        HandleKeyPress::SetBindMode(static_cast<HandleKeyPress::BindMode>(mode));
    }
    
    float GetComboTiming() { return comboTiming; }
    void SetComboTiming(float timing)
    {
        comboTiming = timing;
        HandleKeyPress::SetComboTiming(timing);
    }
    
    float GetProgressiveTiming() { return progressiveTiming; }
    void SetProgressiveTiming(float timing)
    {
        progressiveTiming = timing;
        HandleKeyPress::SetProgressiveTiming(timing);
    }
    
    bool GetCustomChannelPerQC() { return customChannelPerQC; }
    void SetCustomChannelPerQC(bool value) { customChannelPerQC = value; }

    bool GetShowMatchNotif() { return showMatchNotif; }
    void SetShowMatchNotif(bool value) { showMatchNotif = value; }
}

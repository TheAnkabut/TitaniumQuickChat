#include "pch.h"
#include "Simple.h"
#include "../Header.h"
#include "../Profile.h"
#include "../../RL/QuickChatData.h"
#include "../../RL/QuickChatSave.h"
#include "../../RL/MemoryScanner.h"
#include "../../Persistance/Persistance.h"
#include "../../imgui/imgui_searchablecombo.h"
#include "../../Utils/Localization.h"
#include "../../Utils/RLUtils.h"

namespace SimpleMode
{
    
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
        ImVec4 COLOR_CAT_SETTINGS = {0.7f, 0.9f, 1.0f, 1.0f};
        ImVec4 COLOR_GRAY         = {0.75f, 0.75f, 0.75f, 1.0f};
        const char* channelLabels[] = {"M", "T", "P"};

        const auto& cats = QuickChatData::settingsCategories;

        // Vector of strings for ImGui::SearchableCombo
        static std::vector<std::string> comboItems;
        static bool itemsInitialized = false;

        if (!itemsInitialized || comboItems.size() != QuickChatData::allChats.size())
        {
            comboItems.clear();
            for (const auto& qc : QuickChatData::allChats)
            {
                comboItems.push_back(qc.text);
            }
            itemsInitialized = true;
        }
        
        // Checkbox value from Header
        bool customChannelPerQC = Header::GetCustomChannelPerQC();
        
        // Calculate width once for all categories
        float availW = ImGui::GetContentRegionAvail().x;
        constexpr float gapFromSplitter = 50.0f;
        float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
        float dropW = (availW - (itemSpacing * 3) - 60 - gapFromSplitter) / 4.0f;
        if (dropW < 80.0f) dropW = 80.0f;

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
                std::string btnId = channelLabels[static_cast<int>(ch)] + std::string("##cat_chan_simple_") + std::to_string(cat);
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

            // Category header
            ImGui::TextColored(COLOR_GRAY, "[%d]", cat + 1);
            ImGui::SameLine();
            
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
                size_t maxInput = cats[cat].maxChars > 1 ? cats[cat].maxChars - 1 : 50;
                if (maxInput > 127) maxInput = 127;
                ImGui::PushItemWidth(150.0f);
                if (ImGui::InputText(("##settings_simple_" + std::to_string(cat)).c_str(), QuickChatData::settingsBuf[cat], maxInput + 1))
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
                    }
                    Persistance::SaveProfile(Profile::GetCurrentProfileName());
                }
                ImGui::PopItemWidth();
            }

            ImGui::Spacing();

            // 4 slots (Horizontal)
            for (int i = 0; i < 4; i++)
            {
                int idx = baseIdx + i;
                if (idx >= 24) break;

                ImGui::PushID(idx);
                
                auto* qc = QuickChatData::userBindings[idx];
                
                // Individual channel button (only when checkbox is ON)
                if (customChannelPerQC)
                {
                    auto& slotCh = QuickChatData::slotChannels[idx];
                    if (ImGui::SmallButton(channelLabels[static_cast<int>(slotCh)]))
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
                else
                {
                    // Slot number (only when checkbox is OFF)
                    ImGui::TextColored(COLOR_GRAY, "[%d]", i + 1);
                    ImGui::SameLine();
                }
                
                // Find current selection index
                int currentItem = -1;
                std::string currentText = qc ? qc->text : "";
                
                for (int k = 0; k < comboItems.size(); k++)
                {
                    if (currentText == comboItems[k])
                    {
                        currentItem = k;
                        break;
                    }
                }

                // Check for duplicate
                bool isDup = QuickChatData::IsDuplicate(currentText);
                if (isDup)
                {
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
                }

                ImGui::SetNextItemWidth(dropW);
                std::string previewText = currentItem >= 0 ? comboItems[currentItem] : TL("select");
                if (ImGui::SearchableCombo("##combo", &currentItem, comboItems, previewText.c_str(), TL("search")))
                {
                    if (currentItem >= 0 && currentItem < QuickChatData::allChats.size())
                    {
                        QuickChatData::userBindings[idx] = &QuickChatData::allChats[currentItem];
                        QuickChatData::buffersDirty = true;
                        LOG("Simple: Changed slot {} to '{}' fname={}", idx, QuickChatData::userBindings[idx]->text, QuickChatData::userBindings[idx]->id.ToString());
                        QuickChatSave::SaveDelayed(idx, QuickChatData::userBindings[idx]->id.ToString());
                        Persistance::SaveProfile(Profile::GetCurrentProfileName());
                    }
                }

                if (isDup)
                {
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }

                ImGui::PopID();
                
                if (i < 3) ImGui::SameLine();
            }
            
            ImGui::Spacing();
            ImGui::Spacing();
        }
    }
}

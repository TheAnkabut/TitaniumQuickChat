#include "pch.h"
#include "Profile.h"
#include "Header.h"
#include "Welcome.h"
#include "../RL/HandleKeyPress.h"
#include "../RL/Notification.h"
#include "../RL/QuickChatData.h"
#include "../RL/RefreshSettings.h"
#include "../RL/Replacer/InGameReplacer.h"
#include "../RL/Replacer/TitleReplacer.h"
#include "../RL/QuickChatSave.h"
#include "Modes/Playground.h"
#include "../Persistance/Persistance.h"
#include "../QuickChatCustomUI/QuickChatCustomUI.h"
#include "../Loadout/Loadout.h"
#include "../Utils/Localization.h"
#include "../Utils/RLUtils.h"
#include "../RL/MemoryScanner.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>
#include <filesystem>

namespace Profile
{
    // ==================== Data Structures ====================
    
    struct ProfileData
    {
        std::string name;
        std::array<int32_t, 24> bindingIds = {};
        std::array<uint8_t, 24> channels = {};
        std::map<int32_t, std::string> customTexts;
        
        // Title and category customizations
        std::string titleCustomText;
        std::array<std::string, 6> settingsCategoryTexts = {};
        std::array<std::string, 6> inGameCategoryTexts = {};
    };
    
    // ==================== State ====================
    
    static std::vector<ProfileData> profiles;
    static int selectedProfile = 0;
    static char editBuffer[64] = "";
    static int editingProfile = -1;
    static bool editFocused = false;
    static std::string switchBind = "";
    static bool switchBindActive = false;
    
    // ==================== Profile Data Operations ====================
    
    static void ClearAllCustomizations()
    {
        QuickChatData::titleData.customText.clear();
        
        for (auto& cat : QuickChatData::settingsCategories)
            cat.customText.clear();
        
        for (auto& cat : QuickChatData::inGameCategories)
            cat.customText.clear();
        
        for (auto& qc : QuickChatData::allChats)
        {
            qc.customText.clear();
            qc.processedText.clear();
        }
    }
    
    static void CaptureStateToProfile(ProfileData& data)
    {
        for (int i = 0; i < 24; i++)
        {
            data.bindingIds[i] = QuickChatData::userBindings[i]
                ? QuickChatData::userBindings[i]->id.FNameEntryId : 0;
            data.channels[i] = static_cast<uint8_t>(QuickChatData::slotChannels[i]);
        }
        
        data.customTexts.clear();
        for (auto& qc : QuickChatData::allChats)
        {
            if (!qc.customText.empty())
            {
                data.customTexts[qc.id.FNameEntryId] = qc.customText;
            }
        }
        
        // Save title
        data.titleCustomText = QuickChatData::titleData.customText;
        
        // Save settings categories
        for (size_t i = 0; i < QuickChatData::settingsCategories.size() && i < 6; i++)
        {
            data.settingsCategoryTexts[i] = QuickChatData::settingsCategories[i].customText;
        }
        
        // Save in-game categories
        for (size_t i = 0; i < QuickChatData::inGameCategories.size() && i < 6; i++)
        {
            data.inGameCategoryTexts[i] = QuickChatData::inGameCategories[i].customText;
        }
    }
    
    static void ApplyStateFromProfile(const ProfileData& data)
    {
        ClearAllCustomizations();
        
        // Load bindings and channels
        for (int i = 0; i < 24; i++)
        {
            QuickChatData::userBindings[i] = QuickChatData::FindByFNameId(data.bindingIds[i]);
            QuickChatData::slotChannels[i] = static_cast<QuickChatData::Channel>(data.channels[i]);
        }
        
        // Load customTexts
        for (const auto& [fnameId, text] : data.customTexts)
        {
            auto* qc = QuickChatData::FindByFNameId(fnameId);
            if (qc)
            {
                qc->customText = text;
            }
        }
        
        // Load title
        QuickChatData::titleData.customText = data.titleCustomText;
        
        // Load settings categories
        for (size_t i = 0; i < QuickChatData::settingsCategories.size() && i < 6; i++)
        {
            QuickChatData::settingsCategories[i].customText = data.settingsCategoryTexts[i];
        }
        
        // Load in-game categories
        for (size_t i = 0; i < QuickChatData::inGameCategories.size() && i < 6; i++)
        {
            QuickChatData::inGameCategories[i].customText = data.inGameCategoryTexts[i];
        }
        
        QuickChatData::buffersDirty = true;
    }
    
    static void SaveCurrentProfile()
    {
        if (selectedProfile < 0 || selectedProfile >= static_cast<int>(profiles.size()))
        {
            return;
        }
        
        CaptureStateToProfile(profiles[selectedProfile]);
        Persistance::SaveProfile(profiles[selectedProfile].name);
        LOG("Profile saved: {}", profiles[selectedProfile].name);
    }
    
    static void LoadProfile(int index)
    {
        if (index < 0 || index >= static_cast<int>(profiles.size()))
        {
            return;
        }
        
        // Load from disk, then sync in-memory cache
        Persistance::LoadProfile(profiles[index].name);
        CaptureStateToProfile(profiles[index]);
        LOG("Profile loaded: {}", profiles[index].name);
    }
    
    // ==================== Profile Actions ====================
    
    static void CreateProfile(const std::string& name)
    {
        // Prevent duplicate names
        for (const auto& p : profiles)
        {
            if (p.name == name) return;
        }

        if (!profiles.empty())
        {
            SaveCurrentProfile();
        }
        
        // New profile starts with empty customizations
        ClearAllCustomizations();
        
        // Reset settings to defaults
        for (size_t i = 0; i < QuickChatData::settingsCategories.size() && i < 6; i++)
        {
            auto& sc = QuickChatData::settingsCategories[i];
            if (sc.address != 0 && sc.maxChars > 0)
            {
                std::wstring woriginal = RLUtils::Utf8ToWide(sc.text);
                MemoryUtils::WriteWideString(sc.address, woriginal, sc.maxChars);
            }
            QuickChatData::settingsBuf[i][0] = '\0';
        }
        
        ProfileData newProfile;
        newProfile.name = name;
        CaptureStateToProfile(newProfile);
        profiles.push_back(newProfile);
        selectedProfile = static_cast<int>(profiles.size()) - 1;
        
        // Refresh UI buffers to show empty state
        QuickChatData::buffersDirty = true;
        
        Persistance::SaveProfile(name);
        Persistance::SaveGlobalSettings();
        LOG("Profile created: {}", name);
        
        // Apply the new profile state (same as SwitchToProfile refac)
        Playground::UpdateAllProcessedText();
        RefreshSettings::RefreshDelayed();
        InGameReplacer::ForceRefreshDelayed();
        TitleReplacer::SetTitle(QuickChatData::titleData.customText);
        Notification::Show(TL("profile_created"), name);
        Notification::Show(profiles[selectedProfile].name, TL("profile_switch"));
        QuickChatCustomUI::OnProfileChanged(name);
    }
    
    static void SwitchToProfile(int index)
    {
        if (index < 0 || index >= static_cast<int>(profiles.size()))
        {
            return;
        }
        
        if (index == selectedProfile)
        {
            return;
        }
        
        selectedProfile = index;
        LoadProfile(selectedProfile);
        Persistance::SaveGlobalSettings();
        
        // Re-apply all QC bindings to the game
        for (int i = 0; i < 24; i++)
        {
            auto* qc = QuickChatData::userBindings[i];
            if (qc)
            {
                QuickChatSave::SaveNoRefreshDelayed(i, qc->id.ToString());
            }
        }
        
        Playground::UpdateAllProcessedText();
        RefreshSettings::RefreshDelayed();
        InGameReplacer::ForceRefreshDelayed();
        TitleReplacer::SetTitle(QuickChatData::titleData.customText);
        Notification::Show(profiles[selectedProfile].name, TL("profile_switch"));
        QuickChatCustomUI::OnProfileChanged(profiles[selectedProfile].name);
    }
    
    static void SwitchToNext()
    {
        if (profiles.size() < 2)
        {
            return;
        }
        
        int nextProfile = (selectedProfile + 1) % static_cast<int>(profiles.size());
        SwitchToProfile(nextProfile);
    }
    
    static void DeleteProfile(int index)
    {
        if (index < 0 || index >= static_cast<int>(profiles.size()))
        {
            return;
        }
        
        std::string deletedName = profiles[index].name;
        profiles.erase(profiles.begin() + index);
        Persistance::DeleteProfileFolder(deletedName);
        
        if (selectedProfile >= static_cast<int>(profiles.size()) && selectedProfile > 0)
        {
            selectedProfile--;
        }
        
        if (!profiles.empty())
        {
            LoadProfile(selectedProfile);
            QuickChatCustomUI::OnProfileChanged(profiles[selectedProfile].name);
        }
        else
        {
            selectedProfile = 0;
            ClearAllCustomizations();
            Welcome::SetHasProfile(false);
        }
    }
    
    static void MoveProfile(int index, int direction)
    {
        int newIndex = index + direction;
        if (newIndex < 0 || newIndex >= static_cast<int>(profiles.size()))
        {
            return;
        }
        
        std::swap(profiles[index], profiles[newIndex]);
        selectedProfile = newIndex;
    }
    
    // ==================== UI Helpers ====================
    
    static void DrawRotatedText(ImDrawList* drawList, const char* text, 
                                ImVec2 center, float angleDegrees, ImU32 color)
    {
        ImFont* font = ImGui::GetFont();
        const ImFontGlyph* glyph = font->FindGlyph(static_cast<ImWchar>(text[0]));
        if (!glyph)
        {
            return;
        }
        
        float angleRad = angleDegrees * 3.14159265f / 180.0f;
        float cos_a = std::cos(angleRad);
        float sin_a = std::sin(angleRad);
        
        float glyphCenterX = (glyph->X0 + glyph->X1) * 0.5f;
        float glyphCenterY = (glyph->Y0 + glyph->Y1) * 0.5f;
        
        drawList->PushTextureID(font->ContainerAtlas->TexID);
        
        float x0 = glyph->X0 - glyphCenterX;
        float y0 = glyph->Y0 - glyphCenterY;
        float x1 = glyph->X1 - glyphCenterX;
        float y1 = glyph->Y1 - glyphCenterY;
        
        ImVec2 vtx[4];
        vtx[0] = ImVec2(center.x + x0 * cos_a - y0 * sin_a, center.y + x0 * sin_a + y0 * cos_a);
        vtx[1] = ImVec2(center.x + x1 * cos_a - y0 * sin_a, center.y + x1 * sin_a + y0 * cos_a);
        vtx[2] = ImVec2(center.x + x1 * cos_a - y1 * sin_a, center.y + x1 * sin_a + y1 * cos_a);
        vtx[3] = ImVec2(center.x + x0 * cos_a - y1 * sin_a, center.y + x0 * sin_a + y1 * cos_a);
        
        drawList->PrimReserve(6, 4);
        drawList->PrimQuadUV(vtx[0], vtx[1], vtx[2], vtx[3], 
            ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V0),
            ImVec2(glyph->U1, glyph->V1), ImVec2(glyph->U0, glyph->V1), color);
        
        drawList->PopTextureID();
    }
    
    static bool ArrowButton(const char* id, bool up, bool enabled)
    {
        float size = ImGui::GetFrameHeight();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        if (!enabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        }
        
        bool clicked = ImGui::Button(id, ImVec2(size, size)) && enabled;
        
        float angle = up ? 90.f : -90.f;
        ImVec2 center(pos.x + size / 2, pos.y + size / 2);
        DrawRotatedText(ImGui::GetWindowDrawList(), "<", center, angle, 
                        ImGui::GetColorU32(ImGuiCol_Text));
        
        if (!enabled)
        {
            ImGui::PopStyleVar();
        }
        
        return clicked;
    }
    
    static int InlineEdit(const char* id, char* buffer, size_t bufSize, bool& focused)
    {
        float availW = ImGui::GetContentRegionAvail().x;
        float width = std::max(50.f, availW - 20.f);
        ImGui::SetNextItemWidth(width);
        
        if (!focused)
        {
            ImGui::SetKeyboardFocusHere();
            focused = true;
        }
        
        bool enterPressed = ImGui::InputText(id, buffer, bufSize, ImGuiInputTextFlags_EnterReturnsTrue);
        
        // Enter pressed with text = confirm
        if (enterPressed && buffer[0])
        {
            return 1;
        }
        
        // Click outside 
        if (ImGui::IsItemDeactivatedAfterEdit() || (ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()))
        {
            // If there's text, confirm; otherwise cancel
            return buffer[0] ? 1 : -1;
        }
        
        return 0;
    }
    
    // ==================== Public API ====================
    
    void Render()
    {
        int profileCount = static_cast<int>(profiles.size());
        
        // Share code buttons
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.25f, 0.55f, 1.0f));
        
        if (ImGui::Button(TL("copy")))
        {
            std::string code = Loadout::Encode();
            if (!code.empty())
            {
                ImGui::SetClipboardText(code.c_str());
                Notification::Show(TL("copied"), std::string(TL("profile_prefix")) + Profile::GetCurrentProfileName());
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", TL("copy_share_tooltip"));
        
        ImGui::SameLine();
        
        if (ImGui::Button(TL("load")))
        {
            Loadout::OpenLoadModal();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", TL("load_share_tooltip"));
        
        ImGui::PopStyleColor(3);
        
        // Keybind for switching profiles (UI only - the actual check is in Tick())
        if (HandleKeyPress::BindButton("##switch", "Switch: ", switchBind))
        {
            Persistance::SaveGlobalSettings();
        }
        
        ImGui::Spacing();
        
        // Toolbar
        if (ImGui::Button(TL("add")))
        {
            editingProfile = -2;
            editBuffer[0] = '\0';
            editFocused = false;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(TL("rename")) && selectedProfile < profileCount)
        {
            editingProfile = selectedProfile;
            strncpy_s(editBuffer, profiles[selectedProfile].name.c_str(), 63);
            editFocused = false;
        }
        
        if (ArrowButton("##up", true, selectedProfile > 0))
        {
            MoveProfile(selectedProfile, -1);
        }
        
        ImGui::SameLine();
        
        if (ArrowButton("##down", false, selectedProfile < profileCount - 1))
        {
            MoveProfile(selectedProfile, 1);
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(TL("delete")) && profileCount > 0)
        {
            DeleteProfile(selectedProfile);
        }
        
        ImGui::Spacing();
        
        // Profile list
        for (int i = 0; i < profileCount; i++)
        {
            ImGui::PushID(i);
            
            if (editingProfile == i)
            {
                int result = InlineEdit("##edit", editBuffer, 64, editFocused);
                if (result == 1)
                {
                    std::string oldName = profiles[i].name;
                    std::string newName = editBuffer;
                    profiles[i].name = newName;
                    
                    // Rename folder on disk
                    std::string oldPath = Persistance::GetProfilePath(oldName);
                    std::string newPath = Persistance::GetProfilePath(newName);
                    if (std::filesystem::exists(oldPath) && oldName != newName)
                    {
                        std::error_code ec;
                        std::filesystem::rename(oldPath, newPath, ec);
                    }
                    Persistance::SaveProfile(newName);
                }
                if (result != 0)
                {
                    editingProfile = -1;
                }
            }
            else
            {
                if (ImGui::Selectable(profiles[i].name.c_str(), i == selectedProfile))
                {
                    SwitchToProfile(i);
                }
                
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    editingProfile = i;
                    strncpy_s(editBuffer, profiles[i].name.c_str(), 63);
                    editFocused = false;
                }
            }
            
            ImGui::PopID();
        }
        
        // New profile input
        if (editingProfile == -2)
        {
            int result = InlineEdit("##new", editBuffer, 64, editFocused);
            if (result == 1)
            {
                CreateProfile(editBuffer);
            }
            if (result != 0)
            {
                editingProfile = -1;
            }
        }
        
        Loadout::RenderLoadModal();
    }
    
    void AddProfile(const std::string& name)
    {
        CreateProfile(name);
    }
    
    std::string GetCurrentProfileName()
    {
        if (selectedProfile >= 0 && selectedProfile < static_cast<int>(profiles.size()))
        {
            return profiles[selectedProfile].name;
        }
        return "";
    }
    
    void Tick()
    {
        if (!switchBind.empty())
        {
            bool isActive = HandleKeyPress::IsBindingActive(switchBind);
            if (isActive && !switchBindActive)
            {
                SwitchToNext();
            }
            switchBindActive = isActive;
        }
    }
    
    std::string GetSwitchBind()
    {
        return switchBind;
    }
    
    void SetSwitchBind(const std::string& bind)
    {
        switchBind = bind;
    }
    
    bool AnyProfileHasTitle()
    {
        for (const auto& p : profiles)
        {
            if (!p.titleCustomText.empty()) return true;
        }
        return false;
    }
    
    void InitializeProfiles()
    {
        auto savedProfiles = Persistance::GetProfileList();
        if (savedProfiles.empty())
        {
            return; // No saved profiles, Welcome screen will handle first creation
        }
        
        for (const auto& name : savedProfiles)
        {
            ProfileData newProfile;
            newProfile.name = name;
            newProfile.titleCustomText = Persistance::ReadProfileTitle(name);
            profiles.push_back(newProfile);
        }
        
        // Restore the last-used profile from settings.json
        std::string savedName = Persistance::GetSavedSelectedProfile();
        selectedProfile = 0;
        for (int i = 0; i < static_cast<int>(profiles.size()); i++)
        {
            if (profiles[i].name == savedName)
            {
                selectedProfile = i;
                break;
            }
        }
        
        Persistance::LoadProfile(profiles[selectedProfile].name);
        CaptureStateToProfile(profiles[selectedProfile]);
        
        // Sync profile bindings to game
        for (int i = 0; i < 24; i++)
        {
            auto* qc = QuickChatData::userBindings[i];
            if (qc)
                QuickChatSave::SaveNoRefreshDelayed(i, qc->id.ToString());
        }
        
        QuickChatCustomUI::OnProfileChanged(profiles[selectedProfile].name);
        
        LOG("Profiles: Loaded {} saved profiles, selected '{}'", 
            profiles.size(), profiles[selectedProfile].name);
    }
}

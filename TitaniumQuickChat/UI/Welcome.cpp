#include "pch.h"
#include "Welcome.h"
#include "Profile.h"
#include "../Utils/Localization.h"
#include "../Utils/ResourceExtractor.h"
#include "../resource.h"

namespace Welcome
{
    static char firstProfileName[64] = "";
    static bool hasProfile = false;
    static std::shared_ptr<ImageWrapper> logoImage;
    
    bool HasProfile()
    {
        return hasProfile;
    }
    
    void SetHasProfile(bool value)
    {
        hasProfile = value;
    }

    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        HMODULE hModule = GetCurrentModule();
        if (!hModule) return;

        std::string dataFolder = wrapper->GetDataFolder().string() + "\\TitaniumQuickChat";
        std::filesystem::create_directories(dataFolder);

        std::string logoPath = ExtractResource(hModule, IDR_LOGO, dataFolder + "\\logo.png");
        if (!logoPath.empty())
        {
            logoImage = std::make_shared<ImageWrapper>(logoPath, false, true);
        }
    }
    
    void Render()
    {
        float windowW = ImGui::GetContentRegionAvail().x;
        float windowH = ImGui::GetContentRegionAvail().y;
        
        // Center
        float contentHeight = 160.0f;
        ImGui::SetCursorPosY((windowH - contentHeight) * 0.5f);
        
        // Prefix
        ImVec4 green(0.4f, 0.8f, 0.4f, 1.0f);
        float textWidth = ImGui::CalcTextSize(TL("welcome_prefix")).x;
        ImGui::SetCursorPosX((windowW - textWidth) * 0.5f);
        ImGui::TextColored(green, "%s", TL("welcome_prefix"));
        ImGui::Spacing();

        // Logo
        if (logoImage)
        {
            if (!logoImage->IsLoadedForImGui())
            {
                logoImage->LoadForImGui([](bool) {});
            }
            else
            {
                void* tex = logoImage->GetImGuiTex();
                if (tex)
                {
                    Vector2 size = logoImage->GetSize();
                    float logoH = 25.0f;
                    float scale = logoH / size.Y;
                    float logoW = size.X * scale;

                    ImGui::SetCursorPosX((windowW - logoW) * 0.5f);
                    ImGui::Image((ImTextureID)tex, ImVec2(logoW, logoH));
                }
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();
        
        // Label
        textWidth = ImGui::CalcTextSize(TL("profile_name")).x;
        ImGui::SetCursorPosX((windowW - textWidth) * 0.5f);
        ImGui::Text("%s", TL("profile_name"));
        
        // Input center
        ImGui::SetCursorPosX((windowW - 300) * 0.5f);
        ImGui::SetNextItemWidth(300);
        
        // Auto-focus
        bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (windowFocused && !ImGui::IsAnyItemActive())
        {
            ImGui::SetKeyboardFocusHere();
        }
        
        bool enterPressed = ImGui::InputText("##FirstProfile", firstProfileName, 64, 
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::Spacing();
        
        // Button center
        ImGui::SetCursorPosX((windowW - 100) * 0.5f);
        
        bool canCreate = firstProfileName[0] != '\0';
        
        if (!canCreate)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        }
        
        if ((ImGui::Button(TL("create"), ImVec2(100, 0)) || enterPressed) && canCreate)
        {
            Profile::AddProfile(firstProfileName);
            hasProfile = true;
            LOG("Profile created: {}", firstProfileName);
        }
        
        if (!canCreate)
        {
            ImGui::PopStyleVar();
        }
    }
}

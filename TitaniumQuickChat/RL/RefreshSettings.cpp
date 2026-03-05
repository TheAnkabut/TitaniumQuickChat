// RefreshSettings.cpp - Triggers Settings UI refresh via OnShellSet

#include "pch.h"
#include "RefreshSettings.h"
#include "Internals.h"
#include "Replacer/ReplacerUtils.h"

namespace RefreshSettings
{
    // ==================== State ====================
    
    static std::shared_ptr<GameWrapper> gameWrapper;
    static bool refreshPending = false;
    static double lastRefreshTime = 0.0;
    static constexpr double REFRESH_COOLDOWN = 0.5;  // Min seconds between refreshes
    
    // ==================== Helpers ====================
    
    static bool DoRefresh()
    {
        if (!Internals::IsInitialized()) return false;
        
        UFunction* onShellSetFunc = UFunction::FindFunction(
            "Function TAGame.GFxData_QuickChatBindings_TA.OnShellSet"
        );
        if (!onShellSetFunc) return false;
        
        // Always re-find object 
        UObject* gfxBindings = Internals::FindObject(
            "GFxData_QuickChatBindings_TA Transient.GFxData_QuickChatBindings_TA"
        );
        if (!gfxBindings) return false;
        
        // Restore any FString pointers replaced by WriteFString before
        ReplacerUtils::RestoreOriginalPointers();
        
        gfxBindings->ProcessEvent(onShellSetFunc, nullptr, nullptr);
        LOG("RefreshSettings: SUCCESS");
        return true;
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
    }
    
    bool Refresh()
    {
        return DoRefresh();
    }
    
    void RefreshDelayed()
    {
        // Debounce: if a refresh is already pending, skip
        if (refreshPending) {
            LOG("RefreshSettings: Already pending, skipping");
            return;
        }
        
        refreshPending = true;
        
        gameWrapper->SetTimeout([](GameWrapper*) {
            refreshPending = false;
            
            double now = ImGui::GetTime();
            if (now - lastRefreshTime < REFRESH_COOLDOWN) {
                LOG("RefreshSettings: Cooldown active ({:.2f}s since last), skipping", now - lastRefreshTime);
                return;
            }
            
            lastRefreshTime = now;
            DoRefresh();
        }, 0.1f);  // 100ms
    }
}

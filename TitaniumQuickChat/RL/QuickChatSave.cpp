// QuickChatSave.cpp - Saves quick chat bindings to game profile

#include "pch.h"
#include "QuickChatSave.h"
#include "Internals.h"
#include "RefreshSettings.h"  

namespace QuickChatSave
{
    static std::shared_ptr<GameWrapper> gameWrapper;
    static UObject* profileSave = nullptr;
    
    // ==================== Helpers ====================
    
    static void CacheProfileSaveObject()
    {
        if (profileSave) return;
        
        auto GObjects = UObject::GObjObjects();
        
        for (int32_t index = 0; index < GObjects->size(); index++)
        {
            UObject* obj = (*GObjects)[index];
            if (!obj) continue;
            
            if (obj->GetFullName().find("ProfileQuickChatSave_TA Transient.SaveData_TA") != std::string::npos)
            {
                profileSave = obj;
                break;
            }
        }
    }
    
    // ==================== Internal ====================
    
    void SaveNoRefresh(int32_t index, const std::string& fnameName)
    {
        if (!Internals::IsInitialized()) { LOG("QCSave: not initialized"); return; }
        if (index < 0 || index > 23) { LOG("QCSave: bad index {}", index); return; }
        
        FName message;
        if (!Internals::GetFNameByString(fnameName, message)) { LOG("QCSave: FName '{}' not found", fnameName); return; }
        
        if (!profileSave) CacheProfileSaveObject();
        
        UFunction* changeBindingFunc = UFunction::FindFunction(
            "Function TAGame.ProfileQuickChatSave_TA.ChangeQuickChatBinding"
        );
        if (!profileSave || !changeBindingFunc) { LOG("QCSave: profileSave={} changeFunc={}", (void*)profileSave, (void*)changeBindingFunc); return; }
        
        struct { int32_t Index; FName Message; } params;
        params.Index = index;
        params.Message = message;
        
        profileSave->ProcessEvent(changeBindingFunc, &params, nullptr);
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
    }
    
    void Save(int32_t index, const std::string& fnameName)
    {
        SaveNoRefresh(index, fnameName);
        RefreshSettings::RefreshDelayed();
    }
    
    void SaveDelayed(int32_t index, const std::string& fnameName)
    {
        gameWrapper->Execute([index, fnameName](GameWrapper*) {
            Save(index, fnameName);
        });
    }
    
    void SaveNoRefreshDelayed(int32_t index, const std::string& fnameName)
    {
        gameWrapper->Execute([index, fnameName](GameWrapper*) {
            SaveNoRefresh(index, fnameName);
        });
    }
}

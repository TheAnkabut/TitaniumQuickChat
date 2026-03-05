// Notification.cpp - In-game popup notifications via RL engine
// Enables bShowInGameNotifications, creates via PopUpOnlyNotification,
// sets title/body/duration, triggers OnPopUpOnlyNotificationCreated

#include "pch.h"
#include "Notification.h"
#include "Internals.h"
#include "../UI/Profile.h"
#include "../Utils/Localization.h"

namespace Notification
{
    // ==================== Structures ====================
    
    struct UNotificationManager_TA_PopUpOnlyNotification_Params
    {
        UClass*  NotificationClass;              
        UObject* ReturnValue;                    
    };
    
    struct UNotification_TA_SetTitle_Params
    {
        FString  InTitle;                        
        UObject* ReturnValue;                    
    };
    
    struct UNotification_TA_SetBody_Params
    {
        FString  InBody;                         
        UObject* ReturnValue;                    
    };
    
    // ==================== Constants ====================
    
    static constexpr size_t OFFSET_GFXDATA_FLAGS       = 0xA8;
    static constexpr size_t OFFSET_NOTIFSAVE_FLAGS     = 0xD8;
    static constexpr size_t OFFSET_POPUP_FLAGS         = 0x88;
    static constexpr size_t OFFSET_POPUP_DURATION      = 0x8C;
    static constexpr float  DEFAULT_POPUP_DURATION     = 5.0f;
    
    static std::shared_ptr<GameWrapper> gameWrapper;
    static bool notificationsEnabled = false;
    
    // ==================== Helpers ====================
    
    static void EnableNotifications()
    {
        if (notificationsEnabled)
        {
            return;
        }
        
        // GFxData_NotificationManager_TA.bShowInGameNotifications
        UObject* gfxNotificationManager = Internals::FindObject(
            "GFxData_NotificationManager_TA Transient.GFxData_NotificationManager_TA"
        );
        if (gfxNotificationManager)
        {
            uintptr_t base = reinterpret_cast<uintptr_t>(gfxNotificationManager);
            uint32_t* flagsPtr = reinterpret_cast<uint32_t*>(base + OFFSET_GFXDATA_FLAGS);
            *flagsPtr |= 0x03;
        }
        
        // NotificationSave_TA.bShowInGameNotifications
        UObject* notificationSave = Internals::FindObject(
            "NotificationSave_TA Transient.SaveData_TA.NotificationSave_TA"
        );
        if (notificationSave)
        {
            uintptr_t base = reinterpret_cast<uintptr_t>(notificationSave);
            uint32_t* flagsPtr = reinterpret_cast<uint32_t*>(base + OFFSET_NOTIFSAVE_FLAGS);
            *flagsPtr |= 0x03;
        }
        
        notificationsEnabled = (gfxNotificationManager != nullptr || notificationSave != nullptr);
    }
    
    static void DisplayNotification(const std::string& title, const std::string& body)
    {
        EnableNotifications();
        
        // Find NotificationManager_TA (always re-find, RL may recreate it)
        UObject* notificationManager = Internals::FindObject(
            "NotificationManager_TA Transient.GameEngine_TA.LocalPlayer_TA.OnlinePlayer_TA.NotificationManager_TA"
        );
        if (!notificationManager)
        {
            return;
        }
        
        // Find GenericNotification_TA class
        UClass* genericNotificationClass = reinterpret_cast<UClass*>(
            Internals::FindObject("Class TAGame.GenericNotification_TA")
        );
        if (!genericNotificationClass)
        {
            return;
        }
        
        // Find PopUpOnlyNotification function
        UFunction* popUpOnlyNotificationFunc = UFunction::FindFunction(
            "Function TAGame.NotificationManager_TA.PopUpOnlyNotification"
        );
        if (!popUpOnlyNotificationFunc)
        {
            return;
        }
        
        // Call PopUpOnlyNotification
        UNotificationManager_TA_PopUpOnlyNotification_Params popUpParams = {};
        popUpParams.NotificationClass = genericNotificationClass;
        
        notificationManager->ProcessEvent(popUpOnlyNotificationFunc, &popUpParams, nullptr);
        
        UObject* notification = popUpParams.ReturnValue;
        if (!notification)
        {
            return;
        }
        
        // Find SetTitle and SetBody functions
        UFunction* setTitleFunc = UFunction::FindFunction("Function TAGame.Notification_TA.SetTitle");
        UFunction* setBodyFunc = UFunction::FindFunction("Function TAGame.Notification_TA.SetBody");
        
        if (setTitleFunc && setBodyFunc)
        {
            // SetTitle
            std::wstring wideTitle(title.begin(), title.end());
            
            UNotification_TA_SetTitle_Params titleParams = {};
            titleParams.InTitle.Data = const_cast<wchar_t*>(wideTitle.c_str());
            titleParams.InTitle.Count = static_cast<int32_t>(wideTitle.length() + 1);
            titleParams.InTitle.Max = titleParams.InTitle.Count;
            
            notification->ProcessEvent(setTitleFunc, &titleParams, nullptr);
            
            // SetBody
            std::wstring wideBody(body.begin(), body.end());
            
            UNotification_TA_SetBody_Params bodyParams = {};
            bodyParams.InBody.Data = const_cast<wchar_t*>(wideBody.c_str());
            bodyParams.InBody.Count = static_cast<int32_t>(wideBody.length() + 1);
            bodyParams.InBody.Max = bodyParams.InBody.Count;
            
            notification->ProcessEvent(setBodyFunc, &bodyParams, nullptr);
        }
        
        // Set bPopUp and PopUpDuration on notification object
        uintptr_t notificationBase = reinterpret_cast<uintptr_t>(notification);
        
        uint32_t* popUpFlagsPtr = reinterpret_cast<uint32_t*>(notificationBase + OFFSET_POPUP_FLAGS);
        *popUpFlagsPtr |= 0x01;
        *popUpFlagsPtr &= ~0x04;
        
        float* popUpDurationPtr = reinterpret_cast<float*>(notificationBase + OFFSET_POPUP_DURATION);
        *popUpDurationPtr = DEFAULT_POPUP_DURATION;
        
        // Call OnPopUpOnlyNotificationCreated
        UFunction* onPopUpOnlyNotificationCreatedFunc = UFunction::FindFunction(
            "Function TAGame.Notification_TA.OnPopUpOnlyNotificationCreated"
        );
        if (onPopUpOnlyNotificationCreatedFunc)
        {
            notification->ProcessEvent(onPopUpOnlyNotificationCreatedFunc, nullptr, nullptr);
        }
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        DisplayNotification("Titanium Quick Chat", TL("plugin_loaded"));
        
        std::string profileName = Profile::GetCurrentProfileName();
        if (!profileName.empty())
        {
            DisplayNotification(profileName, TL("profile_active"));
        }
    }
    
    void Show(const std::string& title, const std::string& body)
    {
        gameWrapper->Execute([title, body](GameWrapper*) {
            DisplayNotification(title, body);
        });
    }
}

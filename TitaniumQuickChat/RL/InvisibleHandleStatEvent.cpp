// InvisibleHandleStatEvent.cpp - Triggers invisible stat event to refresh UI

#include "pch.h"
#include "InvisibleHandleStatEvent.h"
#include "Internals.h"
#include "../Persistance/Persistance.h"
#include "bakkesmod/wrappers/ImageWrapper.h"

namespace InvisibleHandleStatEvent
{
    static std::shared_ptr<GameWrapper> gameWrapper;
    
    // Cached lookups
    static UObject* cachedHud = nullptr;
    static UObject* cachedTemplate = nullptr;
    static UFunction* cachedFn = nullptr;
    static std::shared_ptr<ImageWrapper> cachedImg;
    static void* cachedTex = nullptr;
    
    static UObject* CreateCustomStatEvent(UObject* templateEvent, UObject* texture)
    {
        static char buffer[0x200];
        memcpy(buffer, templateEvent, 0x200);
        UObject* newEvent = reinterpret_cast<UObject*>(buffer);

        int32_t* pPoints = reinterpret_cast<int32_t*>(buffer + 0x60);
        *pPoints = 0;
        
        UObject** pTexture = reinterpret_cast<UObject**>(buffer + 0x78);
        *pTexture = texture;
        
        static wchar_t label[8] = L" ";
        FString* pLabel = reinterpret_cast<FString*>(buffer + 0x90);
        pLabel->Data = label;
        pLabel->Count = 2;
        pLabel->Max = 8;

        return newEvent;
    }
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
    }
    
    void ResetCache()
    {
        // HUD changes between matches, clear and re-found
        cachedHud = nullptr;
    }
    
    void Trigger()
    {
        gameWrapper->Execute([](GameWrapper* gw) {
            if (!cachedHud)
                cachedHud = Internals::FindObjectBySuffix("PersistentLevel.GFxHUD_Soccar_TA");
            if (!cachedTemplate)
                cachedTemplate = Internals::FindObject("StatEvent_TA StatEvents.Events.TurtleGoal");
            if (!cachedFn)
                cachedFn = UFunction::FindFunction("Function TAGame.GFxHUD_TA.HandleStatEvent");
            
            if (!cachedHud || !cachedTemplate || !cachedFn) return;
            
            if (!cachedTex)
            {
                cachedImg = std::make_shared<ImageWrapper>(Persistance::GetTransparentImagePath(), true);
                cachedTex = cachedImg->GetCanvasTex();
            }
            if (!cachedTex) return;
            
            UObject* customEvent = CreateCustomStatEvent(cachedTemplate, reinterpret_cast<UObject*>(cachedTex));
            auto pri = gw->GetPlayerController().GetPRI();
            if (pri.IsNull()) return;
            
            struct { void* PRI; void* StatEvent; int32_t Count; } params = {
                reinterpret_cast<void*>(pri.memory_address),
                reinterpret_cast<void*>(customEvent),
                1
            };
            cachedHud->ProcessEvent(cachedFn, &params, nullptr);
        });
    }
}

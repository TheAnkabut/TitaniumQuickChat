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

            auto pri = gw->GetPlayerController().GetPRI();
            if (pri.IsNull()) return;

            char* stat = reinterpret_cast<char*>(cachedTemplate);

            // Save
            int32_t  pPoints  = *reinterpret_cast<int32_t*>(stat + 0x60);
            UObject* pTexture = *reinterpret_cast<UObject**>(stat + 0x78);
            uint64_t pSound   = *reinterpret_cast<uint64_t*>(stat + 0x80);
            FString  pLabel   = *reinterpret_cast<FString*>(stat + 0x90);

            // Patch
            *reinterpret_cast<int32_t*>(stat + 0x60) = 0;
            *reinterpret_cast<UObject**>(stat + 0x78) = reinterpret_cast<UObject*>(cachedTex);
            *reinterpret_cast<uint64_t*>(stat + 0x80) = 0;
            static wchar_t emptyLabel[4] = L" ";
            FString* labelPtr = reinterpret_cast<FString*>(stat + 0x90);
            labelPtr->Data  = emptyLabel;
            labelPtr->Count = 2;
            labelPtr->Max   = 4;

            struct { void* PRI; void* StatEvent; int32_t Count; } params = {
                reinterpret_cast<void*>(pri.memory_address),
                reinterpret_cast<void*>(cachedTemplate),
                1
            };
            cachedHud->ProcessEvent(cachedFn, &params, nullptr);

            // Restore
            *reinterpret_cast<int32_t*>(stat + 0x60) = pPoints;
            *reinterpret_cast<UObject**>(stat + 0x78) = pTexture;
            *reinterpret_cast<uint64_t*>(stat + 0x80) = pSound;
            *reinterpret_cast<FString*>(stat + 0x90)  = pLabel;
        });
    }
}

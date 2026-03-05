// Localize.cpp - Localized category names via Core.Object.Localize

#include "pch.h"
#include "Localize.h"
#include "Internals.h"
#include "QuickChatData.h"

namespace Localize
{
    // ==================== Structures ====================
    
    struct UObject_Localize_Params
    {
        FString  SectionName;                    
        FString  KeyName;                        
        FString  PackageName;                    
        uint32_t bOptional : 1;                  
        uint8_t  padding0[4];                    
        FString  ReturnValue;                    
    };
    
    // ==================== Public API ====================
    
    void Initialize()
    {
        QuickChatData::inGameCategories.clear();
        
        UFunction* localizeFunc = UFunction::FindFunction("Function Core.Object.Localize");
        if (!localizeFunc)
        {
            return;
        }
        
        UObject* coreObject = Internals::FindObject("Class Core.Object");
        if (!coreObject)
        {
            return;
        }
        
        const wchar_t* section = L"ChatPresetGroups";
        const wchar_t* package = L"TAGame";
        const wchar_t* groupKeys[] = { L"Group1", L"Group2", L"Group3", L"Group4", L"Group5", L"Group6" };
        
        for (int groupIndex = 0; groupIndex < 6; groupIndex++)
        {
            UObject_Localize_Params params = {};
            
            // SectionName
            params.SectionName.Data = const_cast<wchar_t*>(section);
            params.SectionName.Count = static_cast<int32_t>(wcslen(section) + 1);
            params.SectionName.Max = params.SectionName.Count;
            
            // KeyName
            params.KeyName.Data = const_cast<wchar_t*>(groupKeys[groupIndex]);
            params.KeyName.Count = static_cast<int32_t>(wcslen(groupKeys[groupIndex]) + 1);
            params.KeyName.Max = params.KeyName.Count;
            
            // PackageName
            params.PackageName.Data = const_cast<wchar_t*>(package);
            params.PackageName.Count = static_cast<int32_t>(wcslen(package) + 1);
            params.PackageName.Max = params.PackageName.Count;
            
            params.bOptional = 0;
            
            try
            {
                coreObject->ProcessEvent(localizeFunc, &params, nullptr);
                
                QuickChatData::InGameCategory category;
                category.text = params.ReturnValue.ToUTF8();
                if (category.text.empty())
                {
                    category.text = "Error";
                }
                QuickChatData::inGameCategories.push_back(category);
            }
            catch (...)
            {
                QuickChatData::InGameCategory category;
                category.text = "Error";
                QuickChatData::inGameCategories.push_back(category);
            }
        }
    }
}

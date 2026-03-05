// ReplacerUtils.h - Shared utilities for Replacer modules
#pragma once
#include "../Internals.h"
#include <string>
#include <vector>
#include <Windows.h>

namespace ReplacerUtils
{
    // Track original pointers to restore before UpdateChatGroups frees them
    struct OriginalPointer {
        FString* fstr;
        wchar_t* originalData;
        int32_t originalMax;
    };
    
    inline std::vector<OriginalPointer> g_originalPointers;
    
    // Restore all original pointers before UpdateChatGroups runs.
    inline void RestoreOriginalPointers()
    {
        for (auto& op : g_originalPointers)
        {
            if (op.fstr && op.originalData)
            {
                op.fstr->Data = op.originalData;
                op.fstr->Max = op.originalMax;
            }
        }
        g_originalPointers.clear();
    }
    
    // Write a UTF-8 string to an existing FString in memory.
    // Handles in-place write if fits, or heap allocation if larger.
    // Saves original pointer for safe restoration later.
    inline void WriteFString(FString* fstr, const std::string& text)
    {
        if (!fstr || !fstr->Data || text.empty()) return;
        
        int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wideLength <= 0) return;
        
        std::wstring wideText(wideLength, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wideText[0], wideLength);
        
        if (wideLength <= fstr->Max)
        {
            // Fits in existing buffer
            wcscpy_s(fstr->Data, fstr->Max, wideText.c_str());
            fstr->Count = wideLength;
        }
        else
        {
            // Need larger buffer - allocate on heap
            wchar_t* newBuffer = static_cast<wchar_t*>(
                HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wideLength * sizeof(wchar_t))
            );
            if (!newBuffer) return;
            
            wcscpy_s(newBuffer, wideLength, wideText.c_str());
            
            // Save original pointer for restoration before next UpdateChatGroups
            OriginalPointer op;
            op.fstr = fstr;
            op.originalData = fstr->Data;
            op.originalMax = fstr->Max;
            g_originalPointers.push_back(op);
            
            fstr->Data = newBuffer;
            fstr->Count = wideLength;
            fstr->Max = wideLength;
        }
    }
}

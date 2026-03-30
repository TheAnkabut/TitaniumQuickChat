// MemoryScanner.cpp - Memory scanning for Settings UI category addresses

#include "pch.h"
#include "MemoryScanner.h"
#include "QuickChatData.h"
#include "InvisibleHandleStatEvent.h"
#include "../Utils/RLUtils.h"
#include <thread>
#include <vector>
#include <windows.h>
#include <sstream>
#include <algorithm>
#include <functional>

namespace SettingsScanner
{
    // ==================== Constants - Category names ====================
    static const wchar_t* CATEGORIES_ENG[] = {
        L"Information (Team)", L"Compliments", L"Reactions", 
        L"Apologies", L"Post Game", L"Pre Game"
    };
    
    static const wchar_t* CATEGORIES_ESN[] = {
        L"Informaci\u00F3n (Equipo)", L"Cumplidos", L"Reacciones", 
        L"Disculpas", L"Final de la partida", L"Previa"
    };
    
    // ==================== State ====================
    static std::shared_ptr<GameWrapper> gameWrapper;
    static bool isSpanish = false;
    static bool hasScanned = false;
    static bool isScanning = false;
    static bool isUnsupportedLang = false;
    
    // ==================== Helper: Find wstring pattern in buffer ====================
    static int64_t FindInBuffer(const wchar_t* buffer, size_t bufferLen, const wchar_t* pattern, size_t patternLen)
    {
        for (size_t i = 0; i + patternLen <= bufferLen; ++i) {
            bool match = true;
            for (size_t j = 0; j < patternLen; ++j) {
                if (buffer[i + j] != pattern[j]) { match = false; break; }
            }
            if (match) return static_cast<int64_t>(i);
        }
        return -1;
    }
    
    // ==================== Helper: Scan for maxChars ====================
    static size_t ScanForMaxChars(uintptr_t addr)
    {
        if (addr == 0) return 50;
        const wchar_t* ptr = reinterpret_cast<const wchar_t*>(addr);
        bool foundNulls = false;
        for (size_t j = 0; j < 100; ++j) {
            if (ptr[j] == L'\0') {
                foundNulls = true;
            } else if (foundNulls) {
                return j;
            }
        }
        return 50;
    }
    
    // ==================== Scan Logic - English ====================
    static void ScanEnglish(uintptr_t* foundAddrs)
    {
        const wchar_t** CATEGORIES = CATEGORIES_ENG;
        MEMORY_BASIC_INFORMATION memInfo;
        uintptr_t addr = 0x10000000000;
        
        // Step 1: Find Pre Game and Info in 4MB region
        while (VirtualQuery(reinterpret_cast<void*>(addr), &memInfo, sizeof(memInfo))) {
            uintptr_t regionBase = reinterpret_cast<uintptr_t>(memInfo.BaseAddress);
            if (regionBase < 0x10000000000) { addr = regionBase + memInfo.RegionSize; continue; }
            
            if ((memInfo.State == MEM_COMMIT) && (memInfo.Type == MEM_PRIVATE) && 
                (memInfo.Protect == PAGE_READWRITE) && (memInfo.RegionSize == 0x400000) &&
                (memInfo.AllocationBase == memInfo.BaseAddress)) {
                
                const wchar_t* regionPtr = reinterpret_cast<const wchar_t*>(regionBase);
                size_t numWchars = memInfo.RegionSize / 2;
                
                int64_t preGamePos = FindInBuffer(regionPtr, numWchars, CATEGORIES[5], wcslen(CATEGORIES[5]));
                if (preGamePos >= 0) {
                    int64_t infoPos = FindInBuffer(regionPtr, numWchars, CATEGORIES[0], wcslen(CATEGORIES[0]));
                    if (infoPos >= 0) {
                        for (int c = 0; c < 6; ++c) {
                            int64_t pos = FindInBuffer(regionPtr, numWchars, CATEGORIES[c], wcslen(CATEGORIES[c]));
                            if (pos >= 0) foundAddrs[c] = regionBase + pos * 2;
                        }
                        break;
                    }
                }
            }
            addr = regionBase + memInfo.RegionSize;
            if (addr > 0x7FFFFFFFFFFF) break;
        }
    }
    
    // ==================== Scan Logic - Spanish ====================
    static void ScanSpanish(uintptr_t* foundAddrs)
    {
        const wchar_t** CATEGORIES = CATEGORIES_ESN;
        MEMORY_BASIC_INFORMATION memInfo;
        uintptr_t addr = 0x10000000000;
        
        // Step 1: Find Previa in 2MB region
        while (VirtualQuery(reinterpret_cast<void*>(addr), &memInfo, sizeof(memInfo))) {
            uintptr_t regionBase = reinterpret_cast<uintptr_t>(memInfo.BaseAddress);
            if (regionBase < 0x10000000000) { addr = regionBase + memInfo.RegionSize; continue; }
            
            if ((memInfo.State == MEM_COMMIT) && (memInfo.Type == MEM_PRIVATE) && 
                (memInfo.Protect == PAGE_READWRITE) && (memInfo.RegionSize == 0x200000) &&
                (memInfo.AllocationBase == memInfo.BaseAddress)) {
                
                int64_t pos = FindInBuffer(reinterpret_cast<const wchar_t*>(regionBase), memInfo.RegionSize / 2, CATEGORIES[5], wcslen(CATEGORIES[5]));
                if (pos >= 0) { foundAddrs[5] = regionBase + pos * 2; break; }
            }
            addr = regionBase + memInfo.RegionSize;
            if (addr > 0x7FFFFFFFFFFF) break;
        }
        
        // Step 2: Find Info and Cumplidos in 4MB region
        addr = 0x10000000000;
        while (VirtualQuery(reinterpret_cast<void*>(addr), &memInfo, sizeof(memInfo))) {
            uintptr_t regionBase = reinterpret_cast<uintptr_t>(memInfo.BaseAddress);
            if (regionBase < 0x10000000000) { addr = regionBase + memInfo.RegionSize; continue; }
            
            if ((memInfo.State == MEM_COMMIT) && (memInfo.Type == MEM_PRIVATE) && 
                (memInfo.Protect == PAGE_READWRITE) && (memInfo.RegionSize == 0x400000) &&
                (memInfo.AllocationBase == memInfo.BaseAddress)) {
                
                const wchar_t* regionPtr = reinterpret_cast<const wchar_t*>(regionBase);
                size_t numWchars = memInfo.RegionSize / 2;
                
                int64_t infoPos = FindInBuffer(regionPtr, numWchars, CATEGORIES[0], wcslen(CATEGORIES[0]));
                if (infoPos >= 0) {
                    int64_t cumpPos = FindInBuffer(regionPtr, numWchars, CATEGORIES[1], wcslen(CATEGORIES[1]));
                    if (cumpPos >= 0) {
                        for (int c = 0; c <= 4; ++c) {
                            int64_t pos = FindInBuffer(regionPtr, numWchars, CATEGORIES[c], wcslen(CATEGORIES[c]));
                            if (pos >= 0) foundAddrs[c] = regionBase + pos * 2;
                        }
                        break;
                    }
                }
            }
            addr = regionBase + memInfo.RegionSize;
            if (addr > 0x7FFFFFFFFFFF) break;
        }
    }
    
    // ==================== Public API ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        std::string lang = gameWrapper->GetUILanguage().ToString();
        isSpanish = (lang == "ESN");
        isUnsupportedLang = (lang != "INT" && lang != "ESN");
        if (isUnsupportedLang)
        {
            LOG("SettingsScanner: Unsupported language '{}' - skipping scan", lang);
        }
    }
    
    void Scan()
    {
        if (isScanning) return;
        if (isUnsupportedLang)
        {
            hasScanned = true;
            return;
        }
        isScanning = true;
        hasScanned = false;
        
        std::thread([]() {
            QuickChatData::settingsCategories.clear();
            
            const wchar_t** CATEGORIES = isSpanish ? CATEGORIES_ESN : CATEGORIES_ENG;
            uintptr_t foundAddrs[6] = {0};
            
            if (isSpanish) {
                ScanSpanish(foundAddrs);
            } else {
                ScanEnglish(foundAddrs);
            }
            
            // Build settingsCategories from found addresses
            for (int i = 0; i < 6; ++i) {
                QuickChatData::SettingsCategory category;
                category.text = RLUtils::WideToUtf8(CATEGORIES[i]);
                category.address = foundAddrs[i];
                category.maxChars = ScanForMaxChars(foundAddrs[i]);
                if (QuickChatData::settingsBuf[i][0] != '\0') category.customText = QuickChatData::settingsBuf[i];
                QuickChatData::settingsCategories.push_back(category);
            }
            
            hasScanned = true;
            isScanning = false;
            
            // Log results
            for (size_t i = 0; i < QuickChatData::settingsCategories.size(); ++i) {
                auto& cat = QuickChatData::settingsCategories[i];
                std::stringstream ss;
                ss << "SettingsCat[" << i << "] addr=0x" << std::hex << cat.address 
                   << " maxChars=" << std::dec << cat.maxChars << " text='" << cat.text << "'";
                LOG(ss.str());
                
                // Auto-write customText if already loaded from profile
                if (!cat.customText.empty() && cat.address != 0 && cat.maxChars > 0)
                {
                    std::wstring wtext = RLUtils::Utf8ToWide(cat.customText);
                    MemoryUtils::WriteWideString(cat.address, wtext, cat.maxChars);
                    LOG("SettingsScanner: Auto-wrote customText '{}' for cat {}", cat.customText, i);
                }
            }
        }).detach();
    }
    
    bool IsScanned()
    {
        return hasScanned;
    }
    
    bool IsUnsupportedLanguage()
    {
        return isUnsupportedLang;
    }
}

// ==================== MemoryUtils - Shared utilities ====================
namespace MemoryUtils
{
    void WriteWideString(uintptr_t address, const std::wstring& text, size_t maxChars)
    {
        if (address == 0 || maxChars == 0) return;
        
        // Write text + fill remaining with null chars to clear old content
        size_t maxText = maxChars - 1;
        size_t writeLen = (text.length() < maxText) ? text.length() : maxText;
        size_t bufferSize = maxChars * sizeof(wchar_t);
        
        std::vector<wchar_t> buffer(maxChars, L'\0');
        for (size_t i = 0; i < writeLen; ++i)
        {
            buffer[i] = text[i];
        }
        
        DWORD oldProtect;
        VirtualProtect(reinterpret_cast<void*>(address), bufferSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(reinterpret_cast<void*>(address), buffer.data(), bufferSize);
        VirtualProtect(reinterpret_cast<void*>(address), bufferSize, oldProtect, &oldProtect);
    }
}

// ==================== TitleScanner Implementation ====================
namespace TitleScanner
{
    static std::shared_ptr<GameWrapper> gameWrapper;
    static bool isScanning = false;
    static int activeTarget = -1;
    
    struct Target { const wchar_t* str; size_t len; const char* lang; size_t maxWriteLen; };
    static Target targets[] = {
        { L"QUICK CHAT", 10, "EN", 11 },
        { L"CHAT R\u00C1PIDO", 11, "ES", 12 },
    };
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        gameWrapper = wrapper;
        QuickChatData::titleData.foundAddress = 0;

        std::string lang = gameWrapper->GetUILanguage().ToString();
        if (lang == "INT") activeTarget = 0;
        else if (lang == "ESN") activeTarget = 1;
        else activeTarget = -1;
    }
    
    // Scan memory for title string. Filters: MEM_COMMIT, MEM_PRIVATE.
    // strict=true: Size==exactSize, Protect==0x4, AllocProtect==0x4, BaseAddress==AllocationBase
    // strict=false: Size in [0x300000,0x500000], Protect & PAGE_READWRITE
    static bool ScanPass(const Target& t, size_t anchorPos, wchar_t anchorChar,
                         size_t anchor2Pos, wchar_t anchor2Char,
                         size_t exactSize, bool strict,
                         std::chrono::steady_clock::time_point scanStart,
                         const char* passName)
    {
        uintptr_t addr = 0x10000000000;
        MEMORY_BASIC_INFORMATION memInfo;
        int totalRegions = 0;
        
        while (addr < 0x7FFFFFFFFFFF) {
            if (!VirtualQuery(reinterpret_cast<void*>(addr), &memInfo, sizeof(memInfo))) {
                addr += 0x10000; continue;
            }
            uintptr_t regionBase = reinterpret_cast<uintptr_t>(memInfo.BaseAddress);
            
            bool sizeMatch = (exactSize > 0) 
                ? (memInfo.RegionSize == exactSize)
                : (memInfo.RegionSize >= 0x300000 && memInfo.RegionSize <= 0x500000);
            
            bool protMatch = strict 
                ? (memInfo.Protect == PAGE_READWRITE &&
                   memInfo.AllocationProtect == PAGE_READWRITE &&
                   memInfo.AllocationBase == memInfo.BaseAddress)
                : (memInfo.Protect & PAGE_READWRITE);
            
            if ((memInfo.State == MEM_COMMIT) && protMatch &&
                (memInfo.Type == MEM_PRIVATE) && sizeMatch) {
                
                totalRegions++;
                size_t regionWchars = memInfo.RegionSize / sizeof(wchar_t);
                const wchar_t* regionPtr = reinterpret_cast<const wchar_t*>(regionBase);
                const wchar_t* searchStart = regionPtr + anchorPos;
                size_t remaining = regionWchars - anchorPos - (t.len - anchorPos);
                
                while (remaining > 0) {
                    const wchar_t* hit = wmemchr(searchStart, anchorChar, remaining);
                    if (!hit) break;
                    const wchar_t* candidate = hit - anchorPos;
                    if (candidate < regionPtr) {
                        searchStart = hit + 1;
                        remaining = (regionPtr + regionWchars) - searchStart - (t.len - anchorPos);
                        continue;
                    }
                    if (candidate[anchor2Pos] != anchor2Char) {
                        searchStart = hit + 1;
                        remaining = (regionPtr + regionWchars) - searchStart - (t.len - anchorPos);
                        continue;
                    }
                    if (wmemcmp(candidate, t.str, t.len) == 0) {
                        uintptr_t foundAddr = reinterpret_cast<uintptr_t>(candidate);
                        auto elapsed = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - scanStart).count();
                        QuickChatData::titleData.foundAddress = foundAddr;
                        QuickChatData::titleData.maxWriteLen = t.maxWriteLen;
                        QuickChatData::titleData.originalText = std::wstring(t.str, t.len);
                        LOG("TitleScanner: FOUND [{}] at 0x{:X} ({:.2f}s, {} regions, {})",
                            t.lang, foundAddr, elapsed, totalRegions, passName);
                        {
                            uintptr_t allocBase = reinterpret_cast<uintptr_t>(memInfo.AllocationBase);
                            LOG("  Region: Size=0x{:X} Prot=0x{:X} AllocProt=0x{:X} Base==Alloc:{} Offset=0x{:X}",
                                memInfo.RegionSize, memInfo.Protect, memInfo.AllocationProtect,
                                (allocBase == regionBase) ? "Y" : "N",
                                (foundAddr - regionBase));
                        }
                        if (gameWrapper) {
                            gameWrapper->Execute([](GameWrapper*) {
                                if (!QuickChatData::titleData.customText.empty()) {
                                    std::wstring newText = RLUtils::Utf8ToWide(QuickChatData::titleData.customText);
                                    MemoryUtils::WriteWideString(
                                        QuickChatData::titleData.foundAddress, newText,
                                        QuickChatData::titleData.maxWriteLen);
                                } else if (!QuickChatData::titleData.originalText.empty()) {
                                    MemoryUtils::WriteWideString(
                                        QuickChatData::titleData.foundAddress,
                                        QuickChatData::titleData.originalText,
                                        QuickChatData::titleData.maxWriteLen);
                                }
                                InvisibleHandleStatEvent::Trigger();
                            });
                        }
                        return true;
                    }
                    searchStart = hit + 1;
                    remaining = (regionPtr + regionWchars) - searchStart - (t.len - anchorPos);
                }
            }
            addr = regionBase + memInfo.RegionSize;
        }
        
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - scanStart).count();
        LOG("TitleScanner: {} miss ({:.2f}s, {} regions)", passName, elapsed, totalRegions);
        return false;
    }
    
    void ScanAsync()
    {
        if (isScanning) return;
        if (activeTarget < 0) return;
        isScanning = true;
        QuickChatData::titleData.foundAddress = 0;
        
        std::thread([]() {
            auto scanStart = std::chrono::steady_clock::now();
            const auto& t = targets[activeTarget];
            
            // Anchor chars for wmemchr
            size_t anchorPos = 0;
            wchar_t anchorChar = t.str[0];
            for (size_t k = 0; k < t.len; ++k) {
                if (t.str[k] > 0x7F) { anchorPos = k; anchorChar = t.str[k]; break; }
            }
            size_t anchor2Pos = (anchorPos == 0) ? (t.len - 1) : 0;
            wchar_t anchor2Char = t.str[anchor2Pos];
            
            // Delay to let the game write the title string
            LOG("TitleScanner: Waiting 3s...");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Up to 3 attempts with strict filters (Size==0x410000)
            for (int attempt = 0; attempt < 3; ++attempt) {
                if (attempt > 0) {
                    LOG("TitleScanner: Retry {} (waiting 3s)...", attempt + 1);
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                }
                if (ScanPass(t, anchorPos, anchorChar, anchor2Pos, anchor2Char,
                             0x410000, true, scanStart, attempt == 0 ? "fast" : "retry")) {
                    isScanning = false; return;
                }
            }
            // fallback
            // ScanPass(t, anchorPos, anchorChar, anchor2Pos, anchor2Char, 0, false, scanStart, "full");
            isScanning = false;
        }).detach();
    }
}

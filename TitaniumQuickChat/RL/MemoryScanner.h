#pragma once
#include <memory>

// Settings UI category scanner
namespace SettingsScanner
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Scan();
    bool IsScanned();
    bool IsUnsupportedLanguage();
}

// Shared memory utilities
namespace MemoryUtils
{
    void WriteWideString(uintptr_t address, const std::wstring& text, size_t maxLen);
}

// Quick Chat Title scanner
// Scan-only: populates QuickChatData::titleData
namespace TitleScanner
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void ScanAsync();
}

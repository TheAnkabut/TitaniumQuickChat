#pragma once
#include <memory>
#include <vector>
#include <string>

namespace HandleKeyPress
{
    enum class BindMode
    {
        Combine,     // + : All keys pressed together (Ctrl+F5)
        Progressive, // > : Keys in sequence with timing (Ctrl>F5)
        Sequence     // - : Keys in strict sequence (Ctrl-F5)
    };
    
    BindMode GetBindMode();
    void SetBindMode(BindMode mode);
    const char* GetModeSeparator();
    
    float GetProgressiveTiming();
    void SetProgressiveTiming(float seconds);
    float GetComboTiming();
    void SetComboTiming(float seconds);
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown(std::shared_ptr<GameWrapper> wrapper);
    
    void GetCurrentKeys(std::vector<std::string>& outKeys, bool excludeMouseButtons = false);
    
    // Binding string format: "Key1+Key2" (combo), "Key1>Key2" (progressive), "Key1-Key2" (sequence)
    bool IsBindingActive(const std::string& binding);
    
    bool DetectBinding(std::vector<std::string>& outActiveKeys, std::string& outFinalBinding);
    void ResetBindingState();
    std::string GetActiveKeysString();
    
    // Renders a bind button with capture logic
    // Returns true when a new binding was captured
    bool BindButton(const char* id, const char* label, std::string& binding);
    
} // namespace HandleKeyPress

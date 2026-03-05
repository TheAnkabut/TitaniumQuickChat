// HandleKeyPress.cpp - Input Detection via HandleKeyPress hook

#include "pch.h"
#include "HandleKeyPress.h"
#include "Internals.h"
#include <unordered_set>  
#include <unordered_map> 
#include <shared_mutex>
#include <mutex>
#include <algorithm>
#include <limits>

namespace HandleKeyPress
{
    // ==================== Input Event Types (from RLSDK) ====================
    
    enum class EInputEvent : uint8_t
    {
        IE_Pressed      = 0,
        IE_Released     = 1,
        IE_Repeat       = 2,
        IE_DoubleClick  = 3,
        IE_Axis         = 4
    };
    
    struct UGameViewportClient_TA_HandleKeyPress_Params
    {
        int32_t  ControllerId;                      
        FName    Key;                            
        uint8_t  EventType;                      
        uint8_t  padding0[0x3];                  
        float    AmountDepressed;                
        uint32_t bGamepad : 1;                   
        bool     ReturnValue : 1;                
    };
    
    // ==================== State - Key tracking ====================
    
    static std::unordered_set<int32_t> pressedKeyIds;
    static std::unordered_map<int32_t, double> keyPressTime;
    static std::shared_mutex keyStateMutex;
    
    static int32_t leftMouseButtonId = -1;
    static int32_t rightMouseButtonId = -1;
    
    // ==================== State - Bind mode ====================
    
    static BindMode currentBindMode = BindMode::Combine;
    static float progressiveTiming = 2.0f;
    static float comboTiming = 0.2f;
    
    // ==================== State - Binding detection ====================
    
    static bool bindingStarted = false;
    static std::vector<std::string> bindingKeys;
    static std::vector<std::string> activeKeys;
    static std::unordered_map<std::string, double> keyReleaseTime;
    
    // ==================== State - Binding activation ====================
    
    static std::unordered_map<std::string, size_t> bindingProgress;
    static std::unordered_map<std::string, double> bindingLastTime;
    static std::unordered_map<std::string, std::pair<std::vector<std::string>, char>> parsedBindings;
    static std::unordered_set<std::string> consumedBindings;  // Fired and waiting for all keys released
    
    // ==================== Bind Mode ====================
    
    BindMode GetBindMode()
    {
        return currentBindMode;
    }
    
    void SetBindMode(BindMode mode)
    {
        currentBindMode = mode;
    }
    
    const char* GetModeSeparator()
    {
        switch (currentBindMode)
        {
            case BindMode::Combine:     return "+";
            case BindMode::Progressive: return ">";
            case BindMode::Sequence:    return "-";
        }
        return "+";
    }
    
    float GetProgressiveTiming()
    {
        return progressiveTiming;
    }
    
    void SetProgressiveTiming(float seconds)
    {
        progressiveTiming = std::clamp(seconds, 0.5f, 10.0f);
    }
    
    float GetComboTiming()
    {
        return comboTiming;
    }
    
    void SetComboTiming(float seconds)
    {
        comboTiming = std::clamp(seconds, 0.1f, 0.5f);
    }
    
    // ==================== Key State ====================
    
    void GetCurrentKeys(std::vector<std::string>& outKeys, bool excludeMouseButtons)
    {
        std::shared_lock lock(keyStateMutex);
        outKeys.clear();
        outKeys.reserve(pressedKeyIds.size());
        
        TArray<FNameEntry*>* GNames = FName::Names();
        
        for (int32_t keyId : pressedKeyIds)
        {
            if (excludeMouseButtons && 
                (keyId == leftMouseButtonId || keyId == rightMouseButtonId))
            {
                continue;
            }
            
            if (keyId >= 0 && keyId < GNames->size())
            {
                FName fname;
                fname.FNameEntryId = keyId;
                fname.InstanceNumber = 0;
                outKeys.push_back(fname.ToString());
            }
        }
    }
    
    // ==================== Binding Detection ====================
    
    void ResetBindingState()
    {
        bindingStarted = false;
        bindingKeys.clear();
        activeKeys.clear();
        keyReleaseTime.clear();
    }
    
    std::string GetActiveKeysString()
    {
        if (activeKeys.empty()) return "";
        
        const char* sep = GetModeSeparator();
        std::string result;
        for (size_t i = 0; i < activeKeys.size(); i++) {
            if (i > 0) result += sep;
            result += activeKeys[i];
        }
        return result;
    }
    
    bool DetectBinding(std::vector<std::string>& outActiveKeys, std::string& outFinalBinding)
    {
        outFinalBinding.clear();
        
        std::vector<std::string> currentKeys;
        GetCurrentKeys(currentKeys, true);
        
        double currentTime = ImGui::GetTime();
        
        if (!currentKeys.empty())
        {
            if (!bindingStarted)
            {
                bindingStarted = true;
                bindingKeys = currentKeys;
                activeKeys = currentKeys;
                keyReleaseTime.clear();
            }
            else
            {
                for (const auto& key : currentKeys)
                {
                    if (std::find(bindingKeys.begin(), bindingKeys.end(), key) == bindingKeys.end())
                    {
                        bindingKeys.push_back(key);
                    }
                    keyReleaseTime.erase(key);
                }
                
                for (const auto& key : activeKeys)
                {
                    if (std::find(currentKeys.begin(), currentKeys.end(), key) == currentKeys.end() &&
                        keyReleaseTime.find(key) == keyReleaseTime.end())
                    {
                        keyReleaseTime[key] = currentTime;
                    }
                }
                
                std::vector<std::string> newActiveKeys;
                for (const auto& key : bindingKeys)
                {
                    bool isPressed = std::find(currentKeys.begin(), currentKeys.end(), key) != currentKeys.end();
                    auto releaseIt = keyReleaseTime.find(key);
                    
                    if (isPressed || (releaseIt != keyReleaseTime.end() && (currentTime - releaseIt->second) < progressiveTiming))
                    {
                        newActiveKeys.push_back(key);
                    }
                }
                activeKeys = newActiveKeys;
            }
            
            outActiveKeys = activeKeys;
            return true;
        }
        else if (bindingStarted && !activeKeys.empty())
        {
            const char* sep = GetModeSeparator();
            for (size_t i = 0; i < activeKeys.size(); i++)
            {
                if (i > 0) outFinalBinding += sep;
                outFinalBinding += activeKeys[i];
            }
            
            outActiveKeys = activeKeys;
            ResetBindingState();
            return false;
        }
        
        outActiveKeys = activeKeys;
        return true;
    }
    
    // ==================== Binding Activation - Combo (+) ====================
    
    static void ParseBinding(const std::string& binding, std::vector<std::string>& keys, char& sep)
    {
        auto it = parsedBindings.find(binding);
        if (it != parsedBindings.end())
        {
            keys = it->second.first;
            sep = it->second.second;
            return;
        }
        
        keys.clear();
        sep = '+';
        
        if (binding.empty()) return;
        
        if (binding.find('>') != std::string::npos)
        {
            sep = '>';
        }
        else if (binding.find('-') != std::string::npos)
        {
            sep = '-';
        }
        
        std::string current;
        for (char c : binding)
        {
            if (c == '+' || c == '>' || c == '-')
            {
                if (!current.empty())
                {
                    keys.push_back(current);
                }
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        if (!current.empty())
        {
            keys.push_back(current);
        }
        
        parsedBindings[binding] = {keys, sep};
    }
    
    static bool IsComboActive(const std::vector<std::string>& keys, const std::vector<std::string>& currentKeys)
    {
        for (const auto& key : keys)
        {
            if (std::find(currentKeys.begin(), currentKeys.end(), key) == currentKeys.end())
            {
                return false;
            }
        }
        
        double earliest = std::numeric_limits<double>::max();
        double latest = 0.0;
        
        std::shared_lock lock(keyStateMutex);
        for (const auto& keyName : keys)
        {
            FName fname;
            if (!Internals::GetFNameByString(keyName.c_str(), fname))
            {
                return false;
            }
            
            auto it = keyPressTime.find(fname.FNameEntryId);
            if (it == keyPressTime.end())
            {
                return false;
            }
            
            earliest = std::min(earliest, it->second);
            latest = std::max(latest, it->second);
        }
        
        return (latest - earliest) <= comboTiming;
    }
    
    // ==================== Binding Activation - Progressive (>) ====================
    
    static bool IsProgressiveActive(const std::string& binding, const std::vector<std::string>& keys, 
                                     const std::vector<std::string>& currentKeys, double currentTime)
    {
        size_t& progress = bindingProgress[binding];
        double& lastTime = bindingLastTime[binding];
        
        if (progress > 0 && currentTime - lastTime > progressiveTiming)
        {
            progress = 0;
        }
        
        for (size_t i = 0; i < progress && i < keys.size(); ++i)
        {
            if (std::find(currentKeys.begin(), currentKeys.end(), keys[i]) == currentKeys.end())
            {
                progress = 0;
                break;
            }
        }
        
        if (progress < keys.size())
        {
            bool nextPressed = std::find(currentKeys.begin(), currentKeys.end(), keys[progress]) != currentKeys.end();
            
            if (nextPressed)
            {
                bool prevHeld = true;
                for (size_t i = 0; i < progress; ++i)
                {
                    if (std::find(currentKeys.begin(), currentKeys.end(), keys[i]) == currentKeys.end())
                    {
                        prevHeld = false;
                        break;
                    }
                }
                
                if (progress == 0 || prevHeld)
                {
                    progress++;
                    lastTime = currentTime;
                    
                    if (progress >= keys.size())
                    {
                        progress = 0;
                        return true;
                    }
                }
            }
        }
        
        return false;
    }
    
    // ==================== Binding Activation - Sequence (-) ====================
    
    static bool IsSequenceActive(const std::string& binding, const std::vector<std::string>& keys,
                                  const std::vector<std::string>& currentKeys)
    {
        size_t& progress = bindingProgress[binding];
        
        if (progress >= keys.size())
        {
            progress = 0;
        }
        
        bool nextPressed = std::find(currentKeys.begin(), currentKeys.end(), keys[progress]) != currentKeys.end();
        
        static std::unordered_map<std::string, bool> wasPressed;
        std::string stateKey = binding + ":" + keys[progress];
        
        if (nextPressed && !wasPressed[stateKey])
        {
            progress++;
            wasPressed[stateKey] = nextPressed;
            
            if (progress >= keys.size())
            {
                progress = 0;
                return true;
            }
        }
        
        wasPressed[stateKey] = nextPressed;
        return false;
    }
    
    // ==================== Binding Activation - Main Check ====================
    
    bool IsBindingActive(const std::string& binding)
    {
        if (binding.empty()) return false;
        
        std::vector<std::string> keys;
        char sep;
        ParseBinding(binding, keys, sep);
        
        if (keys.empty()) return false;
        
        std::vector<std::string> currentKeys;
        GetCurrentKeys(currentKeys);
        double currentTime = ImGui::GetTime();
        
        // If consumed, wait until ALL keys are released before allowing re-activation
        if (consumedBindings.count(binding))
        {
            bool anyHeld = false;
            for (const auto& key : keys)
            {
                if (std::find(currentKeys.begin(), currentKeys.end(), key) != currentKeys.end())
                {
                    anyHeld = true;
                    break;
                }
            }
            if (anyHeld) return false;  // Still holding, stay consumed
            consumedBindings.erase(binding);  // All released, allow re-activation
        }
        
        bool result = false;
        switch (sep)
        {
            case '+': result = IsComboActive(keys, currentKeys); break;
            case '>': result = IsProgressiveActive(binding, keys, currentKeys, currentTime); break;
            case '-': return IsSequenceActive(binding, keys, currentKeys);  // Sequence has its own edge detection
        }
        
        if (result)
        {
            consumedBindings.insert(binding);
        }
        return result;
    }
    
    // ==================== Initialization ====================
    
    void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        if (Internals::IsInitialized())
        {
            FName leftMouse, rightMouse;
            if (Internals::GetFNameByString("LeftMouseButton", leftMouse))
            {
                leftMouseButtonId = leftMouse.FNameEntryId;
            }
            if (Internals::GetFNameByString("RightMouseButton", rightMouse))
            {
                rightMouseButtonId = rightMouse.FNameEntryId;
            }
        }
        
        wrapper->HookEventWithCaller<ActorWrapper>(
            "Function TAGame.GameViewportClient_TA.HandleKeyPress",
            [](ActorWrapper caller, void* params, std::string eventName)
            {
                if (!params) return;
                
                auto* data = reinterpret_cast<UGameViewportClient_TA_HandleKeyPress_Params*>(params);
                if (data->Key.FNameEntryId == 0) return;
                
                EInputEvent eventType = static_cast<EInputEvent>(data->EventType);
                
                if (eventType != EInputEvent::IE_Pressed && 
                    eventType != EInputEvent::IE_Released)
                {
                    return;
                }
                
                const int32_t keyId = data->Key.FNameEntryId;
                double currentTime = ImGui::GetTime();
                
                std::unique_lock lock(keyStateMutex);
                if (eventType == EInputEvent::IE_Pressed)
                {
                    pressedKeyIds.insert(keyId);
                    keyPressTime[keyId] = currentTime;
                }
                else
                {
                    pressedKeyIds.erase(keyId);
                    keyPressTime.erase(keyId);
                }
            }
        );
        
        LOG("HandleKeyPress::Initialize - OK");
    }
    
    void Shutdown(std::shared_ptr<GameWrapper> wrapper)
    {
        wrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");
        
        std::unique_lock lock(keyStateMutex);
        pressedKeyIds.clear();
        keyPressTime.clear();
        bindingProgress.clear();
        bindingLastTime.clear();
        parsedBindings.clear();
        consumedBindings.clear();
        
        ResetBindingState();
        
        LOG("HandleKeyPress::Shutdown - OK");
    }
    
    // ==================== UI Helpers - BindButton ====================
    
    static std::string capturingButtonId = "";  // Which button is currently capturing (empty = none)
    
    bool BindButton(const char* id, const char* label, std::string& binding)
    {
        bool captured = false;
        bool isCapturing = (capturingButtonId == id);
        
        std::vector<std::string> localActiveKeys;
        std::string finalBinding;
        
        // If this button is capturing, process detection
        if (isCapturing)
        {
            // Tell ImGui to NOT capture keyboard/mouse input!
            // This allows the game's HandleKeyPress hook to receive events
            ImGui::CaptureKeyboardFromApp(false);
            ImGui::CaptureMouseFromApp(false);
            
            bool stillDetecting = DetectBinding(localActiveKeys, finalBinding);
            
            if (!stillDetecting && !finalBinding.empty())
            {
                binding = finalBinding;
                capturingButtonId = "";
                captured = true;
            }
        }
        
        // Build button label
        std::string buttonLabel = label;
        if (isCapturing && !localActiveKeys.empty())
        {
            buttonLabel += GetActiveKeysString();
        }
        else
        {
            buttonLabel += binding.empty() ? "..." : binding;
        }
        buttonLabel += id;
        
        // Pulsing style when capturing
        if (isCapturing)
        {
            float pulse = 0.5f + 0.4f * std::sin(static_cast<float>(ImGui::GetTime()) * 6);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, pulse));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, pulse));
        }
        
        bool clicked = ImGui::SmallButton(buttonLabel.c_str());
        
        if (isCapturing)
        {
            ImGui::PopStyleColor(2);
        }
        else if (clicked)
        {
            // Cancel any other button's capture and start ours
            if (!capturingButtonId.empty())
            {
                ResetBindingState();
            }
            capturingButtonId = id;
        }
        
        return captured;
    }
    
} // namespace HandleKeyPress

#pragma once
#include <string>

namespace Loadout
{
    // Encode current profile state -> shareable code string
    std::string Encode();
    
    // Decode code string -> apply to current profile
    // True on success, false on invalid code
    bool Decode(const std::string& code);
    
    void OpenLoadModal();
    
    // Call from Profile::Render
    void RenderLoadModal();
}

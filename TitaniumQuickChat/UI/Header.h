#pragma once

namespace Header
{
    void Render();
    int GetViewMode();
    void SetViewMode(int mode);
    int GetBindMode();
    void SetBindMode(int mode);
    float GetComboTiming();
    void SetComboTiming(float timing);
    float GetProgressiveTiming();
    void SetProgressiveTiming(float timing);
    bool GetCustomChannelPerQC();
    void SetCustomChannelPerQC(bool value);
}

#pragma once
#include <memory>

namespace RefreshSettings
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    bool Refresh();
    void RefreshDelayed();
}

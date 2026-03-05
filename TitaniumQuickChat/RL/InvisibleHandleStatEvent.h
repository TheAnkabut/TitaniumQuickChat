#pragma once
#include <memory>

namespace InvisibleHandleStatEvent
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Trigger();
    void ResetCache();
}


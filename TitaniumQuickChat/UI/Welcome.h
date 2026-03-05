#pragma once

#include <memory>

namespace Welcome
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Render();
    bool HasProfile();
    void SetHasProfile(bool value);
}

#pragma once
#include <memory>

namespace SettingsReplacer
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Shutdown(std::shared_ptr<GameWrapper> wrapper);
}

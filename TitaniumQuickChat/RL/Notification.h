#pragma once
#include <memory>
#include <string>

namespace Notification
{
    void Initialize(std::shared_ptr<GameWrapper> wrapper);
    void Show(const std::string& title, const std::string& body);
}

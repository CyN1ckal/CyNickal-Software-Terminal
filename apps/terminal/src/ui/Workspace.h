#pragma once

#include "imgui.h"

namespace myapp {

class Workspace {
public:
    static void draw();
    [[nodiscard]] static const ImVec4& clearColor() noexcept;
};

}  // namespace myapp

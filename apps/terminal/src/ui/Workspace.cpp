#include "ui/Workspace.h"

#include "ui/Theme.h"

#include "imgui_internal.h"

namespace myapp {
namespace {

constexpr float kDataPanelWidthRatio = 0.30f;

void applyDefaultDockLayout(ImGuiID dockspace_id, const ImVec2& size)
{
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    if (node != nullptr && node->IsSplitNode())
    {
        return;
    }

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, size);

    ImGuiID left = 0;
    ImGuiID rest = 0;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, kDataPanelWidthRatio, &left, &rest);
    ImGui::DockBuilderDockWindow("DATA", left);
    ImGui::DockBuilderFinish(dockspace_id);
}

}  // namespace

const ImVec4& Workspace::clearColor() noexcept
{
    return Theme::kCanvas;
}

void Workspace::draw()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Workspace", nullptr, flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dock_id = ImGui::GetID("WorkspaceDock");
    if (!dock_layout_applied_)
    {
        applyDefaultDockLayout(dock_id, viewport->WorkSize);
        dock_layout_applied_ = true;
    }
    ImGui::DockSpace(dock_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    inventory_.draw();
}

}  // namespace myapp

// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudySettings.h"

#include "chart/CStudyCompute.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

namespace terminal {
namespace {

[[nodiscard]] const char* sourceLabel(StudySource source) noexcept
{
    switch (source)
    {
    case StudySource::Open:
        return "Open";
    case StudySource::High:
        return "High";
    case StudySource::Low:
        return "Low";
    case StudySource::Close:
        return "Close";
    }
    return "Close";
}

[[nodiscard]] bool drawMovingAverageParams(CStudyInstance& inst, MovingAverageParams& params)
{
    ImGui::TextUnformatted("Input Data");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    char source_id[64];
    std::snprintf(source_id, sizeof(source_id), "##study_source_%d", inst.id);
    if (ImGui::BeginCombo(source_id, sourceLabel(params.source)))
    {
        if (ImGui::Selectable("Close", params.source == StudySource::Close))
        {
            params.source = StudySource::Close;
        }
        if (ImGui::Selectable("Open", params.source == StudySource::Open))
        {
            params.source = StudySource::Open;
        }
        if (ImGui::Selectable("High", params.source == StudySource::High))
        {
            params.source = StudySource::High;
        }
        if (ImGui::Selectable("Low", params.source == StudySource::Low))
        {
            params.source = StudySource::Low;
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted("Length");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    char len_id[64];
    std::snprintf(len_id, sizeof(len_id), "##study_len_%d", inst.id);
    // step 0: InputScalar's +/- buttons also return true when EnterReturnsTrue is set.
    // Live-edit writes each keystroke. Selecting another row or Add runs before this
    // widget is submitted, and a deactivate-only write would drop the typed length.
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    const bool length_enter =
        ImGui::InputInt(len_id, &params.length, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemFlag();

    ImGui::TextUnformatted("Method");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    char method_id[64];
    std::snprintf(method_id, sizeof(method_id), "##study_method_%d", inst.id);
    if (ImGui::BeginCombo(method_id, "Simple"))
    {
        ImGui::Selectable("Simple", true);
        ImGui::EndCombo();
    }
    ImGui::TextColored(Theme::kMuted, "v1: simple moving average");

    const ImVec4 current = ImGui::ColorConvertU32ToFloat4(inst.color);
    float rgba[4] = {current.x, current.y, current.z, current.w};
    char color_id[64];
    std::snprintf(color_id, sizeof(color_id), "Color##study_color_%d", inst.id);
    if (ImGui::ColorEdit4(color_id, rgba))
    {
        const ImVec4 edited(rgba[0], rgba[1], rgba[2], rgba[3]);
        inst.color = ImGui::ColorConvertFloat4ToU32(edited);
    }
    return length_enter;
}

[[nodiscard]] bool drawSelectedParams(CStudyInstance& inst)
{
    switch (inst.kind)
    {
    case StudyKind::MovingAverage:
    {
        auto* params = std::get_if<MovingAverageParams>(&inst.params);
        if (params == nullptr)
        {
            return false;
        }
        return drawMovingAverageParams(inst, *params);
    }
    }
    return false;
}

}  // namespace

bool drawStudyDraftBody(std::vector<CStudyInstance>& draft, int& selected, int& next_id)
{
    int remove_index = -1;
    const auto row_count = static_cast<int>(draft.size());
    for (int i = 0; i < row_count; ++i)
    {
        CStudyInstance& inst = draft[static_cast<std::size_t>(i)];
        ImGui::PushID(inst.id);
        ImGui::Checkbox("##en", &inst.enabled);
        ImGui::SameLine();
        const ImVec4 swatch = ImGui::ColorConvertU32ToFloat4(inst.color);
        ImGui::ColorButton("##swatch", swatch,
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
        ImGui::SameLine();

        const std::string short_label = studyShortLabel(inst);
        std::string row_label = short_label;
        row_label += "##study_row_";
        row_label += std::to_string(inst.id);
        const float text_w = ImGui::CalcTextSize(short_label.c_str()).x;
        const float select_width = text_w > 0.0f ? text_w : ImGui::GetFrameHeight();
        // A Selectable in a modal window closes the popup unless this flag is set.
        if (ImGui::Selectable(row_label.c_str(), selected == i, ImGuiSelectableFlags_NoAutoClosePopups,
                              ImVec2(select_width, ImGui::GetFrameHeight())))
        {
            selected = i;
        }
        ImGui::SameLine();
        char remove_label[64];
        std::snprintf(remove_label, sizeof(remove_label), "Remove##study_rm_%d", inst.id);
        if (ImGui::Button(remove_label))
        {
            remove_index = i;
        }
        ImGui::PopID();
    }

    if (remove_index >= 0)
    {
        draft.erase(draft.begin() + remove_index);
        if (draft.empty())
        {
            selected = -1;
        }
        else if (remove_index < selected)
        {
            --selected;
        }
        else
        {
            selected = std::min(selected, static_cast<int>(draft.size()) - 1);
        }
    }

    constexpr auto type_count = static_cast<int>(std::size(kStudyTypes));
    int add_index = 0;
    const bool at_cap = static_cast<int>(draft.size()) >= kStudyMaxPerPane;
    ImGui::BeginDisabled(at_cap || type_count <= 0);
    if (type_count > 0)
    {
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("##study_add", kStudyTypes[add_index].display_name))
        {
            for (int i = 0; i < type_count; ++i)
            {
                const bool chosen = ImGui::Selectable(kStudyTypes[i].display_name, add_index == i);
                if (chosen)
                {
                    add_index = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
    }
    const bool add_clicked = ImGui::Button("Add");
    if (!at_cap && add_clicked && add_index >= 0 && add_index < type_count)
    {
        const StudyTypeInfo& info = kStudyTypes[add_index];
        CStudyInstance inst;
        inst.id = next_id++;
        inst.kind = info.kind;
        inst.params = defaultParams(info.kind);
        inst.color = studyPaletteColor(static_cast<int>(draft.size()));
        draft.push_back(inst);
        selected = static_cast<int>(draft.size()) - 1;
    }
    ImGui::EndDisabled();
    if (at_cap)
    {
        ImGui::SameLine();
        ImGui::TextColored(Theme::kMuted, "maximum %d studies", kStudyMaxPerPane);
    }

    bool length_enter = false;
    if (selected >= 0 && selected < static_cast<int>(draft.size()))
    {
        ImGui::Separator();
        length_enter = drawSelectedParams(draft[static_cast<std::size_t>(selected)]);
    }
    return length_enter;
}

}  // namespace terminal

// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudySettings.h"

#include "chart/CStudyCompute.h"
#include "chart/studies/StudyRegistry.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr float kStudyListFraction = 0.38f;
// Value editors stay this wide. The rest of the settings pane stays empty.
constexpr float kStudyValueWidth = 220.0f;
constexpr int kAddStudyRowFloor = 6;
// Existing Add Study limits, counted in frame heights: min width 12, max width 28, max height 32.
constexpr int kAddStudyWindowMaxRows = 32;
constexpr float kAddStudyMinWidthRows = 12.0f;
constexpr float kAddStudyMaxWidthRows = 28.0f;

[[nodiscard]] float equalButtonWidth(const char* a, const char* b)
{
    const float text = std::max(ImGui::CalcTextSize(a).x, ImGui::CalcTextSize(b).x);
    return text + (ImGui::GetStyle().FramePadding.x * 2.0f);
}

[[nodiscard]] float equalButtonWidth(const char* a, const char* b, const char* c)
{
    const float text =
        std::max({ImGui::CalcTextSize(a).x, ImGui::CalcTextSize(b).x, ImGui::CalcTextSize(c).x});
    return text + (ImGui::GetStyle().FramePadding.x * 2.0f);
}

[[nodiscard]] float buttonClusterWidth(float button_w, int count)
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x * static_cast<float>(count - 1);
    return (button_w * static_cast<float>(count)) + spacing;
}

void alignButtonCluster(float cluster_w)
{
    const float align_x = ImGui::GetContentRegionMax().x - cluster_w;
    if (align_x > ImGui::GetCursorPosX())
    {
        ImGui::SetCursorPosX(align_x);
    }
}

// default_focus is OK only. Enter still submits Length and does not press this button.
[[nodiscard]] bool drawAccentButton(const char* label, float width, bool default_focus)
{
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::kAccentPressed);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
    if (default_focus)
    {
        ImGui::SetItemDefaultFocus();
    }
    ImGui::PopStyleColor(4);
    return pressed;
}

[[nodiscard]] bool drawPlainButton(const char* label, float width)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kText);
    const bool pressed = ImGui::Button(label, ImVec2(width, 0.0f));
    ImGui::PopStyleColor();
    return pressed;
}

[[nodiscard]] const char* studyTypeName(const CStudyInstance& inst) noexcept
{
    if (const StudyType* type = findStudy(inst.type_id))
    {
        return type->display_name;
    }
    return "Study";
}

[[nodiscard]] bool selectionInRange(const std::vector<CStudyInstance>& draft, int selected) noexcept
{
    return selected >= 0 && std::cmp_less(selected, draft.size());
}

void drawMutedWrapped(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kMuted);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void drawPaneTitle(const char* title)
{
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kTextDim);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void drawCenteredMuted(const char* text)
{
    const ImVec2 size = ImGui::CalcTextSize(text);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 start = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(start.x + std::max(0.0f, (avail.x - size.x) * 0.5f),
                               start.y + std::max(0.0f, (avail.y - size.y) * 0.5f)));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kMuted);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void drawPropertyLabel(const char* label)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kTextDim);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::TableNextColumn();
}

[[nodiscard]] bool drawStudyOptions(CStudyInstance& inst, const StudyType& type)
{
    bool submitted = false;
    if (inst.options.size() != type.options.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        const StudyOption& option = type.options[index];
        if (!option.shown)
        {
            continue;
        }
        int& value = inst.options[index];
        drawPropertyLabel(option.label);
        ImGui::SetNextItemWidth(kStudyValueWidth);
        char field_id[96];
        std::snprintf(field_id, sizeof(field_id), "##study_%d_%s", inst.id, option.key);
        if (option.choices.empty())
        {
            // step 0: InputScalar's +/- buttons also return true when EnterReturnsTrue is set.
            // Live-edit writes each keystroke. The study list is drawn before this widget, so
            // a deactivate-only write would drop the value when the selection changes.
            ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
            const bool entered =
                ImGui::InputInt(field_id, &value, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemFlag();
            submitted = submitted || entered;
            continue;
        }
        const auto count = static_cast<int>(option.choices.size());
        const char* preview = "";
        if (value >= 0 && value < count)
        {
            preview = option.choices[static_cast<std::size_t>(value)].label;
        }
        if (!ImGui::BeginCombo(field_id, preview))
        {
            continue;
        }
        for (int choice = 0; choice < count; ++choice)
        {
            const StudyChoice& item = option.choices[static_cast<std::size_t>(choice)];
            ImGui::PushID(choice);
            if (ImGui::Selectable(item.label, value == choice))
            {
                value = choice;
                if (item.leave_price_scale && inst.chart_region == kStudyMainChartRegion)
                {
                    inst.chart_region = kStudyVolumeChartRegion;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return submitted;
}

void drawChartRegionField(CStudyInstance& inst)
{
    inst.chart_region = clampStudyChartRegion(inst.chart_region);
    drawPropertyLabel("Chart Region");
    ImGui::SetNextItemWidth(kStudyValueWidth);
    char region_id[64];
    std::snprintf(region_id, sizeof(region_id), "##study_region_%d", inst.id);
    char preview[64];
    if (inst.chart_region == kStudyMainChartRegion)
    {
        std::snprintf(preview, sizeof(preview), "1  Main Price Graph");
    }
    else
    {
        std::snprintf(preview, sizeof(preview), "%d", inst.chart_region);
    }
    if (ImGui::BeginCombo(region_id, preview))
    {
        for (int region = kStudyChartRegionMin; region <= kStudyChartRegionMax; ++region)
        {
            char item[64];
            if (region == kStudyMainChartRegion)
            {
                std::snprintf(item, sizeof(item), "1  Main Price Graph");
            }
            else
            {
                std::snprintf(item, sizeof(item), "%d", region);
            }
            if (ImGui::Selectable(item, inst.chart_region == region))
            {
                inst.chart_region = region;
            }
        }
        ImGui::EndCombo();
    }
    // Dim suffix in the empty pane, not a second paragraph under the table.
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kTextDim);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted("Region 1 is the main price graph. Regions 2-12 are panes below it.");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void drawOutputColorField(CStudyInstance& inst, std::size_t index)
{
    CStudyOutputStyle& style = inst.outputs[index];
    const ImVec4 current = ImGui::ColorConvertU32ToFloat4(style.color);
    float rgba[4] = {current.x, current.y, current.z, current.w};
    char color_id[96];
    std::snprintf(color_id, sizeof(color_id), "Color##study_color_%d_%d", inst.id,
                  static_cast<int>(index));
    if (ImGui::ColorEdit4(color_id, rgba,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_AlphaBar))
    {
        const ImVec4 edited(rgba[0], rgba[1], rgba[2], rgba[3]);
        style.color = ImGui::ColorConvertFloat4ToU32(edited);
        if (index == 0)
        {
            inst.color = style.color;
        }
    }
}

void drawLineStyleField(CStudyInstance& inst, std::size_t index, StudyGraph graph)
{
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kTextDim);
    ImGui::TextUnformatted("Style");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x));
    CStudyOutputStyle& style = inst.outputs[index];
    char field_id[96];
    std::snprintf(field_id, sizeof(field_id), "##study_line_%d_%d", inst.id, static_cast<int>(index));
    const bool value = style.line == StudyLineStyle::Value;
    const char* preview = studyLineStyleLabel(style.line);
    if (graph == StudyGraph::Histogram && !value)
    {
        preview = "Histogram";
    }
    if (!ImGui::BeginCombo(field_id, preview))
    {
        return;
    }
    if (graph == StudyGraph::Histogram)
    {
        if (ImGui::Selectable("Histogram", !value))
        {
            style.line = StudyLineStyle::Solid;
        }
        if (ImGui::Selectable(studyLineStyleLabel(StudyLineStyle::Value), value))
        {
            style.line = StudyLineStyle::Value;
        }
        ImGui::EndCombo();
        return;
    }
    constexpr StudyLineStyle kStyles[] = {
        StudyLineStyle::Solid,
        StudyLineStyle::Dotted,
        StudyLineStyle::Dashed,
        StudyLineStyle::Value,
    };
    for (const StudyLineStyle choice : kStyles)
    {
        ImGui::PushID(static_cast<int>(choice));
        if (ImGui::Selectable(studyLineStyleLabel(choice), style.line == choice))
        {
            style.line = choice;
        }
        ImGui::PopID();
    }
    ImGui::EndCombo();
}

void drawOutputStyleFields(CStudyInstance& inst, const StudyType& type)
{
    if (type.outputs.empty())
    {
        drawPropertyLabel("Color");
        const ImVec4 current = ImGui::ColorConvertU32ToFloat4(inst.color);
        float rgba[4] = {current.x, current.y, current.z, current.w};
        char color_id[64];
        std::snprintf(color_id, sizeof(color_id), "Color##study_color_%d", inst.id);
        if (ImGui::ColorEdit4(color_id, rgba,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel |
                                  ImGuiColorEditFlags_AlphaBar))
        {
            const ImVec4 edited(rgba[0], rgba[1], rgba[2], rgba[3]);
            inst.color = ImGui::ColorConvertFloat4ToU32(edited);
        }
        return;
    }
    normalizeStudyOutputs(inst);
    for (std::size_t index = 0; index < type.outputs.size(); ++index)
    {
        const char* label = "Color";
        if (type.outputs[index].label != nullptr && type.outputs[index].label[0] != '\0')
        {
            label = type.outputs[index].label;
        }
        drawPropertyLabel(label);
        drawOutputColorField(inst, index);
        drawLineStyleField(inst, index, type.graph);
    }
}

[[nodiscard]] float studyPropertyLabelWidth(const StudyType& type)
{
    float width = ImGui::CalcTextSize("Chart Region").x;
    width = std::max(width, ImGui::CalcTextSize("Color").x);
    for (const StudyOption& option : type.options)
    {
        if (!option.shown || option.label == nullptr)
        {
            continue;
        }
        width = std::max(width, ImGui::CalcTextSize(option.label).x);
    }
    for (const StudyOutput& output : type.outputs)
    {
        if (output.label == nullptr || output.label[0] == '\0')
        {
            continue;
        }
        width = std::max(width, ImGui::CalcTextSize(output.label).x);
    }
    return width + ImGui::GetStyle().FramePadding.x;
}

[[nodiscard]] bool drawStudyProperties(CStudyInstance& inst)
{
    const StudyType* type = findStudy(inst.type_id);
    if (type == nullptr)
    {
        drawMutedWrapped("This study is not available.");
        return false;
    }
    if (type->note != nullptr)
    {
        drawMutedWrapped(type->note);
        ImGui::Spacing();
    }

    bool length_enter = false;
    const float label_w = studyPropertyLabelWidth(*type);
    const ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp |
                                        ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##study_props", 3, table_flags))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, label_w);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, kStudyValueWidth);
        ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch);
        length_enter = drawStudyOptions(inst, *type);
        drawChartRegionField(inst);
        drawOutputStyleFields(inst, *type);
        ImGui::EndTable();
    }
    return length_enter;
}

void appendStudy(std::vector<CStudyInstance>& draft, int& selected, int& next_id, const StudyType& type)
{
    if (static_cast<int>(draft.size()) >= kStudyMaxPerPane)
    {
        return;
    }
    CStudyInstance inst;
    inst.id = next_id++;
    inst.type_id = type.id != nullptr ? type.id : "";
    inst.chart_region = type.default_chart_region;
    inst.options.resize(type.options.size());
    for (std::size_t index = 0; index < type.options.size(); ++index)
    {
        inst.options[index] = type.options[index].fallback;
    }
    assignStudyOutputDefaults(inst, static_cast<int>(draft.size()));
    draft.push_back(std::move(inst));
    selected = static_cast<int>(draft.size()) - 1;
}

void removeSelected(std::vector<CStudyInstance>& draft, int& selected)
{
    if (!selectionInRange(draft, selected))
    {
        return;
    }
    draft.erase(draft.begin() + selected);
    if (draft.empty())
    {
        selected = -1;
        return;
    }
    selected = std::min(selected, static_cast<int>(draft.size()) - 1);
}

void drawStudyRow(CStudyInstance& inst, int index, int& selected)
{
    ImGui::PushID(inst.id);
    const ImGuiStyle& style = ImGui::GetStyle();
    const float row_h = ImGui::GetFrameHeight();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 row_max(origin.x + width, origin.y + row_h);
    const bool is_selected = selected == index;
    const bool hovered =
        ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, row_max);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (is_selected || hovered)
    {
        ImGuiCol fill_col = ImGuiCol_HeaderHovered;
        if (is_selected && hovered)
        {
            fill_col = ImGuiCol_HeaderActive;
        }
        else if (is_selected)
        {
            fill_col = ImGuiCol_Header;
        }
        draw->AddRectFilled(origin, row_max, ImGui::GetColorU32(fill_col));
    }

    if (ImGui::Checkbox("##en", &inst.enabled))
    {
        selected = index;
    }
    ImGui::SameLine();
    const std::size_t swatch_count = inst.outputs.empty() ? 1 : inst.outputs.size();
    for (std::size_t swatch = 0; swatch < swatch_count; ++swatch)
    {
        const std::uint32_t packed =
            swatch < inst.outputs.size() ? inst.outputs[swatch].color : inst.color;
        ImGui::PushID(static_cast<int>(swatch));
        if (ImGui::ColorButton("##swatch", ImGui::ColorConvertU32ToFloat4(packed),
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoPicker,
                               ImVec2(row_h, row_h)))
        {
            selected = index;
        }
        ImGui::PopID();
        ImGui::SameLine();
    }

    const char* type_name = studyTypeName(inst);
    const std::string short_label = studyShortLabel(inst);
    const ImVec2 hit_origin = ImGui::GetCursorScreenPos();
    const float remain = std::max(0.0f, ImGui::GetContentRegionAvail().x);
    const float content_right = hit_origin.x + remain;
    const float name_w = ImGui::CalcTextSize(type_name).x;
    const float short_w = ImGui::CalcTextSize(short_label.c_str()).x;
    const float gap = style.ItemSpacing.x * 2.0f;
    const float text_pad = style.FramePadding.x * 2.0f;
    const bool show_both =
        !short_label.empty() && name_w + gap + short_w + text_pad <= remain;
    const bool short_only = !show_both && !short_label.empty() && short_w + text_pad <= remain;
    const char* primary = short_only ? short_label.c_str() : type_name;

    // The hit target starts after the checkbox and swatch so those keep their clicks.
    // A Selectable inside the modal closes it unless NoAutoClosePopups is set.
    const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Header, clear);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, clear);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, clear);
    const ImVec2 text_pos(hit_origin.x + style.FramePadding.x, hit_origin.y + style.FramePadding.y);
    if (ImGui::Selectable("##hit", false, ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(0.0f, row_h)))
    {
        selected = index;
    }
    ImGui::PopStyleColor(3);

    const ImU32 primary_col = ImGui::GetColorU32(inst.enabled ? Theme::kText : Theme::kTextDim);
    if (show_both)
    {
        const float short_x = content_right - style.FramePadding.x - short_w;
        const float clip_right = short_x - gap;
        if (clip_right > text_pos.x)
        {
            draw->PushClipRect(ImVec2(text_pos.x, origin.y), ImVec2(clip_right, row_max.y), true);
            draw->AddText(text_pos, primary_col, type_name);
            draw->PopClipRect();
        }
        draw->AddText(ImVec2(short_x, text_pos.y), ImGui::GetColorU32(Theme::kMuted),
                      short_label.c_str());
    }
    else
    {
        draw->AddText(text_pos, primary_col, primary);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
    {
        selected = index;
    }
    ImGui::PopID();
}

void drawStudyList(std::vector<CStudyInstance>& draft,
                   int& selected,
                   float width,
                   float height,
                   bool scroll_to_end)
{
    ImGui::BeginChild("##study_list", ImVec2(width, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawPaneTitle("Studies");
    ImGui::BeginChild("##study_rows", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    const ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(spacing.x, std::max(1.0f, spacing.y * 0.5f)));
    if (draft.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::kMuted);
        ImGui::TextUnformatted("No studies");
        ImGui::PopStyleColor();
    }
    else
    {
        const auto row_count = static_cast<int>(draft.size());
        for (int i = 0; i < row_count; ++i)
        {
            drawStudyRow(draft[static_cast<std::size_t>(i)], i, selected);
        }
    }
    if (scroll_to_end)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::EndChild();
}

[[nodiscard]] bool drawStudyPropertiesPane(std::vector<CStudyInstance>& draft,
                                           int selected,
                                           float height)
{
    bool length_enter = false;
    ImGui::BeginChild("##study_props", ImVec2(0.0f, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (selectionInRange(draft, selected))
    {
        CStudyInstance& inst = draft[static_cast<std::size_t>(selected)];
        ImGui::AlignTextToFramePadding();
        if (!inst.enabled)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kTextDim);
        }
        ImGui::TextUnformatted(studyTypeName(inst));
        if (!inst.enabled)
        {
            ImGui::PopStyleColor();
        }
        const std::string short_label = studyShortLabel(inst);
        if (!short_label.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(studyPrimaryColor(inst)), "%s",
                               short_label.c_str());
        }
        ImGui::Separator();
        ImGui::BeginChild("##study_fields", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        length_enter = drawStudyProperties(inst);
        ImGui::EndChild();
    }
    else
    {
        drawPaneTitle("Settings");
        ImGui::BeginChild("##study_fields", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
        drawCenteredMuted(draft.empty() ? "Add a study to configure it." : "Select a study.");
        ImGui::EndChild();
    }
    ImGui::EndChild();
    return length_enter;
}

[[nodiscard]] bool atCap(const std::vector<CStudyInstance>& draft) noexcept
{
    return static_cast<int>(draft.size()) >= kStudyMaxPerPane;
}

void drawAddStudyModal(std::vector<CStudyInstance>& draft,
                      int& selected,
                      int& next_id,
                      ImGuiStorage* state,
                      ImGuiID scroll_key)
{
    const std::span<const StudyType* const> types = studyTypes();
    const auto type_count = static_cast<int>(types.size());
    std::vector<std::size_t> order(types.size());
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::ranges::stable_sort(order, [&](std::size_t lhs, std::size_t rhs) {
        return std::strcmp(types[lhs]->display_name, types[rhs]->display_name) < 0;
    });

    const float row_h = ImGui::GetFrameHeight();
    const ImGuiStyle& popup_style = ImGui::GetStyle();
    const float footer_h = ImGui::GetFrameHeightWithSpacing();
    const float chrome = ImGui::GetFrameHeight() + (popup_style.WindowPadding.y * 2.0f) +
                         (popup_style.WindowBorderSize * 2.0f) + (popup_style.ChildBorderSize * 2.0f) +
                         footer_h + popup_style.ItemSpacing.y;
    const float max_window_h = row_h * static_cast<float>(kAddStudyWindowMaxRows);
    int max_rows = kAddStudyRowFloor;
    if (row_h > 0.0f)
    {
        const float fit = std::max(0.0f, (max_window_h - chrome) / row_h);
        max_rows = std::max(kAddStudyRowFloor, static_cast<int>(fit));
    }
    const int catalog_rows = std::clamp(type_count, kAddStudyRowFloor, max_rows);
    const float window_h =
        std::min(max_window_h, (row_h * static_cast<float>(catalog_rows)) + chrome);
    const float floor_h =
        std::min(max_window_h, (row_h * static_cast<float>(kAddStudyRowFloor)) + chrome);
    float name_w = 0.0f;
    for (const StudyType* const info : types)
    {
        name_w = std::max(name_w, ImGui::CalcTextSize(info->display_name).x);
    }
    const float button_w = equalButtonWidth("Add", "Cancel");
    const float buttons = buttonClusterWidth(button_w, 2) + (popup_style.WindowPadding.x * 2.0f);
    const float names =
        name_w + (popup_style.WindowPadding.x * 2.0f) + (popup_style.FramePadding.x * 2.0f);
    const float min_w = row_h * kAddStudyMinWidthRows;
    const float max_w = row_h * kAddStudyMaxWidthRows;
    const float window_w = std::clamp(std::max(names, buttons), min_w, max_w);
    ImGui::SetNextWindowSize(ImVec2(window_w, window_h), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(min_w, floor_h), ImVec2(max_w, max_window_h));
    bool add_open = true;
    if (!ImGui::BeginPopupModal("Add Study###add_study", &add_open,
                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        return;
    }

    ImGuiStorage* modal_state = ImGui::GetStateStorage();
    const ImGuiID pick_key = ImGui::GetID("##add_study_pick");
    int pick = modal_state->GetInt(pick_key, 0);
    if (pick < 0 || pick >= type_count)
    {
        pick = 0;
    }

    const float list_h = std::max(row_h, ImGui::GetContentRegionAvail().y - footer_h);
    bool commit = false;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kBg0);
    ImGui::BeginChild("##add_study_list", ImVec2(0.0f, list_h), ImGuiChildFlags_Borders);
    for (int row = 0; row < type_count; ++row)
    {
        const StudyType& info = *types[order[static_cast<std::size_t>(row)]];
        ImGui::PushID(row);
        // Stay open on a single click so the row can be highlighted before Add.
        const ImGuiSelectableFlags flags =
            ImGuiSelectableFlags_NoAutoClosePopups | ImGuiSelectableFlags_AllowDoubleClick;
        if (ImGui::Selectable(info.display_name, pick == row, flags))
        {
            pick = row;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                commit = true;
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    modal_state->SetInt(pick_key, pick);

    const bool can_add = type_count > 0 && !atCap(draft);
    alignButtonCluster(buttonClusterWidth(button_w, 2));
    ImGui::BeginDisabled(!can_add);
    const bool add_clicked = drawAccentButton("Add", button_w, false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    const bool cancel = drawPlainButton("Cancel", button_w);

    // The studies window does not take Escape while this modal is open.
    const bool escape = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                        ImGui::IsKeyPressed(ImGuiKey_Escape);
    if ((commit || add_clicked) && can_add)
    {
        appendStudy(draft, selected, next_id, *types[order[static_cast<std::size_t>(pick)]]);
        state->SetBool(scroll_key, true);
        ImGui::CloseCurrentPopup();
    }
    else if (cancel || escape)
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawStudyFooter(std::vector<CStudyInstance>& draft,
                     int& selected,
                     int& next_id,
                     ImGuiStorage* state,
                     ImGuiID scroll_key,
                     StudyDraftUi& ui)
{
    const bool at_cap = atCap(draft);
    ImGui::BeginDisabled(at_cap);
    if (ImGui::Button("Add Study"))
    {
        ImGui::OpenPopup("Add Study###add_study");
    }
    ImGui::EndDisabled();
    drawAddStudyModal(draft, selected, next_id, state, scroll_key);

    ImGui::SameLine();
    ImGui::BeginDisabled(!selectionInRange(draft, selected));
    if (ImGui::Button("Remove"))
    {
        removeSelected(draft, selected);
    }
    ImGui::EndDisabled();
    if (at_cap)
    {
        ImGui::SameLine();
        ImGui::TextColored(Theme::kMuted, "maximum %d studies", kStudyMaxPerPane);
    }

    const float action_w = equalButtonWidth("OK", "Apply", "Cancel");
    ImGui::SameLine();
    alignButtonCluster(buttonClusterWidth(action_w, 3));
    ui.ok = drawAccentButton("OK", action_w, true);
    ImGui::SameLine();
    ui.apply = ImGui::Button("Apply", ImVec2(action_w, 0.0f));
    ImGui::SameLine();
    ui.cancel = drawPlainButton("Cancel", action_w);
}

}  // namespace

StudyDraftUi drawStudyDraftBody(std::vector<CStudyInstance>& draft, int& selected, int& next_id)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float separator_h = std::max(style.SeparatorSize, 1.0f);
    const float footer_h = separator_h + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    const float panels_h = std::max(0.0f, avail_h - footer_h);
    const float list_w = std::max(0.0f, avail_w * kStudyListFraction);

    ImGuiStorage* state = ImGui::GetStateStorage();
    const ImGuiID scroll_key = ImGui::GetID("##study_list_scroll_end");
    const bool scroll_to_end = state->GetBool(scroll_key, false);
    if (scroll_to_end)
    {
        state->SetBool(scroll_key, false);
    }

    for (CStudyInstance& inst : draft)
    {
        normalizeStudyOutputs(inst);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kBg0);
    drawStudyList(draft, selected, list_w, panels_h, scroll_to_end);
    ImGui::SameLine();
    StudyDraftUi ui;
    ui.length_enter = drawStudyPropertiesPane(draft, selected, panels_h);
    ImGui::PopStyleColor();

    ImGui::Separator();
    drawStudyFooter(draft, selected, next_id, state, scroll_key, ui);
    return ui;
}

}  // namespace terminal

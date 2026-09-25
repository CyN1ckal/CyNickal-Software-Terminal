// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudySettings.h"

#include "chart/CStudyCompute.h"
#include "chart/studies/StudyRegistry.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace terminal {
namespace {

constexpr float kStudyListFraction = 0.38f;

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
        ImGui::SetNextItemWidth(-FLT_MIN);
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
    ImGui::SetNextItemWidth(-FLT_MIN);
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
}

void drawOutputColorField(CStudyInstance& inst, std::size_t index, const char* label)
{
    drawPropertyLabel(label);
    ImGui::SetNextItemWidth(-FLT_MIN);
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
    drawPropertyLabel("Style");
    ImGui::SetNextItemWidth(-FLT_MIN);
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
        ImGui::SetNextItemWidth(-FLT_MIN);
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
    const bool many = type.outputs.size() > 1;
    for (std::size_t index = 0; index < type.outputs.size(); ++index)
    {
        const char* label = "Color";
        if (many && type.outputs[index].label != nullptr && type.outputs[index].label[0] != '\0')
        {
            label = type.outputs[index].label;
        }
        drawOutputColorField(inst, index, label);
        // A color-by-bar study's later outputs are colors for one series, not more series.
        if (type.color_by_bar && index > 0)
        {
            continue;
        }
        drawLineStyleField(inst, index, type.graph);
    }
}

[[nodiscard]] float studyPropertyLabelWidth(const StudyType& type)
{
    float width = ImGui::CalcTextSize("Chart Region").x;
    width = std::max(width, ImGui::CalcTextSize("Style").x);
    if (type.outputs.size() > 1)
    {
        for (const StudyOutput& output : type.outputs)
        {
            if (output.label != nullptr)
            {
                width = std::max(width, ImGui::CalcTextSize(output.label).x);
            }
        }
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
    if (ImGui::BeginTable("##study_props", 2, table_flags))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, label_w);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        length_enter = drawStudyOptions(inst, *type);
        drawChartRegionField(inst);
        drawOutputStyleFields(inst, *type);
        ImGui::EndTable();
    }

    drawMutedWrapped("Region 1 is the main price graph. Regions 2-12 are panes below it.");
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
    ImGui::SetNextWindowSize(ImVec2(row_h * 16.0f, row_h * 18.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(row_h * 12.0f, row_h * 10.0f),
                                        ImVec2(row_h * 28.0f, row_h * 32.0f));
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

    const float footer_h = ImGui::GetFrameHeightWithSpacing();
    const float list_h = std::max(row_h * 4.0f, ImGui::GetContentRegionAvail().y - footer_h);
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
    ImGui::BeginDisabled(!can_add);
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    const bool add_clicked = ImGui::Button("Add");
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
    const bool cancel = ImGui::Button("Cancel");
    ImGui::PopStyleColor();

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

    const ImGuiStyle& style = ImGui::GetStyle();
    const float ok_w = ImGui::CalcTextSize("OK").x + (style.FramePadding.x * 2.0f);
    const float apply_w = ImGui::CalcTextSize("Apply").x + (style.FramePadding.x * 2.0f);
    const float cancel_w = ImGui::CalcTextSize("Cancel").x + (style.FramePadding.x * 2.0f);
    const float cluster = ok_w + apply_w + cancel_w + (style.ItemSpacing.x * 2.0f);
    const float align_x = ImGui::GetContentRegionMax().x - cluster;
    ImGui::SameLine();
    if (align_x > ImGui::GetCursorPosX())
    {
        ImGui::SetCursorPosX(align_x);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, Theme::kGo);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::kAccentHover);
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
    ui.ok = ImGui::Button("OK");
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ui.apply = ImGui::Button("Apply");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::kCancel);
    ui.cancel = ImGui::Button("Cancel");
    ImGui::PopStyleColor();
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

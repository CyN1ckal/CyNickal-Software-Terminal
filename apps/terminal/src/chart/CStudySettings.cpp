// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "chart/CStudySettings.h"

#include "chart/CStudyCompute.h"
#include "ui/Theme.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>

namespace terminal {
namespace {

constexpr float kStudyListFraction = 0.38f;

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
    case StudySource::Volume:
        return "Volume";
    }
    return "Close";
}

[[nodiscard]] const char* studyTypeName(StudyKind kind) noexcept
{
    if (const StudyTypeInfo* info = findStudyType(kind))
    {
        return info->display_name;
    }
    return "Study";
}

[[nodiscard]] bool selectionInRange(const std::vector<CStudyInstance>& draft, int selected) noexcept
{
    return selected >= 0 && selected < static_cast<int>(draft.size());
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

[[nodiscard]] bool drawMovingAverageFields(CStudyInstance& inst, MovingAverageParams& params)
{
    drawPropertyLabel("Input Data");
    ImGui::SetNextItemWidth(-FLT_MIN);
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
        if (ImGui::Selectable("Volume", params.source == StudySource::Volume))
        {
            params.source = StudySource::Volume;
            // Volume shares a scale with the volume pane. Leave a region the user already chose.
            if (inst.chart_region == kStudyMainChartRegion)
            {
                inst.chart_region = kStudyVolumeChartRegion;
            }
        }
        ImGui::EndCombo();
    }

    drawPropertyLabel("Length");
    ImGui::SetNextItemWidth(-FLT_MIN);
    char len_id[64];
    std::snprintf(len_id, sizeof(len_id), "##study_len_%d", inst.id);
    // step 0: InputScalar's +/- buttons also return true when EnterReturnsTrue is set.
    // Live-edit writes each keystroke. The study list is drawn before this widget, so
    // a deactivate-only write would drop the length when the selection changes.
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    const bool length_enter =
        ImGui::InputInt(len_id, &params.length, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemFlag();

    drawPropertyLabel("Method");
    ImGui::SetNextItemWidth(-FLT_MIN);
    char method_id[64];
    std::snprintf(method_id, sizeof(method_id), "##study_method_%d", inst.id);
    if (ImGui::BeginCombo(method_id, "Simple"))
    {
        ImGui::Selectable("Simple", true);
        ImGui::EndCombo();
    }
    return length_enter;
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

void drawColorField(CStudyInstance& inst)
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
}

[[nodiscard]] bool drawStudyProperties(CStudyInstance& inst)
{
    bool length_enter = false;
    if (inst.kind == StudyKind::Volume)
    {
        drawMutedWrapped(
            "Volume of each chart bar. Bars use the candle colors. Color sets the label.");
        ImGui::Spacing();
    }

    const float label_w =
        ImGui::CalcTextSize("Chart Region").x + ImGui::GetStyle().FramePadding.x;
    const ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp |
                                        ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##study_props", 2, table_flags))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, label_w);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        if (inst.kind == StudyKind::MovingAverage)
        {
            if (auto* params = std::get_if<MovingAverageParams>(&inst.params))
            {
                length_enter = drawMovingAverageFields(inst, *params);
            }
        }
        drawChartRegionField(inst);
        drawColorField(inst);
        ImGui::EndTable();
    }

    if (inst.kind == StudyKind::MovingAverage)
    {
        drawMutedWrapped("v1: simple moving average");
    }
    drawMutedWrapped("Region 1 is the main price graph. Regions 2-12 are panes below it.");
    return length_enter;
}

void appendStudy(std::vector<CStudyInstance>& draft,
                 int& selected,
                 int& next_id,
                 const StudyTypeInfo& info)
{
    if (static_cast<int>(draft.size()) >= kStudyMaxPerPane)
    {
        return;
    }
    CStudyInstance inst;
    inst.id = next_id++;
    inst.kind = info.kind;
    inst.chart_region = info.default_chart_region;
    inst.params = defaultParams(info.kind);
    inst.color = info.kind == StudyKind::Volume ? kStudyPalette[2]
                                                : studyPaletteColor(static_cast<int>(draft.size()));
    draft.push_back(inst);
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
    const ImVec4 swatch = ImGui::ColorConvertU32ToFloat4(inst.color);
    if (ImGui::ColorButton("##swatch", swatch,
                           ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop |
                               ImGuiColorEditFlags_NoPicker,
                           ImVec2(row_h, row_h)))
    {
        selected = index;
    }
    ImGui::SameLine();

    const char* type_name = studyTypeName(inst.kind);
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
        ImGui::TextUnformatted(studyTypeName(inst.kind));
        if (!inst.enabled)
        {
            ImGui::PopStyleColor();
        }
        const std::string short_label = studyShortLabel(inst);
        if (!short_label.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(inst.color), "%s", short_label.c_str());
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
    constexpr auto kTypeCount = static_cast<int>(std::size(kStudyTypes));
    std::array<std::size_t, std::size(kStudyTypes)> order{};
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::ranges::stable_sort(order, [](std::size_t lhs, std::size_t rhs) {
        return std::strcmp(kStudyTypes[lhs].display_name, kStudyTypes[rhs].display_name) < 0;
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
    if (pick < 0 || pick >= kTypeCount)
    {
        pick = 0;
    }

    const float footer_h = ImGui::GetFrameHeightWithSpacing();
    const float list_h = std::max(row_h * 4.0f, ImGui::GetContentRegionAvail().y - footer_h);
    bool commit = false;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::kBg0);
    ImGui::BeginChild("##add_study_list", ImVec2(0.0f, list_h), ImGuiChildFlags_Borders);
    for (int row = 0; row < kTypeCount; ++row)
    {
        const StudyTypeInfo& info = kStudyTypes[order[static_cast<std::size_t>(row)]];
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

    const bool can_add = kTypeCount > 0 && !atCap(draft);
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
        appendStudy(draft, selected, next_id, kStudyTypes[order[static_cast<std::size_t>(pick)]]);
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
    const float ok_w = ImGui::CalcTextSize("OK").x + style.FramePadding.x * 2.0f;
    const float apply_w = ImGui::CalcTextSize("Apply").x + style.FramePadding.x * 2.0f;
    const float cancel_w = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2.0f;
    const float cluster = ok_w + apply_w + cancel_w + style.ItemSpacing.x * 2.0f;
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

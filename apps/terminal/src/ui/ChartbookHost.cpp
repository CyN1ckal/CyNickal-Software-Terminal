// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "ui/ChartbookHost.h"

#include "RepoRoot.h"
#include "chart/CChartbookFile.h"
#include "ui/InventoryPanel.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

namespace terminal {
namespace {

struct BuiltWindow
{
    std::string window;
    ImGuiID dock{0};
};

[[nodiscard]] std::string dockWindowName(int runtime, std::string_view window)
{
    if (window == "data")
    {
        return "###cb" + std::to_string(runtime) + "_data";
    }
    int financials_id = 0;
    if (financialsIdFromWindow(window, financials_id))
    {
        return "###cb" + std::to_string(runtime) + "_financials" + std::to_string(financials_id);
    }
    int options_id = 0;
    if (optionsIdFromWindow(window, options_id))
    {
        return "###cb" + std::to_string(runtime) + "_options" + std::to_string(options_id);
    }
    if (window.starts_with("pane:"))
    {
        return "###cb" + std::to_string(runtime) + "_pane" + std::string(window.substr(5));
    }
    return {};
}

[[nodiscard]] std::string windowIdFromName(const char* name, int runtime)
{
    if (name == nullptr)
    {
        return {};
    }
    const std::string prefix = "###cb" + std::to_string(runtime) + "_";
    const char* const found = std::strstr(name, prefix.c_str());
    if (found == nullptr)
    {
        return {};
    }
    const std::string rest = found + prefix.size();
    if (rest == "data")
    {
        return "data";
    }
    constexpr std::string_view financials_prefix = "financials";
    if (rest.starts_with(financials_prefix))
    {
        const std::string suffix = rest.substr(financials_prefix.size());
        if (suffix.empty() || suffix.find_first_not_of("0123456789") != std::string::npos)
        {
            return {};
        }
        return "financials:" + suffix;
    }
    constexpr std::string_view options_prefix = "options";
    if (rest.starts_with(options_prefix))
    {
        const std::string suffix = rest.substr(options_prefix.size());
        if (suffix.empty() || suffix.find_first_not_of("0123456789") != std::string::npos)
        {
            return {};
        }
        return "options:" + suffix;
    }
    if (rest.starts_with("pane"))
    {
        return "pane:" + rest.substr(4);
    }
    return {};
}

[[nodiscard]] int paneIdOf(std::string_view window)
{
    if (!window.starts_with("pane:"))
    {
        return 0;
    }
    int value = 0;
    for (const char digit : window.substr(5))
    {
        if (digit < '0' || digit > '9')
        {
            return 0;
        }
        value = (value * 10) + (digit - '0');
    }
    return value;
}

[[nodiscard]] std::vector<std::string> chartbookStems()
{
    std::vector<std::string> names;
    std::error_code error;
    const std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator it(chartbooksDirectory(), error); !error && it != end; it.increment(error))
    {
        if (!it->is_regular_file())
        {
            continue;
        }
        const std::string filename = it->path().filename().string();
        if (!filename.ends_with(kChartbookFileSuffix))
        {
            continue;
        }
        names.push_back(chartbookStemFromPath(it->path()));
    }
    std::ranges::sort(names);
    return names;
}

[[nodiscard]] bool floatingNear(const ChartbookFloating& left, const ChartbookFloating& right)
{
    return left.window == right.window && std::fabs(left.x - right.x) <= 2.f &&
           std::fabs(left.y - right.y) <= 2.f && std::fabs(left.w - right.w) <= 2.f &&
           std::fabs(left.h - right.h) <= 2.f;
}

void collapseEmptySplits(ChartbookLayout& layout)
{
    for (int pass = 0; pass < 16; ++pass)
    {
        bool changed = false;
        for (int index = 0; std::cmp_less(index, layout.nodes.size()); ++index)
        {
            ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(index)];
            if (!node.is_split)
            {
                continue;
            }
            const bool first_ok = node.first >= 0;
            const bool second_ok = node.second >= 0;
            if (first_ok && second_ok)
            {
                continue;
            }
            const int survivor = first_ok ? node.first : node.second;
            if (survivor < 0)
            {
                if (layout.root == index)
                {
                    layout.root = -1;
                    layout.nodes.clear();
                    return;
                }
                continue;
            }
            node = layout.nodes[static_cast<std::size_t>(survivor)];
            changed = true;
        }
        if (!changed)
        {
            return;
        }
    }
}

void orderDockLeaf(ImGuiDockNode* node, ImGuiWindow* selected)
{
    if (node == nullptr || node->Windows.Size == 0)
    {
        return;
    }
    for (int index = 0; index < node->Windows.Size; ++index)
    {
        node->Windows[index]->DockOrder = static_cast<short>(index);
    }
    if (selected != nullptr && selected->DockNode == node)
    {
        node->SelectedTabId = selected->TabId;
        node->VisibleWindow = selected;
        if (node->TabBar != nullptr)
        {
            node->TabBar->SelectedTabId = selected->TabId;
            node->TabBar->NextSelectedTabId = selected->TabId;
        }
    }
    if (node->TabBar == nullptr || node->TabBar->Tabs.Size <= 1)
    {
        return;
    }
    ImVector<ImGuiTabItem> tabs;
    for (int index = 0; index < node->Windows.Size; ++index)
    {
        ImGuiWindow const* window = node->Windows[index];
        for (int tab_n = 0; tab_n < node->TabBar->Tabs.Size; ++tab_n)
        {
            if (node->TabBar->Tabs[tab_n].Window != window)
            {
                continue;
            }
            ImGuiTabItem tab = node->TabBar->Tabs[tab_n];
            tab.Flags &= ~ImGuiTabItemFlags_Unsorted;
            tabs.push_back(tab);
            break;
        }
    }
    for (int tab_n = 0; tab_n < node->TabBar->Tabs.Size; ++tab_n)
    {
        const ImGuiTabItem& tab = node->TabBar->Tabs[tab_n];
        const bool listed = std::ranges::any_of(tabs, [&](const ImGuiTabItem& have) { return have.ID == tab.ID; });
        if (!listed)
        {
            tabs.push_back(tab);
        }
    }
    node->TabBar->Tabs.swap(tabs);
    node->TabBar->WantLayout = true;
}

}  // namespace

ChartbookHost::ChartbookHost()
{
    try
    {
        store_ = std::make_unique<Store>(defaultMarketDataDbPath(), StoreMode::Reader);
    }
    catch (const std::exception& ex)
    {
        open_error_ = ex.what();
    }

    const StartupLoadResult startup = loadStartupSettings(defaultTerminalSettingsPath());
    if (!startup.ok)
    {
        file_error_ = startup.error;
    }
    else
    {
        const StartupOpenResult opened = openStartupChartbooks(startup.settings);
        for (const StartupOpenResult::Opened& book : opened.books)
        {
            adopt(book.document, book.path, false);
        }
        for (const std::string& error : opened.errors)
        {
            if (!file_error_.empty())
            {
                file_error_ += '\n';
            }
            file_error_ += error;
        }
    }
    if (books_.empty())
    {
        adopt(makeDefaultChartbook(nextName()), {}, false);
        refresh_clean_ = true;
    }
    else
    {
        refresh_clean_ = false;
    }
    active_ = static_cast<int>(books_.size()) - 1;
    apply_layout_ = true;
    panel_import_ = true;
}

void ChartbookHost::adopt(const CChartbookDocument& document, std::filesystem::path path, bool dirty)
{
    auto book = std::make_unique<CChartBook>(next_runtime_);
    ++next_runtime_;
    book->loadDocument(document);
    OpenBook open;
    open.book = std::move(book);
    open.path = std::move(path);
    open.clean_json = chartbookToJson(open.book->exportDocument());
    open.dirty = dirty;
    books_.push_back(std::move(open));
}

std::string ChartbookHost::nextName()
{
    const std::string name = "chartbook" + std::to_string(unnamed_);
    ++unnamed_;
    return name;
}

const CChartBook& ChartbookHost::activeBook() const
{
    return *books_[static_cast<std::size_t>(active_)].book;
}

int ChartbookHost::findPath(const std::filesystem::path& path) const
{
    for (int index = 0; std::cmp_less(index, books_.size()); ++index)
    {
        const std::filesystem::path& open = books_[static_cast<std::size_t>(index)].path;
        if (!open.empty() && chartbookPathsEqual(open, path))
        {
            return index;
        }
    }
    return -1;
}

bool ChartbookHost::dataShown(const OpenBook& open)
{
    CChartbookDocument probe;
    probe.layout = open.book->layout();
    probe.floating = open.book->floating();
    return chartbookWindowReferenced(probe, "data");
}

void ChartbookHost::show(int index, bool fill_defaults)
{
    if (index < 0 || std::cmp_greater_equal(index, books_.size()) || index == active_)
    {
        if (fill_defaults)
        {
            refresh_clean_ = true;
            panel_import_ = true;
        }
        return;
    }
    active_ = index;
    apply_layout_ = true;
    panel_import_ = true;
    refresh_clean_ = fill_defaults;
}

void ChartbookHost::destroyBook(int index)
{
    if (index < 0 || std::cmp_greater_equal(index, books_.size()))
    {
        return;
    }
    books_.erase(books_.begin() + index);
    if (books_.empty())
    {
        adopt(makeDefaultChartbook(nextName()), {}, false);
        active_ = 0;
        refresh_clean_ = true;
    }
    else if (std::cmp_greater_equal(active_, books_.size()))
    {
        active_ = static_cast<int>(books_.size()) - 1;
    }
    else if (index < active_)
    {
        --active_;
    }
    apply_layout_ = true;
    panel_import_ = true;
}

bool ChartbookHost::saveBook(int index, InventoryPanel& inventory)
{
    if (index < 0 || std::cmp_greater_equal(index, books_.size()))
    {
        return false;
    }
    if (index == active_)
    {
        syncActive(inventory);
    }
    OpenBook& open = books_[static_cast<std::size_t>(index)];
    if (open.path.empty())
    {
        save_as_index_ = index;
        modal_error_.clear();
        const std::string stem = open.book->name();
        std::snprintf(name_, sizeof(name_), "%s", stem.c_str());
        requestModal(Modal::SaveAs);
        return false;
    }
    const CChartbookDocument document = open.book->exportDocument();
    const std::string error = saveChartbook(open.path, document);
    if (!error.empty())
    {
        file_error_ = error;
        return false;
    }
    open.clean_json = chartbookToJson(document);
    open.dirty = false;
    saved_this_frame_ = true;
    return true;
}

void ChartbookHost::requestModal(Modal modal) noexcept
{
    pending_modal_ = modal;
}

const char* ChartbookHost::modalTitle(Modal modal) noexcept
{
    switch (modal)
    {
    case Modal::Open:
        return "Open Chartbook";
    case Modal::SaveAs:
        return "Save Chartbook";
    case Modal::CloseBook:
        return "Close Chartbook";
    case Modal::Quit:
        return "Save Chartbooks";
    case Modal::Startup:
        return "Chartbooks to Open on Startup";
    case Modal::None:
        return "";
    }
    return "";
}

bool ChartbookHost::saveAll(InventoryPanel& inventory)
{
    for (int index = 0; std::cmp_less(index, books_.size()); ++index)
    {
        const OpenBook& open = books_[static_cast<std::size_t>(index)];
        if (!open.dirty && !open.path.empty())
        {
            continue;
        }
        if (!saveBook(index, inventory))
        {
            return false;
        }
    }
    return true;
}

void ChartbookHost::drawFileMenu(InventoryPanel& inventory)
{
    if (!beginTitleMenu("File"))
    {
        return;
    }
    if (!file_error_.empty())
    {
        ImGui::TextColored(Theme::kDown, "%s", file_error_.c_str());
        if (ImGui::MenuItem("Dismiss"))
        {
            file_error_.clear();
        }
        ImGui::Separator();
    }
    if (ImGui::MenuItem("New Chartbook"))
    {
        adopt(makeDefaultChartbook(nextName()), {}, false);
        show(static_cast<int>(books_.size()) - 1, true);
    }
    if (ImGui::MenuItem("Open Chartbook..."))
    {
        open_selected_ = -1;
        requestModal(Modal::Open);
    }
    if (ImGui::MenuItem("Save"))
    {
        save_as_then_quit_ = false;
        save_as_then_close_ = false;
        save_as_then_save_all_ = false;
        if (saveBook(active_, inventory))
        {
            file_error_.clear();
        }
    }
    if (ImGui::MenuItem("Save As..."))
    {
        save_as_index_ = active_;
        save_as_then_quit_ = false;
        save_as_then_close_ = false;
        save_as_then_save_all_ = false;
        modal_error_.clear();
        const std::string stem = books_[static_cast<std::size_t>(active_)].book->name();
        std::snprintf(name_, sizeof(name_), "%s", stem.c_str());
        requestModal(Modal::SaveAs);
    }
    if (ImGui::MenuItem("Save All"))
    {
        save_as_then_quit_ = false;
        save_as_then_close_ = false;
        save_as_then_save_all_ = true;
        if (saveAll(inventory))
        {
            save_as_then_save_all_ = false;
            file_error_.clear();
        }
        else if (pending_modal_ != Modal::SaveAs)
        {
            save_as_then_save_all_ = false;
        }
    }
    if (ImGui::MenuItem("Close Chartbook"))
    {
        close_index_ = active_;
        if (books_[static_cast<std::size_t>(active_)].dirty)
        {
            requestModal(Modal::CloseBook);
        }
        else
        {
            destroyBook(active_);
            close_index_ = -1;
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Chartbooks to Open on Startup..."))
    {
        const StartupLoadResult loaded = loadStartupSettings(defaultTerminalSettingsPath());
        startup_edit_ = loaded.ok ? loaded.settings.open_on_startup : std::vector<std::string>{};
        startup_selected_ = startup_edit_.empty() ? -1 : 0;
        startup_picking_ = false;
        requestModal(Modal::Startup);
    }
    endTitleMenu();
}

void ChartbookHost::drawViewMenu()
{
    if (!beginTitleMenu("View"))
    {
        return;
    }
    OpenBook const& open = books_[static_cast<std::size_t>(active_)];
    const bool shown = dataShown(open);
    if (ImGui::MenuItem("DATA", nullptr, shown))
    {
        if (!shown)
        {
            ChartbookLayout layout = open.book->layout();
            chartbookInsertData(layout);
            open.book->setLayout(std::move(layout));
            apply_layout_ = true;
        }
    }
    if (ImGui::MenuItem("New Financials"))
    {
        open.book->addFinancials();
    }
    const bool close_financials = open.book->focusedFinancials() != nullptr;
    if (ImGui::MenuItem("Close Financials", nullptr, false, close_financials))
    {
        open.book->closeFocusedFinancials();
    }
    if (ImGui::MenuItem("New Options Chain"))
    {
        open.book->addOptions();
    }
    const bool close_options = open.book->focusedOptions() != nullptr;
    if (ImGui::MenuItem("Close Options Chain", nullptr, false, close_options))
    {
        open.book->closeFocusedOptions();
    }
    endTitleMenu();
}

void ChartbookHost::drawTitleMenus(InventoryPanel& inventory, float tabs_right)
{
    drawFileMenu(inventory);
    books_[static_cast<std::size_t>(active_)].book->drawMenu();
    drawViewMenu();
    drawTabs(tabs_right);
}

void ChartbookHost::drawTabs(float tabs_right)
{
    float width = 0.f;
    for (const OpenBook& open : books_)
    {
        const std::string label = open.book->name() + (open.dirty ? "*" : "");
        width += ImGui::CalcTextSize(label.c_str()).x + ImGui::GetFrameHeight() + 16.f;
    }
    const float local_right = tabs_right - ImGui::GetWindowPos().x;
    const float target = local_right - width - ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetCursorPosX() < target)
    {
        ImGui::SetCursorPosX(target);
    }
    int reorder_from = -1;
    int reorder_to = -1;
    int close_now = -1;
    for (int index = 0; std::cmp_less(index, books_.size()); ++index)
    {
        OpenBook const& open = books_[static_cast<std::size_t>(index)];
        const std::string label = open.book->name() + (open.dirty ? "*" : "");
        const bool highlight = index == active_;
        if (highlight)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, Theme::kAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::kBg0);
        }
        ImGui::PushID(index);
        if (ImGui::Button(label.c_str()))
        {
            show(index, false);
        }
        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("CHARTBOOK_TAB", &index, sizeof(index));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CHARTBOOK_TAB"))
            {
                int from = 0;
                std::memcpy(&from, payload->Data, sizeof(from));
                reorder_from = from;
                reorder_to = index;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine(0.f, 2.f);
        if (ImGui::SmallButton("x"))
        {
            close_index_ = index;
            if (open.dirty)
            {
                show(index, false);
                requestModal(Modal::CloseBook);
            }
            else
            {
                close_now = index;
                close_index_ = -1;
            }
        }
        ImGui::PopID();
        if (highlight)
        {
            ImGui::PopStyleColor(2);
        }
        if (index + 1 < static_cast<int>(books_.size()))
        {
            ImGui::SameLine(0.f, 6.f);
        }
    }
    if (reorder_from >= 0 && reorder_to >= 0 && reorder_from != reorder_to &&
        std::cmp_less(reorder_from, books_.size()))
    {
        OpenBook moved = std::move(books_[static_cast<std::size_t>(reorder_from)]);
        books_.erase(books_.begin() + reorder_from);
        books_.insert(books_.begin() + reorder_to, std::move(moved));
        if (active_ == reorder_from)
        {
            active_ = reorder_to;
        }
        else if (reorder_from < active_ && reorder_to >= active_)
        {
            --active_;
        }
        else if (reorder_from > active_ && reorder_to <= active_)
        {
            ++active_;
        }
    }
    if (close_now >= 0)
    {
        destroyBook(close_now);
    }
}

WorkspaceClose ChartbookHost::drawChrome(InventoryPanel& inventory, bool close_requested)
{
    bool any_dirty = false;
    for (const OpenBook& open : books_)
    {
        any_dirty = any_dirty || open.dirty;
    }
    if (close_requested && !quit_modal_ && any_dirty)
    {
        quit_modal_ = true;
        requestModal(Modal::Quit);
    }

    drawModals(inventory);

    if (quit_now_)
    {
        return WorkspaceClose::Quit;
    }
    if (quit_modal_)
    {
        return WorkspaceClose::Cancel;
    }
    if (close_requested && !any_dirty)
    {
        return WorkspaceClose::Quit;
    }
    return WorkspaceClose::Continue;
}

void ChartbookHost::drawModals(InventoryPanel& inventory)
{
    // Menus and other modals are different ImGui windows. OpenPopup and BeginPopupModal
    // only meet when they run in this host, so Save As can follow File >> Save or Save All.
    // The host has to stay inside the main viewport. A position outside it becomes another
    // OS window while viewports are enabled, and that window shows up as a blank desktop entry.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##chartbook_modals", nullptr, host_flags);
    ImGui::PopStyleVar();
    if (pending_modal_ != Modal::None)
    {
        ImGui::OpenPopup(modalTitle(pending_modal_));
        pending_modal_ = Modal::None;
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Open Chartbook", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const std::vector<std::string> names = chartbookStems();
        if (ImGui::BeginListBox("##open_list", ImVec2(320.f, 220.f)))
        {
            for (int index = 0; std::cmp_less(index, names.size()); ++index)
            {
                if (ImGui::Selectable(names[static_cast<std::size_t>(index)].c_str(), open_selected_ == index,
                                      ImGuiSelectableFlags_AllowDoubleClick))
                {
                    open_selected_ = index;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        const std::filesystem::path path = chartbookPathForStem(names[static_cast<std::size_t>(index)]);
                        const int existing = findPath(path);
                        if (existing >= 0)
                        {
                            show(existing, false);
                        }
                        else
                        {
                            const ChartbookLoadResult loaded = loadChartbook(path);
                            if (!loaded.ok)
                            {
                                file_error_ = loaded.error;
                            }
                            else
                            {
                                adopt(loaded.document, path, false);
                                show(static_cast<int>(books_.size()) - 1, false);
                            }
                        }
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Open") && open_selected_ >= 0 && std::cmp_less(open_selected_, names.size()))
        {
            const std::filesystem::path path =
                chartbookPathForStem(names[static_cast<std::size_t>(open_selected_)]);
            const int existing = findPath(path);
            if (existing >= 0)
            {
                show(existing, false);
            }
            else
            {
                const ChartbookLoadResult loaded = loadChartbook(path);
                if (!loaded.ok)
                {
                    file_error_ = loaded.error;
                }
                else
                {
                    adopt(loaded.document, path, false);
                    show(static_cast<int>(books_.size()) - 1, false);
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Save Chartbook", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const std::vector<std::string> names = chartbookStems();
        ImGui::InputText("Name", name_, sizeof(name_));
        if (ImGui::BeginListBox("##save_list", ImVec2(320.f, 180.f)))
        {
            for (const std::string& stem : names)
            {
                if (ImGui::Selectable(stem.c_str(), stem == name_))
                {
                    std::snprintf(name_, sizeof(name_), "%s", stem.c_str());
                }
            }
            ImGui::EndListBox();
        }
        const std::filesystem::path path = chartbookPathForStem(name_);
        if (ImGui::Button("Save"))
        {
            const int existing = path.empty() ? -1 : findPath(path);
            if (save_as_index_ < 0 || std::cmp_greater_equal(save_as_index_, books_.size()))
            {
                modal_error_ = "That chartbook is no longer open.";
            }
            else if (path.empty())
            {
                modal_error_ = "Use a single name without reserved characters.";
            }
            else if (existing >= 0 && existing != save_as_index_)
            {
                modal_error_ = "That chartbook is already open.";
            }
            else
            {
                OpenBook& open = books_[static_cast<std::size_t>(save_as_index_)];
                if (save_as_index_ == active_)
                {
                    syncActive(inventory);
                }
                open.book->setName(chartbookStemFromPath(path));
                const CChartbookDocument document = open.book->exportDocument();
                const std::string error = saveChartbook(path, document);
                if (!error.empty())
                {
                    modal_error_ = error;
                    file_error_ = error;
                }
                else
                {
                    open.path = path;
                    open.clean_json = chartbookToJson(document);
                    open.dirty = false;
                    saved_this_frame_ = true;
                    modal_error_.clear();
                    const bool closing = save_as_then_close_;
                    const bool quitting = save_as_then_quit_;
                    const bool save_all = save_as_then_save_all_;
                    const int saved_index = save_as_index_;
                    save_as_then_close_ = false;
                    save_as_then_quit_ = false;
                    save_as_then_save_all_ = false;
                    ImGui::CloseCurrentPopup();
                    if (closing)
                    {
                        destroyBook(saved_index);
                        close_index_ = -1;
                    }
                    if (quitting)
                    {
                        if (saveAll(inventory))
                        {
                            quit_modal_ = false;
                            quit_now_ = true;
                        }
                        else if (pending_modal_ == Modal::SaveAs)
                        {
                            save_as_then_quit_ = true;
                        }
                        else
                        {
                            requestModal(Modal::Quit);
                        }
                    }
                    else if (save_all && !saveAll(inventory) && pending_modal_ == Modal::SaveAs)
                    {
                        save_as_then_save_all_ = true;
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            const bool reopen_quit = save_as_then_quit_;
            const bool reopen_close = save_as_then_close_;
            save_as_then_quit_ = false;
            save_as_then_close_ = false;
            save_as_then_save_all_ = false;
            modal_error_.clear();
            ImGui::CloseCurrentPopup();
            if (reopen_quit)
            {
                requestModal(Modal::Quit);
            }
            else if (reopen_close)
            {
                requestModal(Modal::CloseBook);
            }
        }
        if (!modal_error_.empty())
        {
            ImGui::TextColored(Theme::kDown, "%s", modal_error_.c_str());
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Close Chartbook", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (close_index_ < 0 || std::cmp_greater_equal(close_index_, books_.size()))
        {
            ImGui::CloseCurrentPopup();
        }
        else
        {
            const int index = close_index_;
            const std::string label = books_[static_cast<std::size_t>(index)].book->name();
            ImGui::Text("Save changes to %s?", label.c_str());
            if (ImGui::Button("Save"))
            {
                save_as_then_close_ = true;
                save_as_then_quit_ = false;
                save_as_then_save_all_ = false;
                if (saveBook(index, inventory))
                {
                    destroyBook(index);
                    close_index_ = -1;
                    save_as_then_close_ = false;
                    ImGui::CloseCurrentPopup();
                }
                else if (pending_modal_ == Modal::SaveAs)
                {
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    save_as_then_close_ = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save"))
            {
                destroyBook(index);
                close_index_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                close_index_ = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Save Chartbooks", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Save changes to the open chartbooks?");
        if (ImGui::Button("Save All"))
        {
            save_as_then_quit_ = true;
            save_as_then_close_ = false;
            save_as_then_save_all_ = false;
            if (saveAll(inventory))
            {
                quit_modal_ = false;
                quit_now_ = true;
                save_as_then_quit_ = false;
                ImGui::CloseCurrentPopup();
            }
            else if (pending_modal_ == Modal::SaveAs)
            {
                ImGui::CloseCurrentPopup();
            }
            else
            {
                save_as_then_quit_ = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            quit_modal_ = false;
            quit_now_ = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            quit_modal_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Chartbooks to Open on Startup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginListBox("##startup_list", ImVec2(360.f, 180.f)))
        {
            for (int index = 0; std::cmp_less(index, startup_edit_.size()); ++index)
            {
                const std::string& stored = startup_edit_[static_cast<std::size_t>(index)];
                if (ImGui::Selectable(stored.c_str(), startup_selected_ == index))
                {
                    startup_selected_ = index;
                }
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Add"))
        {
            startup_picking_ = !startup_picking_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove") && startup_selected_ >= 0 &&
            std::cmp_less(startup_selected_, startup_edit_.size()))
        {
            startup_edit_.erase(startup_edit_.begin() + startup_selected_);
            if (std::cmp_greater_equal(startup_selected_, startup_edit_.size()))
            {
                startup_selected_ = static_cast<int>(startup_edit_.size()) - 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Move Up") && startup_selected_ > 0)
        {
            std::swap(startup_edit_[static_cast<std::size_t>(startup_selected_)],
                      startup_edit_[static_cast<std::size_t>(startup_selected_ - 1)]);
            --startup_selected_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Move Down") && startup_selected_ >= 0 &&
            startup_selected_ + 1 < static_cast<int>(startup_edit_.size()))
        {
            const std::size_t next = static_cast<std::size_t>(startup_selected_) + 1;
            std::swap(startup_edit_[static_cast<std::size_t>(startup_selected_)], startup_edit_[next]);
            ++startup_selected_;
        }
        if (startup_picking_)
        {
            const std::vector<std::string> names = chartbookStems();
            if (ImGui::BeginListBox("##startup_add", ImVec2(360.f, 120.f)))
            {
                for (const std::string& stem : names)
                {
                    if (ImGui::Selectable(stem.c_str()))
                    {
                        const std::filesystem::path path = chartbookPathForStem(stem);
                        startup_edit_.push_back(pathForStorage(path));
                        startup_selected_ = static_cast<int>(startup_edit_.size()) - 1;
                        startup_picking_ = false;
                    }
                }
                ImGui::EndListBox();
            }
        }
        if (ImGui::Button("OK"))
        {
            StartupSettings settings;
            settings.open_on_startup = startup_edit_;
            const std::string error = saveStartupSettings(defaultTerminalSettingsPath(), settings);
            if (!error.empty())
            {
                file_error_ = error;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

void ChartbookHost::restoreOpenTabs()
{
    if (active_ < 0 || std::cmp_greater_equal(active_, books_.size()))
    {
        return;
    }
    const OpenBook& open = books_[static_cast<std::size_t>(active_)];
    const ChartbookLayout& layout = open.book->layout();
    const int runtime = open.book->runtimeId();
    std::vector<int> pending;
    if (layout.root >= 0)
    {
        pending.push_back(layout.root);
    }
    for (int steps = 0; !pending.empty() && steps < 64; ++steps)
    {
        const int index = pending.back();
        pending.pop_back();
        if (index < 0 || std::cmp_greater_equal(index, layout.nodes.size()))
        {
            continue;
        }
        const ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(index)];
        if (node.is_split)
        {
            if (node.second >= 0)
            {
                pending.push_back(node.second);
            }
            if (node.first >= 0)
            {
                pending.push_back(node.first);
            }
            continue;
        }
        ImVector<ImGuiWindow*> windows;
        ImGuiWindow* selected = nullptr;
        bool complete = !node.windows.empty();
        for (const std::string& id : node.windows)
        {
            const std::string name = dockWindowName(runtime, id);
            ImGuiWindow* window = name.empty() ? nullptr : ImGui::FindWindowByID(ImHashStr(name.c_str()));
            if (window == nullptr || window->DockNode == nullptr)
            {
                complete = false;
                break;
            }
            windows.push_back(window);
            if (id == node.selected)
            {
                selected = window;
            }
        }
        if (!complete)
        {
            continue;
        }
        ImGuiDockNode* leaf = windows[0]->DockNode;
        const bool same_node = std::ranges::all_of(windows, [leaf](const ImGuiWindow* window) {
            return window->DockNode == leaf;
        });
        if (!same_node || leaf == nullptr)
        {
            continue;
        }
        for (int window_n = 0; window_n < leaf->Windows.Size; ++window_n)
        {
            // windows stores mutable ImGuiWindow pointers.
            ImGuiWindow* window = leaf->Windows[window_n]; // NOLINT(misc-const-correctness)
            if (!windows.contains(window))
            {
                windows.push_back(window);
            }
        }
        leaf->Windows.swap(windows);
        orderDockLeaf(leaf, selected != nullptr ? selected : leaf->Windows[0]);
    }
}

void ChartbookHost::applyLayout(InventoryPanel& inventory, ImGuiID dock_id, ImVec2 size)
{
    restore_tabs_ = true;
    OpenBook const& open = books_[static_cast<std::size_t>(active_)];
    const ChartbookLayout& layout = open.book->layout();
    const int runtime = open.book->runtimeId();
    ImGui::DockBuilderRemoveNode(dock_id);
    ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock_id, size);

    struct Job
    {
        ImGuiID node{0};
        int index{-1};
    };
    std::vector<Job> jobs;
    std::vector<BuiltWindow> built;
    if (layout.root >= 0)
    {
        jobs.push_back(Job{.node=dock_id, .index=layout.root});
    }
    for (int steps = 0; !jobs.empty() && steps < 64; ++steps)
    {
        const Job job = jobs.back();
        jobs.pop_back();
        if (job.index < 0 || std::cmp_greater_equal(job.index, layout.nodes.size()))
        {
            continue;
        }
        const ChartbookLayoutNode& node = layout.nodes[static_cast<std::size_t>(job.index)];
        if (!node.is_split)
        {
            for (const std::string& window : node.windows)
            {
                const std::string name = dockWindowName(runtime, window);
                if (!name.empty())
                {
                    ImGui::DockBuilderDockWindow(name.c_str(), job.node);
                    built.push_back(BuiltWindow{.window=window, .dock=job.node});
                }
            }
            continue;
        }
        ImGuiID first = 0;
        ImGuiID second = 0;
        const ImGuiDir direction = node.axis == ChartbookSplitAxis::Vertical ? ImGuiDir_Up : ImGuiDir_Left;
        ImGui::DockBuilderSplitNode(job.node, direction, node.ratio, &first, &second);
        jobs.push_back(Job{.node=second, .index=node.second});
        jobs.push_back(Job{.node=first, .index=node.first});
    }
    ImGui::DockBuilderFinish(dock_id);

    inventory.setWindowScope(runtime);
    for (const BuiltWindow& window : built)
    {
        int financials_id = 0;
        int options_id = 0;
        if (window.window == "data")
        {
            inventory.setPlacement(true, false, window.dock, ImVec2{}, ImVec2{});
        }
        else if (financialsIdFromWindow(window.window, financials_id))
        {
            open.book->placeFinancials(financials_id, true, false, window.dock, ImVec2{}, ImVec2{});
        }
        else if (optionsIdFromWindow(window.window, options_id))
        {
            open.book->placeOptions(options_id, true, false, window.dock, ImVec2{}, ImVec2{});
        }
        else
        {
            open.book->placePane(paneIdOf(window.window), true, false, window.dock, ImVec2{}, ImVec2{});
        }
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    for (const ChartbookFloating& floating : open.book->floating())
    {
        const ImVec2 pos{floating.x, floating.y};
        const ImVec2 floating_size{floating.w, floating.h};
        int financials_id = 0;
        int options_id = 0;
        if (floating.window == "data")
        {
            inventory.setPlacement(true, true, 0, pos, floating_size);
        }
        else if (financialsIdFromWindow(floating.window, financials_id))
        {
            open.book->placeFinancials(financials_id, true, true, 0, pos, floating_size);
        }
        else if (optionsIdFromWindow(floating.window, options_id))
        {
            open.book->placeOptions(options_id, true, true, 0, pos, floating_size);
        }
        else
        {
            open.book->placePane(paneIdOf(floating.window), true, true, 0, pos, floating_size);
        }
    }
    ImGui::SetNextWindowViewport(viewport->ID);
}

void ChartbookHost::captureLayout(ImGuiID dock_id)
{
    OpenBook const& open = books_[static_cast<std::size_t>(active_)];
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dock_id);
    if (root == nullptr)
    {
        return;
    }
    const int runtime = open.book->runtimeId();
    ChartbookLayout layout;
    struct Job
    {
        ImGuiDockNode* node{nullptr};
        int parent{-1};
        bool second{false};
    };
    std::vector<Job> jobs;
    jobs.push_back(Job{.node=root, .parent=-1, .second=false});
    std::vector<std::string> seen;
    for (int steps = 0; !jobs.empty() && steps < 64; ++steps)
    {
        const Job job = jobs.back();
        jobs.pop_back();
        if (job.node == nullptr)
        {
            continue;
        }
        if (job.node->IsSplitNode())
        {
            ImGuiDockNode* child_a = job.node->ChildNodes[0];
            ImGuiDockNode* child_b = job.node->ChildNodes[1];
            const bool horizontal = job.node->SplitAxis == ImGuiAxis_X;
            const bool a_is_first =
                child_a != nullptr && child_b != nullptr &&
                (horizontal ? child_a->Pos.x <= child_b->Pos.x : child_a->Pos.y <= child_b->Pos.y);
            ImGuiDockNode* first = a_is_first ? child_a : child_b;
            ImGuiDockNode* second = a_is_first ? child_b : child_a;
            if (child_a == nullptr || child_b == nullptr)
            {
                first = child_a != nullptr ? child_a : child_b;
                second = nullptr;
            }
            const float span = horizontal ? job.node->Size.x : job.node->Size.y;
            float part = span * 0.5f;
            if (first != nullptr)
            {
                part = horizontal ? first->Size.x : first->Size.y;
            }
            ChartbookLayoutNode split;
            split.is_split = true;
            split.axis = horizontal ? ChartbookSplitAxis::Horizontal : ChartbookSplitAxis::Vertical;
            const float ratio = span > 1.f ? part / span : 0.5f;
            split.ratio = std::round(std::clamp(ratio, 0.05f, 0.95f) * 1000.f) / 1000.f;
            layout.nodes.push_back(split);
            const int index = static_cast<int>(layout.nodes.size()) - 1;
            if (job.parent < 0)
            {
                layout.root = index;
            }
            else if (job.second)
            {
                layout.nodes[static_cast<std::size_t>(job.parent)].second = index;
            }
            else
            {
                layout.nodes[static_cast<std::size_t>(job.parent)].first = index;
            }
            jobs.push_back(Job{.node=second, .parent=index, .second=true});
            jobs.push_back(Job{.node=first, .parent=index, .second=false});
            continue;
        }
        ChartbookLayoutNode leaf;
        for (int window_n = 0; window_n < job.node->Windows.Size; ++window_n)
        {
            ImGuiWindow const* window = job.node->Windows[window_n];
            const std::string id = windowIdFromName(window->Name, runtime);
            if (id.empty() || id == "##StatusRail")
            {
                continue;
            }
            int financials_id = 0;
            int options_id = 0;
            const bool is_financials = financialsIdFromWindow(id, financials_id);
            const bool is_options = optionsIdFromWindow(id, options_id);
            if (id != "data" && !is_financials && !is_options && !open.book->containsPane(paneIdOf(id)))
            {
                continue;
            }
            if (is_financials && !open.book->containsFinancials(financials_id))
            {
                continue;
            }
            if (is_options && !open.book->containsOptions(options_id))
            {
                continue;
            }
            leaf.windows.push_back(id);
            seen.push_back(id);
            if (job.node->SelectedTabId == window->TabId || job.node->SelectedTabId == window->ID)
            {
                leaf.selected = id;
            }
        }
        if (leaf.windows.empty())
        {
            continue;
        }
        if (leaf.selected.empty())
        {
            leaf.selected = leaf.windows.front();
        }
        layout.nodes.push_back(leaf);
        const int index = static_cast<int>(layout.nodes.size()) - 1;
        if (job.parent < 0)
        {
            layout.root = index;
        }
        else if (job.second)
        {
            layout.nodes[static_cast<std::size_t>(job.parent)].second = index;
        }
        else
        {
            layout.nodes[static_cast<std::size_t>(job.parent)].first = index;
        }
    }
    collapseEmptySplits(layout);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    std::vector<ChartbookFloating> floating;
    const auto consider = [&](std::string_view window_id) {
        if (std::ranges::find(seen, std::string(window_id)) != seen.end())
        {
            return;
        }
        const std::string name = dockWindowName(runtime, window_id);
        ImGuiWindow const* window = ImGui::FindWindowByName(name.c_str());
        if (window == nullptr || window->DockId != 0)
        {
            return;
        }
        ChartbookFloating placed;
        placed.window = std::string(window_id);
        placed.x = window->Pos.x - viewport->WorkPos.x;
        placed.y = window->Pos.y - viewport->WorkPos.y;
        placed.w = window->Size.x;
        placed.h = window->Size.y;
        seen.push_back(placed.window);
        floating.push_back(std::move(placed));
    };
    if (dataShown(open))
    {
        consider("data");
    }
    const CChartbookDocument exported = open.book->exportDocument();
    for (const ChartbookFinancials& panel : exported.financials)
    {
        consider(financialsWindowId(panel.id));
    }
    for (const ChartbookOptions& panel : exported.options)
    {
        consider(optionsWindowId(panel.id));
    }
    for (const ChartbookPane& pane : exported.panes)
    {
        consider(paneWindowId(pane.id));
    }

    bool complete = true;
    for (const ChartbookPane& pane : exported.panes)
    {
        if (std::ranges::find(seen, paneWindowId(pane.id)) == seen.end())
        {
            complete = false;
        }
    }
    for (const ChartbookFinancials& panel : exported.financials)
    {
        if (std::ranges::find(seen, financialsWindowId(panel.id)) == seen.end())
        {
            complete = false;
        }
    }
    for (const ChartbookOptions& panel : exported.options)
    {
        if (std::ranges::find(seen, optionsWindowId(panel.id)) == seen.end())
        {
            complete = false;
        }
    }
    if (dataShown(open) && std::ranges::find(seen, std::string("data")) == seen.end())
    {
        complete = false;
    }
    if (!complete)
    {
        return;
    }
    const bool same_layout = chartbookLayoutsEquivalent(open.book->layout(), layout);
    bool same_floating = open.book->floating().size() == floating.size();
    if (same_floating)
    {
        for (const ChartbookFloating& item : open.book->floating())
        {
            const bool matched = std::ranges::any_of(floating, [&](const ChartbookFloating& candidate) {
                return floatingNear(item, candidate);
            });
            same_floating = same_floating && matched;
        }
    }
    if (!same_layout)
    {
        open.book->setLayout(std::move(layout));
    }
    if (!same_floating)
    {
        open.book->setFloating(std::move(floating));
    }
}

void ChartbookHost::syncActive(InventoryPanel& inventory)
{
    if (active_ < 0 || std::cmp_greater_equal(active_, books_.size()))
    {
        return;
    }
    OpenBook& open = books_[static_cast<std::size_t>(active_)];
    if (dataShown(open))
    {
        open.book->setData(inventory.exportData());
    }
    const std::string json = chartbookToJson(open.book->exportDocument());
    if (saved_this_frame_)
    {
        open.clean_json = json;
        open.dirty = false;
        saved_this_frame_ = false;
    }
    else
    {
        open.dirty = json != open.clean_json;
    }
}

void ChartbookHost::drawSpace(InventoryPanel& inventory)
{
    OpenBook& open = books_[static_cast<std::size_t>(active_)];
    if (open.book->consumeLayoutRequest())
    {
        apply_layout_ = true;
    }
    if (panel_import_)
    {
        inventory.importData(open.book->data());
        inventory.setWindowScope(open.book->runtimeId());
        if (refresh_clean_)
        {
            open.book->setData(inventory.exportData());
            open.clean_json = chartbookToJson(open.book->exportDocument());
            open.dirty = false;
            refresh_clean_ = false;
        }
        panel_import_ = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Workspace", nullptr, flags);
    ImGui::PopStyleVar(3);
    const ImGuiID dock_id = ImGui::GetID("WorkspaceDock");
    if (apply_layout_)
    {
        applyLayout(inventory, dock_id, viewport->WorkSize);
        apply_layout_ = false;
    }
    ImGui::DockSpace(dock_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();

    bool closed_data = false;
    if (dataShown(open))
    {
        if (!inventory.draw())
        {
            ChartbookLayout layout = open.book->layout();
            chartbookRemoveWindow(layout, "data");
            open.book->setLayout(std::move(layout));
            std::vector<ChartbookFloating> floating = open.book->floating();
            std::erase_if(floating, [](const ChartbookFloating& item) { return item.window == "data"; });
            open.book->setFloating(std::move(floating));
            apply_layout_ = true;
            closed_data = true;
        }
    }
    open.book->drawFinancials(store_.get(), open_error_, inventory.ingestWorker());
    open.book->drawOptions(store_.get(), open_error_, inventory.ingestWorker());
    open.book->draw(store_.get(), open_error_, inventory.ingestWorker());
    if (restore_tabs_)
    {
        restoreOpenTabs();
        restore_tabs_ = false;
    }
    if (!closed_data)
    {
        captureLayout(dock_id);
    }
    syncActive(inventory);
}

}  // namespace terminal

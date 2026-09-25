// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartBook.h"
#include "market_data/Store.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace terminal {

class InventoryPanel;

enum class WorkspaceClose : std::uint8_t
{
    Continue = 0,
    Quit,
    Cancel
};

// Loaded chartbooks. One is visible. Menus are drawn on the title bar. The status rail is not part of a book.
class ChartbookHost
{
public:
    ChartbookHost();

    void drawTitleMenus(InventoryPanel& inventory, float tabs_right);
    [[nodiscard]] WorkspaceClose drawChrome(InventoryPanel& inventory, bool close_requested);
    void drawSpace(InventoryPanel& inventory);
    [[nodiscard]] const CChartBook& activeBook() const;

private:
    struct OpenBook
    {
        std::unique_ptr<CChartBook> book;
        std::filesystem::path path;
        std::string clean_json;
        bool dirty{false};
    };

    void adopt(const CChartbookDocument& document, std::filesystem::path path, bool dirty);
    void show(int index, bool fill_defaults);
    void syncActive(InventoryPanel& inventory);
    [[nodiscard]] bool saveBook(int index, InventoryPanel& inventory);
    [[nodiscard]] bool saveAll(InventoryPanel& inventory);
    void destroyBook(int index);
    [[nodiscard]] static bool dataShown(const OpenBook& open);
    void setDataShown(bool shown);
    [[nodiscard]] bool openListed(const std::filesystem::path& path);
    [[nodiscard]] int findPath(const std::filesystem::path& path) const;
    [[nodiscard]] std::string nextName();

    void drawFileMenu(InventoryPanel& inventory);
    void drawViewMenu();
    void dispatchRefresh(InventoryPanel& inventory);
    void drawTabs(float tabs_right);
    void drawModals(InventoryPanel& inventory);
    void applyLayout(InventoryPanel& inventory, ImGuiID dock_id, ImVec2 size);
    void restoreOpenTabs();
    void captureLayout(ImGuiID dock_id);

    std::unique_ptr<Store> store_;
    std::string open_error_;
    std::vector<OpenBook> books_;
    int active_{0};
    int next_runtime_{1};
    int unnamed_{1};
    bool apply_layout_{true};
    bool panel_import_{true};
    bool refresh_clean_{true};
    PanelKind armed_{PanelKind::None};
    bool refresh_from_menu_{false};
    std::string file_error_;
    std::string modal_error_;
    bool saved_this_frame_{false};

    enum class Modal : std::uint8_t
    {
        None = 0,
        Open,
        SaveAs,
        CloseBook,
        Quit,
        Startup
    };

    void requestModal(Modal modal) noexcept;
    [[nodiscard]] static const char* modalTitle(Modal modal) noexcept;

    int close_index_{-1};
    Modal pending_modal_{Modal::None};
    bool quit_modal_{false};
    bool quit_now_{false};
    bool save_as_then_quit_{false};
    bool save_as_then_close_{false};
    bool save_as_then_save_all_{false};
    bool restore_tabs_{false};
    int save_as_index_{-1};
    char name_[64]{};
    std::vector<std::string> startup_edit_;
    int startup_selected_{-1};
    bool startup_picking_{false};
    int open_selected_{-1};
};

}  // namespace terminal

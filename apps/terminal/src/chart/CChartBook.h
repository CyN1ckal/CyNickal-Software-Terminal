// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartbookDocument.h"
#include "ui/FinancialsPanel.h"

#include "imgui.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class CChartPane;
class IngestWorker;
class Store;

class CChartBook
{
public:
    explicit CChartBook(int runtime_id);
    ~CChartBook();

    CChartBook(const CChartBook&) = delete;
    CChartBook& operator=(const CChartBook&) = delete;
    CChartBook(CChartBook&&) = delete;
    CChartBook& operator=(CChartBook&&) = delete;

    void drawMenu();
    void draw(Store* store, std::string_view store_error, IngestWorker* ingest);
    [[nodiscard]] bool drawFinancials(Store* store, std::string_view store_error, IngestWorker* ingest);
    void placeFinancials(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] const CChartPane* focusedPane() const;
    void addPane();
    void closeFocused();
    void openFocusedSettings();
    void openFocusedStudies();

    void loadDocument(const CChartbookDocument& document);
    [[nodiscard]] CChartbookDocument exportDocument() const;
    [[nodiscard]] int runtimeId() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    void setName(std::string name);
    [[nodiscard]] const ChartbookData& data() const noexcept;
    void setData(ChartbookData data);
    [[nodiscard]] const ChartbookLayout& layout() const noexcept;
    void setLayout(ChartbookLayout layout);
    [[nodiscard]] const std::vector<ChartbookFloating>& floating() const noexcept;
    void setFloating(std::vector<ChartbookFloating> floating);
    [[nodiscard]] bool consumeLayoutRequest() noexcept;
    void setWindowScope(int runtime_id);
    void placePane(int pane_id, bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] bool containsPane(int pane_id) const;

private:
    void eraseClosed();
    [[nodiscard]] CChartPane* focused();
    [[nodiscard]] const CChartPane* findPane(int pane_id) const;

    int runtime_id_{0};
    std::string name_;
    FinancialsPanel financials_;
    ChartbookData data_{};
    ChartbookLayout layout_{};
    std::vector<ChartbookFloating> floating_;
    std::vector<std::unique_ptr<CChartPane>> panes_;
    int next_id_{1};
    int focused_id_{0};
    bool layout_request_{false};
};

}  // namespace terminal

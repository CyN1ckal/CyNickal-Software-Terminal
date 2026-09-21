#pragma once

#include "imgui.h"
#include "market_data/Store.h"
#include "market_data/Types.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace terminal {

class IngestWorker;

class InventoryPanel
{
public:
    InventoryPanel();
    ~InventoryPanel();

    InventoryPanel(const InventoryPanel&) = delete;
    InventoryPanel& operator=(const InventoryPanel&) = delete;
    InventoryPanel(InventoryPanel&&) = delete;
    InventoryPanel& operator=(InventoryPanel&&) = delete;

    void draw();

private:
    void fillDefaultDates();
    void pollWorker();
    void refreshSummaries();
    void refreshDays();
    void submitIngest();
    void drawToolbar();
    void drawSummaryTable();
    void drawDayTable();
    void applySortSpecs();

    std::filesystem::path db_path_;
    std::unique_ptr<Store> store_;
    std::unique_ptr<IngestWorker> worker_;
    std::string open_error_;
    std::string status_;
    std::vector<CoverageSummary> summaries_;
    std::vector<CoverageDay> days_;
    std::optional<InstrumentId> selected_id_;
    char symbol_[32]{};
    char from_[16]{};
    char to_[16]{};
    std::chrono::steady_clock::time_point last_refresh_;
};

}  // namespace terminal

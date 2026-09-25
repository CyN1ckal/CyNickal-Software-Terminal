// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartLoad.h"
#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "chart/CChartbookDocument.h"
#include "chart/CStudy.h"
#include "chart/CSymbolLink.h"
#include "market_data/Store.h"

#include "imgui.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace terminal {

class IngestWorker;

class CChartPane
{
public:
    explicit CChartPane(int id);

    CChartPane(const CChartPane&) = delete;
    CChartPane& operator=(const CChartPane&) = delete;
    CChartPane(CChartPane&&) = delete;
    CChartPane& operator=(CChartPane&&) = delete;
    ~CChartPane() = default;

    [[nodiscard]] int id() const noexcept;
    [[nodiscard]] bool windowOpen() const noexcept;
    [[nodiscard]] const CChartSettings& settings() const noexcept;
    [[nodiscard]] ChartLoadStatus status() const noexcept;
    // Load message, or the unconfigured / busy prompt when that message is empty.
    [[nodiscard]] std::string_view statusLine() const noexcept;
    [[nodiscard]] std::string_view keyNote() const noexcept;
    [[nodiscard]] int barCount() const noexcept;
    [[nodiscard]] const std::vector<CStudyInstance>& studies() const noexcept;
    [[nodiscard]] bool settingsOpen() const noexcept;
    [[nodiscard]] bool studiesOpen() const noexcept;

    void openSettings();
    // `index` selects that study in the draft. Out of range selects the first.
    void openStudies(int index = 0);
    void closeWindow();
    void requestFocus();
    // Downloads the chart window again, including sessions that already have bars.
    void requestData(Store* store, IngestWorker* ingest);
    void zoomBy(float delta_px);
    void scrollBy(int delta);
    void goToEnd();
    void goToStart();
    // Lower study regions only. Does not reset the price scale.
    void resetStudyScales();
    void setWindowScope(int runtime_id) noexcept;
    void attachSymbolLink(CSymbolLink& link);
    void setSymbolLinkGroup(int group) noexcept;
    void setPlacement(bool force, bool floating, ImGuiID dock, ImVec2 pos, ImVec2 size);
    [[nodiscard]] ChartbookPane exportRecord() const;
    void importRecord(const ChartbookPane& record);

    bool draw(Store* store, std::string_view store_error, IngestWorker* ingest);

private:
    void drawSettingsPopup(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawStudiesPopup();
    // Pins the resolved FIGI (and a renamed ticker) into the live settings.
    void adoptResolvedIdentity(const ChartLoadResult& incoming);
    void applyDraft(Store* store, std::string_view store_error, IngestWorker* ingest);
    void applyLiveSettings(Store* store, std::string_view store_error, IngestWorker* ingest);
    void requestMissingData(Store* store, IngestWorker* ingest);
    void requestSplitSync(Store* store, std::string_view store_error, IngestWorker* ingest);
    void overlayDownloadStatus(Store* store, std::string_view store_error, IngestWorker* ingest);
    void applyStudyDraft();
    void cancelDraft();
    void cancelStudyDraft();
    void reload(Store* store, std::string_view store_error);
    void drawStrip(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawStripScalePopup();
    void drawChartMenu(ImVec2 origin, ImVec2 size);
    void drawStudyLegend(ImVec2 cursor, float width);
    void showEnabledStudyTooltip() const;
    void drawPlotBody();
    void handleChartKeys(Store* store, std::string_view store_error, IngestWorker* ingest);
    void commitKeyBuffer(Store* store, std::string_view store_error, IngestWorker* ingest);
    // Assigns a normalized symbol and clears FIGI. False when the text already matches.
    [[nodiscard]] bool commitSymbol(std::string_view raw);
    void flushSymbolLink();
    static void applyLinkedThunk(void* self, std::string_view symbol);

    int id_{};
    int runtime_id_{0};
    bool place_force_{false};
    bool place_floating_{false};
    ImGuiID place_dock_{0};
    ImVec2 place_pos_;
    ImVec2 place_size_;
    bool window_open_{true};
    bool settings_open_{false};
    bool focus_on_appear_{false};
    bool refocus_keyboard_{false};
    CChartSettings settings_{};
    CChartSettings draft_{};
    CChartSettings loaded_settings_{};
    ChartLoadResult loaded_{};
    char draft_symbol_[32]{};
    std::string key_buffer_;
    std::chrono::steady_clock::time_point key_buffer_at_;
    std::string key_note_;
    ChartDownloadRequest pending_download_{};
    std::uint64_t download_serial_{0};
    std::string download_error_;
    bool coverage_retry_{false};
    bool refresh_requested_{false};
    std::string split_sync_symbol_;
    std::string split_sync_pending_;
    std::uint64_t split_sync_serial_{0};
    CChartViewState view_;
    std::vector<CStudyInstance> studies_;
    std::vector<CStudyInstance> study_draft_;
    std::vector<CStudySeries> computed_;
    int next_study_id_{1};
    int study_draft_selected_{-1};
    bool studies_open_{false};
    CSymbolLink::Binding symbol_link_;
    SymbolLinkGroup draft_link_{SymbolLinkGroup::None};
    bool linked_reload_{false};
    bool link_publish_pending_{false};
    std::string link_publish_symbol_;
    std::chrono::steady_clock::time_point last_reload_;
};

}  // namespace terminal

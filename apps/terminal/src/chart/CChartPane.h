// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#pragma once

#include "chart/CChartLoad.h"
#include "chart/CChartSettings.h"
#include "chart/CChartView.h"
#include "chart/CStudy.h"
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
    [[nodiscard]] const std::vector<CStudyInstance>& studies() const noexcept;
    [[nodiscard]] bool settingsOpen() const noexcept;
    [[nodiscard]] bool studiesOpen() const noexcept;

    void openSettings();
    void openStudies();
    void closeWindow();
    void requestFocus();

    bool draw(Store* store, std::string_view store_error, ImGuiID dock_id, IngestWorker* ingest);

private:
    void drawSettingsPopup(Store* store, std::string_view store_error, IngestWorker* ingest);
    void drawStudiesPopup();
    void applyDraft(Store* store, std::string_view store_error, IngestWorker* ingest);
    void applyLiveSettings(Store* store, std::string_view store_error, IngestWorker* ingest);
    void requestMissingData(Store* store, IngestWorker* ingest);
    void overlayDownloadStatus(Store* store, std::string_view store_error, IngestWorker* ingest);
    void applyStudyDraft();
    void cancelDraft();
    void cancelStudyDraft();
    void reload(Store* store, std::string_view store_error);
    void drawStatusLine() const;
    void drawKeyBuffer() const;
    void drawPlotBody();
    void handleChartKeys(Store* store, std::string_view store_error, IngestWorker* ingest);
    void commitKeyBuffer(Store* store, std::string_view store_error, IngestWorker* ingest);

    int id_{};
    bool window_open_{true};
    bool settings_open_{false};
    bool focus_on_appear_{false};
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
    CChartViewState view_;
    std::vector<CStudyInstance> studies_;
    std::vector<CStudyInstance> study_draft_;
    std::vector<CStudySeries> computed_;
    int next_study_id_{1};
    int study_draft_selected_{-1};
    bool studies_open_{false};
    std::chrono::steady_clock::time_point last_reload_;
};

}  // namespace terminal

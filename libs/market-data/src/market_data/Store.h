#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace myapp {

enum class StoreMode : std::uint8_t
{
    Writer,
    Reader
};

class Store
{
public:
    explicit Store(std::filesystem::path db_path, StoreMode mode = StoreMode::Writer);
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    Store(Store&&) = delete;
    Store& operator=(Store&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] int userVersion() const;
    [[nodiscard]] std::vector<std::string> tableNames() const;
    [[nodiscard]] bool foreignKeysEnabled() const;

    static void testingSetUserVersion(const std::filesystem::path& path, int version);

    InstrumentId upsertInstrument(const Instrument& instrument);
    [[nodiscard]] std::optional<Instrument> findInstrument(
        std::string_view symbol,
        std::optional<std::string_view> exchange = std::nullopt) const;
    [[nodiscard]] std::optional<Instrument> findInstrumentById(InstrumentId id) const;

    UpsertBarsResult upsertBars(std::span<const Bar> bars);
    [[nodiscard]] std::vector<Bar> queryBars(InstrumentId id,
                                             int timeframe_s,
                                             UnixSeconds ts_begin,
                                             UnixSeconds ts_end) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace myapp

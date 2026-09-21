#pragma once

#include "market_data/Types.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace myapp

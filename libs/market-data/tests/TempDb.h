#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

class TempDb
{
public:
    TempDb()
    {
        static std::atomic<std::uint64_t> seq{0};
        const auto n = seq.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() /
                ("myapp-md-" + std::to_string(::getpid()) + "-" + std::to_string(n) + ".sqlite");
        std::filesystem::remove(path_);
        removeSidecars();
    }

    ~TempDb()
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(walPath(), ec);
        std::filesystem::remove(shmPath(), ec);
    }

    TempDb(const TempDb&) = delete;
    TempDb& operator=(const TempDb&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    void removeSidecars() const
    {
        std::error_code ec;
        std::filesystem::remove(walPath(), ec);
        std::filesystem::remove(shmPath(), ec);
    }

    [[nodiscard]] std::filesystem::path walPath() const { return std::filesystem::path(path_.string() + "-wal"); }
    [[nodiscard]] std::filesystem::path shmPath() const { return std::filesystem::path(path_.string() + "-shm"); }

    std::filesystem::path path_;
};

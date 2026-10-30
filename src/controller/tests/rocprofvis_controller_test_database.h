// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>

namespace RocProfVis
{
namespace Controller
{
namespace Test
{

// Declare before controller handles so the copied database outlives them.
class TemporaryDatabase
{
public:
    TemporaryDatabase()                                    = default;
    TemporaryDatabase(const TemporaryDatabase&)            = delete;
    TemporaryDatabase& operator=(const TemporaryDatabase&) = delete;

    ~TemporaryDatabase()
    {
        if(!m_path.empty())
        {
            std::error_code error;
            std::filesystem::remove(m_path, error);
        }
    }

    const std::filesystem::path& Path() const { return m_path; }

    void Copy(const std::filesystem::path& source, const char* prefix)
    {
        REQUIRE(m_path.empty());
        const std::string suffix =
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path destination =
            std::filesystem::temp_directory_path() / (prefix + suffix + ".db");
        std::error_code error;
        const bool      copied = std::filesystem::copy_file(source, destination, error);
        INFO(error.message());
        REQUIRE_FALSE(error);
        REQUIRE(copied);
        m_path = destination;
    }

    void Execute(const std::string& sql) const
    {
        sqlite3*  raw_db = nullptr;
        const int opened = sqlite3_open(m_path.string().c_str(), &raw_db);
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw_db, &sqlite3_close);
        REQUIRE(opened == SQLITE_OK);
        char*     error  = nullptr;
        const int result = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &error);
        const std::string message = error ? error : "";
        sqlite3_free(error);
        INFO(message);
        REQUIRE(result == SQLITE_OK);
    }

private:
    std::filesystem::path m_path;
};

}  // namespace Test
}  // namespace Controller
}  // namespace RocProfVis

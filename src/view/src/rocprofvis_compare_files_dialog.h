// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

class CompareFilesDialog
{
public:
    enum class FileSlot
    {
        kFirst,
        kSecond
    };

    using BrowseCallback = std::function<void(FileSlot)>;
    using OpenCallback   = std::function<void(const std::string&)>;

    CompareFilesDialog(BrowseCallback browse_callback, OpenCallback open_callback);

    void Show();
    void SetFilePath(FileSlot slot, const std::string& file_path);
    void Render();

private:
    std::string WriteManifest();

    BrowseCallback m_browse_callback;
    OpenCallback   m_open_callback;
    bool           m_should_open = false;
    std::string    m_first_file;
    std::string    m_second_file;
    std::string    m_error_message;
};

}  // namespace View
}  // namespace RocProfVis

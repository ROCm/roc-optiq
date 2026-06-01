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
    using OpenCallback =
        std::function<void(const std::string& first, const std::string& second)>;

    CompareFilesDialog(BrowseCallback browse_callback, OpenCallback open_callback);

    void Show();
    void SetFilePath(FileSlot slot, const std::string& file_path);
    void Render();

    // True while the modal is showing. Lets the app route dropped/opened files into the
    // dialog's slots instead of opening them as standalone traces behind the modal.
    bool IsOpen() const { return m_is_open; }
    // Fills the next empty slot (or overwrites the second once both are set).
    void AddDroppedFile(const std::string& file_path);

private:
    // Validates the two selections, returns true and clears any error when they are a
    // usable pair (both set and not the same file).
    bool Validate();

    BrowseCallback m_browse_callback;
    OpenCallback   m_open_callback;
    bool           m_should_open = false;
    bool           m_is_open     = false;
    std::string    m_first_file;
    std::string    m_second_file;
    std::string    m_error_message;
};

}  // namespace View
}  // namespace RocProfVis

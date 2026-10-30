// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

#include <array>
#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

/* Picks the two traces of a compare project: the baseline (A) and the target (B)
 * compared against it. Files are browsed for, dropped onto the window, or taken
 * from the recent list, and are checked as they are picked so the dialog can say
 * what is wrong before anything is opened.
 */
class CompareFilesDialog
{
    friend struct CompareFilesDialogTestPeer;

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

    // Opens the dialog, with baseline already in the first slot when given.
    void Show(const std::string& baseline = "");
    void SetFilePath(FileSlot slot, const std::string& file_path);
    void Render();

    // True while the modal is showing. Lets the app route dropped/opened files into the
    // dialog's slots instead of opening them as standalone traces behind the modal.
    bool IsOpen() const { return m_is_open; }
    // Fills the next empty slot (or overwrites the second once both are set).
    void AddDroppedFile(const std::string& file_path);

private:
    // What a slot holds, read once when it is set rather than every frame.
    struct SlotFile
    {
        std::string path;
        std::string name;
        std::string folder;
        std::string details;  // Size and last modified time
        std::string problem;  // Why the file cannot be compared; empty when it can
    };

    static SlotFile Describe(const std::string& file_path);
    void            RenderSlot(FileSlot slot, const ImVec2& size);
    void            RenderRecent(FileSlot slot);
    void            Close();
    // Reads whether the two picks come from one profiling run, once per pair of picks:
    // it opens both databases.
    void CheckPair();
    // Why the two picks cannot be compared together; empty when they can.
    std::string PairProblem() const;
    bool        CanCompare() const;

    BrowseCallback             m_browse_callback;
    OpenCallback               m_open_callback;
    bool                       m_should_open = false;
    bool                       m_is_open     = false;
    std::array<SlotFile, 2>    m_slots;
    std::array<std::string, 2> m_checked_picks;
    // The checked picks are copies of one trace, which cannot be compared.
    bool m_same_run = false;
};

}  // namespace View
}  // namespace RocProfVis

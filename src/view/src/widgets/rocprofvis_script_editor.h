// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_event_manager.h"
#include "rocprofvis_widget.h"

#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

class DataProvider;

// Floating Python editor. Sends the source string through DataProvider
// (same request/poll path as other controller work) and shows
// optiq.result.text. Load/Save go through AppWindow file dialogs.
class ScriptEditor : public RocWidget
{
public:
    static ScriptEditor* GetInstance();
    static void          DestroyInstance();

    void  ToggleVisible();
    bool* VisiblePtr();

    void Render() override;

    static void RenderToolbarButton();

private:
    ScriptEditor();
    ~ScriptEditor() override;

    void          Run();
    void          Cancel();
    void          LoadFromFile();
    void          SaveToFile();
    void          ReadFile(const std::string& path);
    void          WriteFile(const std::string& path);
    void          RenderToolbar();
    void          RenderSource();
    void          RenderOutput();
    DataProvider* CurrentDataProvider() const;
    DataProvider* DataProviderForProject(const std::string& project_id) const;
    bool          CanRun() const;

    static ScriptEditor* s_instance;

    bool        m_visible;
    bool        m_running;
    uint64_t    m_progress_percent;
    std::string m_source;
    std::string m_output;
    std::string m_status;
    std::string m_file_path;
    std::string m_running_project_id;

    EventManager::SubscriptionToken m_complete_token;
    EventManager::SubscriptionToken m_progress_token;
};

}  // namespace View
}  // namespace RocProfVis

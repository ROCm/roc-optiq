// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_widget.h"
#include "model/compute/rocprofvis_compute_model_types.h"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class QueryBuilder : public RocWidget
{
public:
    QueryBuilder();
    ~QueryBuilder() = default;

    void Render() override;
    void Open();

    void SetWorkloads(const std::unordered_map<uint32_t, WorkloadInfo>* workloads);

    std::string GetQueryString() const;
    bool        HasCompleteSelection() const;

    std::optional<uint32_t>    GetSelectedWorkloadId() const;
    std::optional<uint32_t>    GetSelectedCategoryId() const;
    std::optional<uint32_t>    GetSelectedTableId() const;
    std::optional<uint32_t>    GetSelectedEntryId() const;
    std::optional<std::string> GetSelectedValueName() const;

    using SelectionCallback = std::function<void(const std::string&)>;
    void SetSelectionChangedCallback(const SelectionCallback& callback);

private:
    static constexpr int LEVEL_WORKLOAD   = 0;
    static constexpr int LEVEL_CATEGORY   = 1;
    static constexpr int LEVEL_TABLE      = 2;
    static constexpr int LEVEL_ENTRY      = 3;
    static constexpr int LEVEL_VALUE_NAME = 4;
    static constexpr int LEVEL_COUNT      = 5;

    struct Selection
    {
        uint32_t    id;
        std::string name;
    };

    struct ListEntry
    {
        uint32_t    id;
        std::string label;
        std::string tooltip;
    };

    void                    RenderTags();
    void                    RenderList();
    std::vector<ListEntry>  GetCurrentLevelItems() const;
    void                    SelectItem(const ListEntry& item);
    void                    ClearFromLevel(int level);
    const char*             GetLevelLabel(int level) const;

    const std::unordered_map<uint32_t, WorkloadInfo>* m_workloads = nullptr;

    int                                     m_current_level = 0;
    std::vector<std::optional<Selection>>   m_selections;
    std::string                             m_value_name;
    bool                                    m_should_open        = false;
    bool                                    m_scroll_tags_to_end = false;

    SelectionCallback m_on_changed;
};

}  // namespace View
}  // namespace RocProfVis

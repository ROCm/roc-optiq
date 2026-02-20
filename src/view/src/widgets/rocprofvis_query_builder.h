// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_widget.h"
#include "model/compute/rocprofvis_compute_model_types.h"

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

    void Render() override;
    void Open();

    void SetWorkloads(const std::unordered_map<uint32_t, WorkloadInfo>* workloads);

    std::string GetQueryString() const;

    std::optional<uint32_t> GetSelectedWorkloadId() const;

private:
    static constexpr int LEVEL_WORKLOAD   = 0;
    static constexpr int LEVEL_CATEGORY   = 1;
    static constexpr int LEVEL_TABLE      = 2;
    static constexpr int LEVEL_ENTRY      = 3;
    static constexpr int LEVEL_VALUE_NAME = 4;
    static constexpr int LEVEL_COUNT      = 5;

    struct LevelItem
    {
        uint32_t    id;
        std::string label;
    };

    void                   RenderTags();
    void                   RenderList();
    std::vector<LevelItem> GetItems() const;
    void                   Select(int level, const LevelItem& item);
    void                   ClearFrom(int level);
    const char*            GetLevelLabel(int level) const;

    const std::unordered_map<uint32_t, WorkloadInfo>* m_workloads = nullptr;

    int                                    m_level           = 0;
    bool                                   m_should_open     = false;
    bool                                   m_scroll_to_end   = false;
    std::string                            m_value_name;
    std::vector<std::optional<LevelItem>>  m_selections;
};

}  // namespace View
}  // namespace RocProfVis

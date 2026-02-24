// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_widget.h"
#include <optional>
#include <string>
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

    void SetWorkload(const WorkloadInfo* workload);

    std::string GetQueryString() const;

private:
    static constexpr int LEVEL_CATEGORY   = 0;
    static constexpr int LEVEL_TABLE      = 1;
    static constexpr int LEVEL_ENTRY      = 2;
    static constexpr int LEVEL_VALUE_NAME = 3;
    static constexpr int LEVEL_COUNT      = 4;

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

    const WorkloadInfo* m_workload = nullptr;

    int                                   m_level         = 0;
    bool                                  m_should_open   = false;
    bool                                  m_scroll_to_end = false;
    std::string                           m_value_name;
    std::vector<std::optional<LevelItem>> m_selections;
};

}  // namespace View
}  // namespace RocProfVis

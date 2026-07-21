// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Reads compute metric values for a kernel directly out of a rocprof-compute
// trace database (SQLite), plus the list of workloads/kernels and the optional
// memory_chart_extdata layout blob. This is a self-contained stand-in for the
// full model/controller/data-provider stack used by the main app: the values
// are pre-stored in the DB (via compute_kernel_metric_view), so a direct SELECT
// is all that is needed.

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace mcv
{

// A single resolved metric for the selected kernel.
struct ResolvedMetric
{
    std::string metric_id;    // Canonical dotted id, e.g. "3.1.41".
    std::string name;         // Display name from compute_metric_definition.
    std::string unit;         // May be empty.
    std::string description;  // May be empty.
    double      value     = 0.0;
    bool        has_value = false;
};

struct WorkloadRow
{
    uint32_t    id = 0;
    std::string name;
    std::string sub_name;
};

struct KernelRow
{
    int64_t     uuid = 0;
    std::string name;
};

class MetricStore
{
public:
    MetricStore() = default;
    ~MetricStore();

    // Open a trace DB read-only. Returns false and sets *error on failure.
    bool Open(const std::string& db_path, std::string* error);
    void Close();
    bool IsOpen() const { return m_db != nullptr; }

    const std::vector<WorkloadRow>& Workloads() const { return m_workloads; }

    // Kernels that have metrics in `category_id` for the given workload. Falls
    // back to every kernel in the workload when none carry category metrics.
    std::vector<KernelRow> KernelsForWorkload(uint32_t workload_id,
                                              uint32_t category_id) const;

    // Load every metric in `category_id` for (workload, kernel) into the lookup
    // maps, replacing any previously loaded set.
    bool LoadKernelMetrics(uint32_t workload_id, int64_t kernel_uuid,
                           uint32_t category_id, std::string* error);

    // Raw memory_chart_extdata JSON for a workload ("" when the column/blob is
    // absent).
    std::string ReadLayoutBlob(uint32_t workload_id) const;
    bool        HasLayoutColumn() const { return m_has_layout_col; }

    const ResolvedMetric* ById(const std::string& dotted_id) const;
    const ResolvedMetric* ByName(const std::string& name) const;
    size_t                LoadedMetricCount() const { return m_metrics.size(); }

    const std::string& SchemaVersion() const { return m_schema_version; }
    const std::string& MetricViewName() const { return m_view_name; }

private:
    bool TableOrViewExists(const std::string& name) const;
    void ClearMetrics();

    sqlite3*    m_db = nullptr;
    std::string m_schema_version;
    std::string m_view_name;  // Resolved kernel-metric view name.
    bool        m_has_layout_col = false;

    std::vector<WorkloadRow>    m_workloads;
    std::vector<ResolvedMetric> m_metrics;  // Backing storage (stable pointers).
    std::unordered_map<std::string, const ResolvedMetric*> m_by_id;
    std::unordered_map<std::string, const ResolvedMetric*> m_by_name;
};

}  // namespace mcv

// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "metric_store.h"

#include "sqlite3.h"

#include <unordered_map>

namespace mcv
{

namespace
{

std::string
ColumnText(sqlite3_stmt* stmt, int col)
{
    if(sqlite3_column_type(stmt, col) == SQLITE_NULL) return "";
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

}  // namespace

MetricStore::~MetricStore() { Close(); }

void
MetricStore::Close()
{
    ClearMetrics();
    if(m_db)
    {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
    m_workloads.clear();
    m_schema_version.clear();
    m_view_name.clear();
    m_has_layout_col = false;
}

void
MetricStore::ClearMetrics()
{
    m_metrics.clear();
    m_by_id.clear();
    m_by_name.clear();
}

bool
MetricStore::TableOrViewExists(const std::string& name) const
{
    if(!m_db) return false;
    sqlite3_stmt* stmt = nullptr;
    const char*   sql =
        "SELECT 1 FROM sqlite_master WHERE type IN ('table','view') AND name = ? LIMIT 1";
    if(sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool
MetricStore::Open(const std::string& db_path, std::string* error)
{
    Close();

    int rc = sqlite3_open_v2(db_path.c_str(), &m_db, SQLITE_OPEN_READONLY, nullptr);
    if(rc != SQLITE_OK)
    {
        if(error)
        {
            *error = "cannot open database '" + db_path + "': " +
                     (m_db ? sqlite3_errmsg(m_db) : sqlite3_errstr(rc));
        }
        if(m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    if(!TableOrViewExists("compute_workload"))
    {
        if(error)
        {
            *error = "'" + db_path +
                     "' is not a rocprof-compute trace (no compute_workload table).";
        }
        Close();
        return false;
    }

    // Schema version (best effort).
    {
        sqlite3_stmt* stmt = nullptr;
        if(sqlite3_prepare_v2(m_db, "SELECT schema_version FROM compute_metadata LIMIT 1",
                              -1, &stmt, nullptr) == SQLITE_OK)
        {
            if(sqlite3_step(stmt) == SQLITE_ROW) m_schema_version = ColumnText(stmt, 0);
            sqlite3_finalize(stmt);
        }
    }

    // Pick the view that carries per-kernel metric values.
    if(TableOrViewExists("compute_kernel_metric_view"))
        m_view_name = "compute_kernel_metric_view";
    else if(TableOrViewExists("compute_metric_view"))
        m_view_name = "compute_metric_view";
    else
        m_view_name.clear();

    // Detect the optional memory_chart_extdata layout blob column.
    {
        sqlite3_stmt* stmt = nullptr;
        const char*   sql =
            "SELECT name FROM pragma_table_info('compute_workload') "
            "WHERE name = 'memory_chart_extdata'";
        if(sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            m_has_layout_col = (sqlite3_step(stmt) == SQLITE_ROW);
            sqlite3_finalize(stmt);
        }
    }

    // Workload list. sub_name is optional across schema versions, so fall back.
    auto load_workloads = [&](bool with_sub_name) -> bool {
        const char* sql = with_sub_name
                              ? "SELECT workload_id, name, sub_name FROM compute_workload "
                                "ORDER BY workload_id"
                              : "SELECT workload_id, name FROM compute_workload "
                                "ORDER BY workload_id";
        sqlite3_stmt* stmt = nullptr;
        if(sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        m_workloads.clear();
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            WorkloadRow row;
            row.id   = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0));
            row.name = ColumnText(stmt, 1);
            if(with_sub_name) row.sub_name = ColumnText(stmt, 2);
            m_workloads.push_back(std::move(row));
        }
        sqlite3_finalize(stmt);
        return true;
    };
    if(!load_workloads(true)) load_workloads(false);

    return true;
}

std::vector<KernelRow>
MetricStore::KernelsForWorkload(uint32_t workload_id, uint32_t category_id) const
{
    std::vector<KernelRow> kernels;
    if(!m_db) return kernels;

    const std::string like = std::to_string(category_id) + ".%";

    // Prefer kernels that actually carry metrics in this category.
    if(!m_view_name.empty())
    {
        const std::string sql = "SELECT DISTINCT kernel_uuid, kernel_name FROM " +
                                m_view_name +
                                " WHERE workload_id = ? AND metric_id LIKE ? "
                                "ORDER BY kernel_uuid";
        sqlite3_stmt* stmt = nullptr;
        if(sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, workload_id);
            sqlite3_bind_text(stmt, 2, like.c_str(), -1, SQLITE_TRANSIENT);
            while(sqlite3_step(stmt) == SQLITE_ROW)
            {
                KernelRow row;
                row.uuid = sqlite3_column_int64(stmt, 0);
                row.name = ColumnText(stmt, 1);
                kernels.push_back(std::move(row));
            }
            sqlite3_finalize(stmt);
        }
    }

    if(!kernels.empty()) return kernels;

    // Fallback: every kernel in the workload.
    sqlite3_stmt* stmt = nullptr;
    const char*   sql  = "SELECT kernel_uuid, kernel_name FROM compute_kernel "
                         "WHERE workload_id = ? ORDER BY kernel_uuid";
    if(sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int64(stmt, 1, workload_id);
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            KernelRow row;
            row.uuid = sqlite3_column_int64(stmt, 0);
            row.name = ColumnText(stmt, 1);
            kernels.push_back(std::move(row));
        }
        sqlite3_finalize(stmt);
    }
    return kernels;
}

bool
MetricStore::LoadKernelMetrics(uint32_t workload_id, int64_t kernel_uuid,
                               uint32_t category_id, std::string* error)
{
    ClearMetrics();
    if(!m_db) return false;
    if(m_view_name.empty())
    {
        if(error) *error = "no compute metric view found in this database";
        return false;
    }

    const std::string like = std::to_string(category_id) + ".%";
    const std::string sql =
        "SELECT metric_id, metric_name, description, unit, value_name, value FROM " +
        m_view_name +
        " WHERE workload_id = ? AND kernel_uuid = ? AND metric_id LIKE ?";

    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        if(error) *error = sqlite3_errmsg(m_db);
        return false;
    }
    sqlite3_bind_int64(stmt, 1, workload_id);
    sqlite3_bind_int64(stmt, 2, kernel_uuid);
    sqlite3_bind_text(stmt, 3, like.c_str(), -1, SQLITE_TRANSIENT);

    // Keep the first value_name encountered per metric id (mirrors the app,
    // which shows a single value per metric).
    std::unordered_map<std::string, size_t> index_by_id;
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::string metric_id = ColumnText(stmt, 0);
        if(metric_id.empty() || index_by_id.count(metric_id)) continue;

        ResolvedMetric m;
        m.metric_id   = metric_id;
        m.name        = ColumnText(stmt, 1);
        m.description = ColumnText(stmt, 2);
        m.unit        = ColumnText(stmt, 3);
        if(sqlite3_column_type(stmt, 5) != SQLITE_NULL)
        {
            m.value     = sqlite3_column_double(stmt, 5);
            m.has_value = true;
        }
        index_by_id[metric_id] = m_metrics.size();
        m_metrics.push_back(std::move(m));
    }
    sqlite3_finalize(stmt);

    // Build lookup maps once storage is stable.
    m_by_id.reserve(m_metrics.size());
    m_by_name.reserve(m_metrics.size());
    for(const ResolvedMetric& m : m_metrics)
    {
        m_by_id[m.metric_id] = &m;
        if(!m.name.empty()) m_by_name[m.name] = &m;
    }
    return true;
}

std::string
MetricStore::ReadLayoutBlob(uint32_t workload_id) const
{
    if(!m_db || !m_has_layout_col) return "";
    sqlite3_stmt* stmt = nullptr;
    const char*   sql =
        "SELECT memory_chart_extdata FROM compute_workload WHERE workload_id = ?";
    if(sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return "";
    sqlite3_bind_int64(stmt, 1, workload_id);
    std::string blob;
    if(sqlite3_step(stmt) == SQLITE_ROW) blob = ColumnText(stmt, 0);
    sqlite3_finalize(stmt);
    return blob;
}

const ResolvedMetric*
MetricStore::ById(const std::string& dotted_id) const
{
    std::unordered_map<std::string, const ResolvedMetric*>::const_iterator it =
        m_by_id.find(dotted_id);
    return it != m_by_id.end() ? it->second : nullptr;
}

const ResolvedMetric*
MetricStore::ByName(const std::string& name) const
{
    std::unordered_map<std::string, const ResolvedMetric*>::const_iterator it =
        m_by_name.find(name);
    return it != m_by_name.end() ? it->second : nullptr;
}

}  // namespace mcv

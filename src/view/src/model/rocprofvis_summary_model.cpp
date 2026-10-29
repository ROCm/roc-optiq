// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_summary_model.h"
#include "../rocprofvis_settings_manager.h"
#include "../rocprofvis_utils.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_topology_model.h"

namespace RocProfVis
{
namespace View
{

SummaryModel::SummaryModel()
: m_revision(0)
{}

const SummaryInfo::AggregateMetrics&
SummaryModel::GetSummaryData() const
{
    return m_summary;
}

SummaryInfo::AggregateMetrics&
SummaryModel::GetSummaryData()
{
    return m_summary;
}

const TablesModel&
SummaryModel::GetTables() const
{
    return m_tables;
}

TablesModel&
SummaryModel::GetTables()
{
    return m_tables;
}

const std::vector<SummaryInfo::AggregateMetrics>&
SummaryModel::GetSummaryNodes() const
{
    return m_summary.sub_metrics;
}

const SummaryInfo::TopOpEvents*
SummaryModel::TopOpEvents(const SummaryInfo::AggregateMetrics&   metrics,
                          rocprofvis_analysis_summary_category_t type) const
{
    const SummaryInfo::TopOpEvents* op_events = nullptr;
    switch(type)
    {
        case kRPVAnalysisSummaryCategoryKernel:
        {
            op_events = &metrics.gpu.dispatch;
            break;
        }
        case kRPVAnalysisSummaryCategoryMemory:
        {
            if(metrics.type == kRPVAnalysisSummaryAggregationLevelProcessor &&
               metrics.info.processor->type == kRPVControllerProcessorTypeGPU)
            {
                op_events = &metrics.gpu.memory;
            }
            else if(metrics.type == kRPVAnalysisSummaryAggregationLevelProcessor &&
                    metrics.info.processor->type == kRPVControllerProcessorTypeCPU)
            {
                op_events = &metrics.cpu.memory;
            }
            else
            {
                op_events = &metrics.memory;
            }
            break;
        }
        case kRPVAnalysisSummaryCategoryThread:
        {
            op_events = &metrics.thread;
            break;
        }
        default:
        {
            ROCPROFVIS_ASSERT(false);
            break;
        }
    }
    return op_events;
}

void
SummaryModel::RegisterEventValue(SummaryInfo::Value& value)
{
    m_event_values.push_back(&value);
}

void
SummaryModel::RegisterCounterValue(SummaryInfo::Value& value)
{
    m_counter_values.push_back(&value);
}

void
SummaryModel::RegisterTimeunitValue(SummaryInfo::Value& value)
{
    m_timeunit_values.push_back(&value);
}

void
SummaryModel::UpdateValueStrings()
{
    for(SummaryInfo::Value* value : m_event_values)
    {
        if(value->str.empty())
        {
            switch(value->type)
            {
                case SummaryInfo::Value::Type::UInt:
                {
                    value->str = std::to_string(value->numeric.uint);
                    break;
                }
                case SummaryInfo::Value::Type::Double:
                {
                    value->str = full_number_format(value->numeric.dbl);
                    break;
                }
                case SummaryInfo::Value::Type::Float:
                {
                    value->str =
                        full_number_format(static_cast<double>(value->numeric.flt));
                    break;
                }
            }
        }
    }
    for(SummaryInfo::Value* value : m_counter_values)
    {
        if(value->str.empty())
        {
            switch(value->type)
            {
                case SummaryInfo::Value::Type::UInt:
                {
                    value->str =
                        compact_number_format(static_cast<double>(value->numeric.uint));
                    break;
                }
                case SummaryInfo::Value::Type::Double:
                {
                    value->str = compact_number_format(value->numeric.dbl);
                    break;
                }
                case SummaryInfo::Value::Type::Float:
                {
                    value->str =
                        full_number_format(static_cast<double>(value->numeric.flt));
                    break;
                }
            }
        }
    }
    TimeFormat timeformat =
        SettingsManager::GetInstance().GetUserSettings().unit_settings.time_format;
    for(SummaryInfo::Value* value : m_timeunit_values)
    {
        switch(value->type)
        {
            case SummaryInfo::Value::Type::UInt:
            {
                value->str = nanosecond_to_formatted_str(
                    static_cast<double>(value->numeric.uint), timeformat);
                break;
            }
            case SummaryInfo::Value::Type::Double:
            {
                value->str = nanosecond_to_formatted_str(value->numeric.dbl, timeformat);
                break;
            }
            case SummaryInfo::Value::Type::Float:
            {
                value->str = nanosecond_to_formatted_str(
                    static_cast<double>(value->numeric.flt), timeformat);
                break;
            }
        }
    }
}

void
SummaryModel::SetRevision(uint64_t revision)
{
    m_revision = revision;
}

uint64_t
SummaryModel::GetRevision() const
{
    return m_revision;
}

void
SummaryModel::Clear()
{
    m_summary = SummaryInfo::AggregateMetrics{};
    m_tables.ClearAllTables();
    m_event_values.clear();
    m_counter_values.clear();
    m_timeunit_values.clear();
    m_revision = 0;
}

}  // namespace View
}  // namespace RocProfVis

// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table_system_search.h"
#include "rocprofvis_controller_arguments.h"

namespace RocProfVis
{
namespace Controller
{

EventSearchTable::EventSearchTable(uint64_t id)
: SystemTable(id)
, m_include_substrings(true)
, m_include_category(false)
, m_partial_matching(false)
{
}

EventSearchTable::~EventSearchTable()
{
}

void EventSearchTable::Reset()
{
    SystemTable::Reset();
    m_string_table_filters.clear();
    m_include_substrings = true;
    m_include_category = false;
    m_partial_matching = false;
}

rocprofvis_result_t
EventSearchTable::UnpackArguments(Arguments& args, TableArguments*& out) const
{
    if(!out)
    {
        out = new EventSearchTableArguments();
    }
    EventSearchTableArguments* search_out = (EventSearchTableArguments*)out;
    rocprofvis_result_t result = SystemTable::UnpackArguments(args, out);
    if(result == kRocProfVisResultSuccess)
    {
        uint64_t num_string_table_filters = 0;
        uint64_t include_substrings = 1;
        uint64_t include_category = 0;
        uint64_t partial_matching = 0;
        std::vector<std::string> string_table_filters;
        uint64_t table_type = static_cast<uint64_t>(kRPVControllerTableTypeSearchResults);

        result = args.GetUInt64(kRPVControllerTableArgsType, 0, &table_type);
        if(result == kRocProfVisResultSuccess &&
           (table_type == kRPVControllerTableTypeSearchResults ||
            table_type == kRPVControllerTableTypeSummaryKernelInstances))
        {
            result = args.GetUInt64(kRPVControllerTableArgsNumStringTableFilters, 0, &num_string_table_filters);
            if(result == kRocProfVisResultSuccess)
            {
                for (uint32_t i = 0; i < num_string_table_filters && (result == kRocProfVisResultSuccess); i++)
                {
                    uint32_t length = 0;
                    result = args.GetString(kRPVControllerTableArgsStringTableFiltersIndexed, i, nullptr, &length);
                    if(result == kRocProfVisResultSuccess && length > 0)
                    {
                        std::string f;
                        f.resize(length);
                        result = args.GetString(kRPVControllerTableArgsStringTableFiltersIndexed, i, f.data(), &length);
                        if(result == kRocProfVisResultSuccess)
                        {
                            string_table_filters.push_back(f);
                        }
                    }
                }
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = args.GetUInt64(kRPVControllerTableArgsStringTableFiltersIncludeSubstrings, 0, &include_substrings);
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = args.GetUInt64(kRPVControllerTableArgsStringTableFiltersIncludeCategory, 0, &include_category);
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = args.GetUInt64(kRPVControllerTableArgsStringTableFiltersPartialMatching, 0, &partial_matching);
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            search_out->m_string_table_filters = std::move(string_table_filters);
            search_out->m_include_substrings = include_substrings != 0;
            search_out->m_include_category = include_category != 0;
            search_out->m_partial_matching = partial_matching != 0;
        }
    }
	return result;
}

void
EventSearchTable::GetCurrentArguments(TableArguments*& out) const
{
    if(!out)
    {
        out = new EventSearchTableArguments();
    }
    EventSearchTableArguments* search_out = (EventSearchTableArguments*)out;
    SystemTable::GetCurrentArguments(out);    
    search_out->m_string_table_filters = m_string_table_filters;
    search_out->m_include_substrings = m_include_substrings;
    search_out->m_include_category = m_include_category;
    search_out->m_partial_matching = m_partial_matching;
}

void
EventSearchTable::SetCurrentArguments(TableArguments& in)
{
    EventSearchTableArguments& search_in = (EventSearchTableArguments&)in;
    SystemTable::SetCurrentArguments(search_in);
    m_string_table_filters = search_in.m_string_table_filters;
    m_include_substrings = search_in.m_include_substrings;
    m_include_category = search_in.m_include_category;
    m_partial_matching = search_in.m_partial_matching;
}

bool
EventSearchTable::ArgumentsChanged(SystemTableArguments& in) const
{
    EventSearchTableArguments& search_in = (EventSearchTableArguments&)in;
    return SystemTable::ArgumentsChanged(search_in) || m_string_table_filters != search_in.m_string_table_filters || m_include_substrings != search_in.m_include_substrings || m_include_category != search_in.m_include_category || m_partial_matching != search_in.m_partial_matching;
}

rocprofvis_result_t
EventSearchTable::UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const
{
    (void) args;
    out = kRPVDMTableUseCaseEventSearch;
    return kRocProfVisResultSuccess;
}

rocprofvis_dm_result_t
EventSearchTable::BuildQuery(rocprofvis_dm_database_t db, TableArguments& args, uint64_t index, uint64_t count, bool count_only, char** out) const
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
    EventSearchTableArguments& arguments = (EventSearchTableArguments&)args;
    char const* sort_column = count_only ? nullptr : (m_columns.size() > arguments.m_sort_column ? m_columns[arguments.m_sort_column].m_name.c_str() : nullptr);
    std::vector<const char*> string_table_filters_ptr;
    for(const std::string& filter : arguments.m_string_table_filters)
    {
        string_table_filters_ptr.push_back(filter.c_str());
    }
    result = rocprofvis_db_build_event_search_query(db, 
                                                    static_cast<rocprofvis_dm_timestamp_t>(arguments.m_start_ts), static_cast<rocprofvis_dm_timestamp_t>(arguments.m_end_ts), 
                                                    static_cast<rocprofvis_db_num_of_tracks_t>(arguments.m_tracks.size()), arguments.m_tracks.data(),
                                                    arguments.m_where.c_str(),
                                                    static_cast<rocprofvis_dm_num_string_table_filters_t>(string_table_filters_ptr.size()), string_table_filters_ptr.data(),
                                                    arguments.m_include_substrings, arguments.m_include_category, arguments.m_partial_matching,
                                                    sort_column, (rocprofvis_dm_sort_order_t)arguments.m_sort_order,
                                                    count_only ? 0 : count, count_only ? 0 : index, count_only, out);
    return result;
}

}
}

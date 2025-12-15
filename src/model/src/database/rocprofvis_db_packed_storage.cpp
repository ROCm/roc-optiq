// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rocprofvis_db_packed_storage.h"
#include "rocprofvis_db_profile.h"
#include <numeric>
#include <execution>
#include <cfloat>

namespace RocProfVis
{
namespace DataModel
{

    uint8_t PackedTable::ColumnTypeSize(ColumnType type)
    {
        switch (type)
        {
        case ColumnType::Null:   return 0;
        case ColumnType::Byte:   return 1;
        case ColumnType::Word:   return 2;
        case ColumnType::Dword:  return 4;
        case ColumnType::Qword:  return 8;
        case ColumnType::Double: return 8;
        default: throw std::runtime_error("Invalid ColumnType");
        }
    }

    void PackedTable::AddColumn(const std::string& name, ColumnType type, uint8_t orig_index, uint8_t schema_index)
    {
        m_columns.push_back({name, orig_index, type, m_rowSize, schema_index});
        m_rowSize += ColumnTypeSize(type);
    }

    void PackedTable::AddMergedColumn(const std::string& name, uint8_t op, ColumnType type, uint8_t offset, uint8_t schema_index)
    {
        if (op >= kRocProfVisDmNumOperation)
        {
            throw std::runtime_error("Invalid operation");
        }
        auto it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [name](MergedColumnDef& c) { return c.m_name == name; });
        if (it == m_merged_columns.end())
        {
            m_merged_columns.push_back({ name, 0, schema_index });
            m_merged_columns.back().m_op_mask |= (uint32_t)1 << op;
            m_merged_columns.back().m_offset[op] = offset;
            m_merged_columns.back().m_schema_index[op] = schema_index;
            m_merged_columns.back().m_type[op] = type;
        }
        else
        {
            it->m_max_schema_index = std::max(it->m_max_schema_index, schema_index);
            it->m_op_mask |= (uint32_t)1 << op;
            it->m_offset[op] = offset;
            it->m_schema_index[op] = schema_index;
            it->m_type[op] = type;
        }
    }

    void PackedTable::AddRow()
    {
        m_rows.emplace_back(std::make_unique<PackedRow>(m_rowSize));
        m_currentRow = m_rows.size() - 1;
    }

    void PackedTable::Validate(size_t col)
    {
        if (m_currentRow == static_cast<size_t>(-1))
            throw std::runtime_error("AddRow() must be called before SetValue()");
        if (col >= m_columns.size())
            throw std::out_of_range("Column out of range");
    }

    DbInstance* PackedTable::GetDbInstanceForRow(ProfileDatabase * db, int row_index)
    {
        auto columns = GetMergedColumns();
        uint8_t op = GetOperationValue(row_index);
        auto track_column_it = find_if(columns.begin(), columns.end(), [op](MergedColumnDef& cd) {return cd.m_schema_index[op] == Builder::SCHEMA_INDEX_TRACK_ID; });
        ROCPROFVIS_ASSERT_MSG_RETURN(track_column_it != columns.end(), ERROR_NODE_KEY_CANNOT_BE_NULL, nullptr);
        Numeric track = GetMergeTableValue(op, row_index, track_column_it - columns.begin(), db);
        ROCPROFVIS_ASSERT_MSG_RETURN(db->IsTrackIndexValid(track.data.u64), ERROR_NODE_KEY_CANNOT_BE_NULL, nullptr);
        return (DbInstance*)db->TrackPropertiesAt(track.data.u64)->db_instance;
    }

    void PackedTable::PlaceValue(size_t col, double value)
    {
        Validate(col);

        auto& c = m_columns[col];
        auto& r = m_rows[m_currentRow];

        switch (c.m_type)
        {
            case ColumnType::Byte:   r->Set<uint8_t>(c.m_offset, static_cast<uint8_t>(value)); break;
            case ColumnType::Word:   r->Set<uint16_t>(c.m_offset, static_cast<uint16_t>(value)); break;
            case ColumnType::Dword:  r->Set<uint32_t>(c.m_offset, static_cast<uint32_t>(value)); break;
            case ColumnType::Qword:  r->Set<uint64_t>(c.m_offset, static_cast<uint64_t>(value)); break;
            case ColumnType::Double: r->Set<double>(c.m_offset, static_cast<double>(value)); break;
        }
    }

    void PackedTable::PlaceValue(size_t col, uint64_t value)
    {
        Validate(col);

        auto& c = m_columns[col];
        auto& r = m_rows[m_currentRow];

        switch (c.m_type)
        {
            case ColumnType::Byte:   r->Set<uint8_t>(c.m_offset, static_cast<uint8_t>(value)); break;
            case ColumnType::Word:   r->Set<uint16_t>(c.m_offset, static_cast<uint16_t>(value)); break;
            case ColumnType::Dword:  r->Set<uint32_t>(c.m_offset, static_cast<uint32_t>(value)); break;
            case ColumnType::Qword:  r->Set<uint64_t>(c.m_offset, static_cast<uint64_t>(value)); break;
            case ColumnType::Double: r->Set<double>(c.m_offset, static_cast<double>(value)); break;
        }
    }

    uint8_t PackedTable::GetOperationValue(size_t row) const
    {
        if (row >= m_rows.size())
            throw std::out_of_range("Row out of range");
        return  m_rows[row]->Get<uint8_t>(0);
    }

    Numeric PackedTable::GetMergeTableValue(uint8_t op, size_t row, size_t col, ProfileDatabase* requestor) const
    {
        if (row >= m_rows.size() || col >= m_merged_columns.size())
            throw std::out_of_range("Row/Column out of range");

        const auto& r = m_rows[row];
        const auto& c = m_merged_columns[col];
        Numeric val;
        val.data.u64 = 0;

        switch (c.m_type[op])
        {
            case ColumnType::Byte:   val.data.u64 = r->Get<uint8_t>(c.m_offset[op]);break;
            case ColumnType::Word:   val.data.u64 = r->Get<uint16_t>(c.m_offset[op]);break;
            case ColumnType::Dword:  val.data.u64 = r->Get<uint32_t>(c.m_offset[op]);break;
            case ColumnType::Qword:  val.data.u64 = r->Get<uint64_t>(c.m_offset[op]);break;
            case ColumnType::Double: val.data.d = r->Get<double>(c.m_offset[op]); break;
            case ColumnType::Null:   val.data.u64 = static_cast<uint64_t>(0);break;
            default: throw std::runtime_error("Unknown column type");
        }

        return val;
    }


    const char* PackedTable::ConvertSqlStringReference(ProfileDatabase* db, uint32_t column_index, uint64_t  value, uint32_t node_id, bool & numeric_string) {
        numeric_string = false;
        uint64_t string_index = 0;
        if (column_index == Builder::SCHEMA_INDEX_NODE_ID)
        {
            numeric_string = true;
            return db->CachedTables(node_id)->GetTableCellByIndex("Node", value, "id");
        } else
        if (column_index == Builder::SCHEMA_INDEX_CATEGORY || column_index == Builder::SCHEMA_INDEX_CATEGORY_RPD || 
            column_index == Builder::SCHEMA_INDEX_EVENT_NAME || column_index == Builder::SCHEMA_INDEX_EVENT_NAME_RPD)
        {
            if (kRocProfVisDmResultSuccess == db->RemapStringId(value, rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory, node_id, string_index))
            {
                return db->BindObject()->FuncGetString(db->BindObject()->trace_object, string_index);
            }
        } else
        if (column_index == Builder::SCHEMA_INDEX_COUNTER_ID_RPD)
        {
            return db->StringTableReference().ToString(value);
        } else
        if (column_index == Builder::SCHEMA_INDEX_STREAM_NAME)
        {
            return db->CachedTables(node_id)->GetTableCell("Stream", value, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_QUEUE_NAME)
        {
            return db->CachedTables(node_id)->GetTableCell("Queue", value, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_EVENT_SYMBOL)
        {
            if (kRocProfVisDmResultSuccess == db->RemapStringId(value, rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol, node_id, string_index))
            {
                return db->BindObject()->FuncGetString(db->BindObject()->trace_object, string_index);
            }
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_ABS_INDEX || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_ABS_INDEX )
        {
            numeric_string = true;
            return db->CachedTables(node_id)->GetTableCell("Agent", value, "absolute_index");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_TYPE || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_TYPE)
        {
            return db->CachedTables(node_id)->GetTableCell("Agent", value, "type");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_TYPE_INDEX || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_TYPE_INDEX)
        {
            numeric_string = true;
            return db->CachedTables(node_id)->GetTableCell("Agent", value, "type_index");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_NAME || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_NAME)
        {
            return db->CachedTables(node_id)->GetTableCell("Agent", value, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_COUNTER_ID)
        {
            return db->CachedTables(node_id)->GetTableCell("PMC", value, "symbol");
        } else 
        if (column_index == Builder::SCHEMA_INDEX_MEM_TYPE)
        {
            return Builder::IntToTypeEnum(value,Builder::mem_alloc_types);
        }else
        if (column_index == Builder::SCHEMA_INDEX_LEVEL)
        {
            return Builder::IntToTypeEnum(value,Builder::mem_alloc_types);
        }
        return nullptr;
    }


    bool PackedTable::SetupAggregation(std::string agg_spec, int num_threads)
    {       
        auto agg_params = FilterExpression::ParseAggregationSpec(agg_spec);
        if (agg_params.size() > 0)
        {
            std::unordered_map<std::string, MergedColumnDef> column_def;
            for (auto it = agg_params.begin(); it != agg_params.end(); ) 
            {
                auto column_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [it](MergedColumnDef& cdef) { return cdef.m_name == it->column; });
                if (column_it != m_merged_columns.end())
                {
                    column_def[it->column] = *column_it;
                    ++it;
                }
                else 
                if (it->command == FilterExpression::SqlCommand::Count)
                {
                    ++it;
                } else
                {
                    it = agg_params.erase(it);
                }
            }
            if (agg_params.size() == 0) return false;

            auto column_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [](MergedColumnDef& cdef) { return cdef.m_name == Builder::TRACK_ID_PUBLIC_NAME; });
            if (column_it != m_merged_columns.end())
            {
                column_def[Builder::TRACK_ID_PUBLIC_NAME] = *column_it;
            }
            m_aggregation.agg_params = agg_params;
            m_aggregation.column_def = column_def;
            if (m_aggregation.agg_params[0].command != FilterExpression::SqlCommand::Column) return false;

            m_aggregation.aggregation_maps.resize(num_threads);
            return true;

        }
        return false;
    }

    void PackedTable::FinalizeAggregation()
    {
        std::map<std::string, ColumnAggr> result;
        for (auto & map : m_aggregation.aggregation_maps)
        {
            for (auto [key, val] : map)
            {
                auto it = result.find(val.name);
                if (it == result.end())
                {
                    result.insert({ val.name, {} });
                    it = result.find(val.name);
                    it->second.count = val.count;
                    for (auto param : m_aggregation.agg_params)
                    {
                        auto it_src = val.result.find(param.public_name);
                        if (it_src != val.result.end())
                        {
                            it->second.result[param.public_name].numeric.data.u64 = it_src->second.numeric.data.u64;
                            it->second.result[param.public_name].type = it_src->second.type;
                        }
                    }
                } else
                { 
                    it->second.count+=val.count;
                    for (auto param : m_aggregation.agg_params)
                    {
                        auto it_dst = it->second.result.find(param.public_name);
                        auto it_src = val.result.find(param.public_name);
                        if (it_dst != it->second.result.end() && it_src != val.result.end())
                        {
                            it_dst->second.type = it_src->second.type;
                            switch (param.command)
                            {
                            case FilterExpression::SqlCommand::Count:
                                it_dst->second.numeric.data.u64 += it_src->second.numeric.data.u64;
                                break;
                            case FilterExpression::SqlCommand::Avg:
                                it_dst->second.numeric.data.d += (it_src->second.numeric.data.d - it_dst->second.numeric.data.d) / it->second.count;
                                break;
                            case FilterExpression::SqlCommand::Min:
                                it_dst->second.numeric.data.d = std::min(it_src->second.numeric.data.d, it_dst->second.numeric.data.d);
                                break;
                            case FilterExpression::SqlCommand::Max:
                                it_dst->second.numeric.data.d = std::max(it_src->second.numeric.data.d, it_dst->second.numeric.data.d);
                                break;
                            case FilterExpression::SqlCommand::Sum:
                                it_dst->second.numeric.data.d += it_src->second.numeric.data.d;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
            map.clear();
        }
        std::vector<std::pair<std::string, ColumnAggr>> items(result.begin(), result.end());
        m_aggregation.result.clear();
        m_aggregation.result = std::move(items);
        m_aggregation.sort_order.clear();
        m_aggregation.sort_order.resize(m_aggregation.result.size());
        std::iota(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(), 0);
    }

    void
    PackedTable::ClearAggregation()
    {
        m_aggregation.column_def.clear();
        m_aggregation.agg_params.clear();
        m_aggregation.aggregation_maps.clear();
        m_aggregation.result.clear();
        m_aggregation.sort_order.clear();
        m_aggregation.m_string_data.Clear();
    }

    void PackedTable::SortAggregationByColumn(ProfileDatabase* db, std::string sort_column, bool sort_order) {


        if (m_aggregation.agg_params[0].public_name == sort_column)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ?
                        std::strcmp(m_aggregation.result[so1].first.c_str(), m_aggregation.result[so2].first.c_str()) < 0 :
                        std::strcmp(m_aggregation.result[so2].first.c_str(), m_aggregation.result[so1].first.c_str()) < 0;
                });
        }
        else
        {

            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    NumericWithType nval1 = m_aggregation.result[so1].second.result[sort_column];
                    NumericWithType nval2 = m_aggregation.result[so2].second.result[sort_column];
                    double val1 = nval1.type == NumericDouble ? nval1.numeric.data.d : nval1.numeric.data.u64;
                    double val2 = nval2.type == NumericDouble ? nval2.numeric.data.d : nval2.numeric.data.u64;
                    return sort_order ? val1 < val2 : val2 < val1;
                });

        }
        
    }

    void PackedTable::AggregateRow(ProfileDatabase* db, int row_index, int map_index)
    {
        std::unique_ptr<PackedRow>& r = m_rows[row_index];
        uint8_t op = r->Get<uint8_t>(0);
        std::string group_by = m_aggregation.agg_params[0].column;
        MergedColumnDef& group_by_column_info = m_aggregation.column_def[group_by];
        MergedColumnDef& track_column_info = m_aggregation.column_def[Builder::TRACK_ID_PUBLIC_NAME];
        uint8_t size = ColumnTypeSize(Builder::TRACK_ID_TYPE);
        uint32_t track = r->Get<uint64_t>(track_column_info.m_offset[op], size);
        DbInstance* db_instance = (DbInstance*)db->TrackPropertiesAt(track)->db_instance;
        ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, );
        size = ColumnTypeSize(group_by_column_info.m_type[op]);
        double value = 0;
        if (size > 0)
        {
            value = group_by_column_info.m_type[op] == ColumnType::Double ?  r->Get<double>(group_by_column_info.m_offset[op]) :  r->Get<uint64_t>(group_by_column_info.m_offset[op], size);
            
        }
        auto it = m_aggregation.aggregation_maps[map_index].find(value);
        if (it == m_aggregation.aggregation_maps[map_index].end())
        {
            m_aggregation.aggregation_maps[map_index].insert({ value, {} });
            it = m_aggregation.aggregation_maps[map_index].find(value);
            bool numeric_string = false;
            const char* str =  PackedTable::ConvertSqlStringReference(db, group_by_column_info.m_schema_index[op], value, db_instance->GuidIndex(), numeric_string);
            if (str == nullptr){
                if (group_by_column_info.m_type[op] == ColumnType::Double)
                {
                    it->second.name = std::to_string(value);
                }
                else
                {
                    it->second.name = std::to_string((uint64_t)value);
                }
            }
            else
            {
                it->second.name = str;
            }
            it->second.count = 0;
            for (auto param : m_aggregation.agg_params)
            {
                if (param.command == FilterExpression::SqlCommand::Column)
                {
                    if (group_by == param.column) continue;
                    std::string column = param.column;
                    MergedColumnDef& column_info = m_aggregation.column_def[column];
                    uint8_t size = ColumnTypeSize(column_info.m_type[op]);
                    if (size > 0)
                    {
                        value = column_info.m_type[op] == ColumnType::Double ?  r->Get<double>(column_info.m_offset[op]) :  r->Get<uint64_t>(column_info.m_offset[op], size);
                    }
                    bool numeric_string = false;
                    const char* str =  PackedTable::ConvertSqlStringReference(db, column_info.m_schema_index[op], value, db_instance->GuidIndex(), numeric_string);
                    if (str == nullptr){
                        if (column_info.m_type[op] == ColumnType::Double)
                        {
                            it->second.result[param.public_name].numeric.data.d = value;
                            it->second.result[param.public_name].type = NumericDouble;
                        }
                        else
                        {
                            it->second.result[param.public_name].numeric.data.u64 = value;
                            it->second.result[param.public_name].type = NumericUInt64;
                        }
                    }
                    else
                    {
                        it->second.result[param.public_name].type = NotNumeric;
                        it->second.result[param.public_name].numeric.data.u64 = m_aggregation.m_string_data.ToInt(str);
                    }
                }
                else
                {
                    switch (param.command)
                    {
                    case FilterExpression::SqlCommand::Count:
                        it->second.result[param.public_name].numeric.data.u64 = 0;
                        it->second.result[param.public_name].type = NumericUInt64;
                        break;
                    case FilterExpression::SqlCommand::Avg:
                        it->second.result[param.public_name].numeric.data.d = 0;
                        it->second.result[param.public_name].type = NumericDouble;
                        break;
                    case FilterExpression::SqlCommand::Min:
                        it->second.result[param.public_name].numeric.data.d = DBL_MAX;
                        it->second.result[param.public_name].type = NumericDouble;
                        break;
                    case FilterExpression::SqlCommand::Max:
                        it->second.result[param.public_name].numeric.data.d = DBL_MIN;
                        it->second.result[param.public_name].type = NumericDouble;
                        break;
                    case FilterExpression::SqlCommand::Sum:
                        it->second.result[param.public_name].numeric.data.d = 0;
                        it->second.result[param.public_name].type = NumericDouble;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        it->second.count++;
        for (auto param : m_aggregation.agg_params)
        {
            if (param.command == FilterExpression::SqlCommand::Column) continue;
            auto it_val = it->second.result.find(param.public_name);
            if (it_val != it->second.result.end())
            { 
                if (param.command == FilterExpression::SqlCommand::Count)
                {
                    it_val->second.numeric.data.u64++;
                }
                else
                {
                    std::string column = param.column;
                    MergedColumnDef& column_info = m_aggregation.column_def[column];
                    uint8_t size = ColumnTypeSize(column_info.m_type[op]);
                    double value = 0;
                    if (size > 0)
                    {
                        value = column_info.m_type[op] == ColumnType::Double ? r->Get<double>(column_info.m_offset[op]) : r->Get<uint64_t>(column_info.m_offset[op], size);
                    }
                    switch (param.command)
                    {
                    case FilterExpression::SqlCommand::Avg:
                        it_val->second.numeric.data.d += (value - it_val->second.numeric.data.d) / it->second.count;
                        break;
                    case FilterExpression::SqlCommand::Min:
                        it_val->second.numeric.data.d = std::min(value, it_val->second.numeric.data.d);
                        break;
                    case FilterExpression::SqlCommand::Max:
                        it_val->second.numeric.data.d = std::max(value, it_val->second.numeric.data.d);
                        break;
                    case FilterExpression::SqlCommand::Sum:
                        it_val->second.numeric.data.d += value;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }

    void PackedTable::SortByColumn(ProfileDatabase* db, std::string column, bool ascending)
    {
        auto it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [column](MergedColumnDef& cdef) { return cdef.m_name == column; });
        if (it != m_merged_columns.end())
        {
            if (it->m_name == Builder::COUNTER_VALUE_PUBLIC_NAME)
            {
                std::vector<double> sort_values;
                sort_values.reserve(m_rows.size());
                for (std::unique_ptr<PackedRow> & row : m_rows) {
                    uint8_t op = row->Get<uint8_t>(0);
                    uint8_t size = ColumnTypeSize(it->m_type[op]);
                    double value = size > 0 ? row->Get<double>(it->m_offset[op], size) : (ascending ? DBL_MAX : 0);
                    sort_values.push_back(value);
                }
                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](uint32_t so1, uint32_t so2) {
                        return ascending ? sort_values[so1] < sort_values[so2]
                            : sort_values[so2] < sort_values[so1];
                    });
            }
            else if (it->m_name == Builder::CATEGORY_PUBLIC_NAME || it->m_name == Builder::NAME_PUBLIC_NAME)
            {
                std::vector<uint64_t> sort_values;
                sort_values.reserve(m_rows.size());
                for (std::unique_ptr<PackedRow> & row : m_rows) {
                    uint8_t op = row->Get<uint8_t>(0);
                    MergedColumnDef& track_column_info = m_aggregation.column_def[Builder::TRACK_ID_PUBLIC_NAME];
                    uint8_t size = ColumnTypeSize(Builder::TRACK_ID_TYPE);
                    uint32_t track = row->Get<uint64_t>(track_column_info.m_offset[op], size);
                    DbInstance* db_instance = (DbInstance*)db->TrackPropertiesAt(track);
                    ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, );
                    size = ColumnTypeSize(it->m_type[op]);
                    if (size > 0)
                    {
                        uint64_t value = row->Get<uint64_t>(it->m_offset[op], size);
                        uint64_t string_index = 0;
                        if (kRocProfVisDmResultSuccess == db->RemapStringId(value, rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory, db_instance->GuidIndex(), string_index))
                        {
                            value = db->BindObject()->FuncGetStringOrder(db->BindObject()->trace_object, string_index);
                        }
                        
                        sort_values.push_back(value);
                    } else
                        sort_values.push_back(0);
               
                }
                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](uint32_t so1, uint32_t so2) {
                        return ascending ? sort_values[so1] < sort_values[so2]
                            : sort_values[so2] < sort_values[so1];
                    });

            }
            else
            {
                std::vector<uint64_t> sort_values;
                sort_values.reserve(m_rows.size());
                for (std::unique_ptr<PackedRow> & row : m_rows) {
                    uint8_t op = row->Get<uint8_t>(0);
                    uint8_t size = ColumnTypeSize(it->m_type[op]);
                    uint64_t value = size > 0 ? row->Get<uint64_t>(it->m_offset[op], size) : (ascending ? UINT64_MAX : 0);
                    sort_values.push_back(value);

                }
                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](uint32_t so1, uint32_t so2) {
                        return ascending ? sort_values[so1] < sort_values[so2]
                            : sort_values[so2] < sort_values[so1];
                    });
            }

        }
    }


    void PackedTable::RemoveDuplicates()
    {
        struct SortKey {
            uint64_t key;
            size_t index;
        };

        std::vector<SortKey> keys;
        keys.reserve(m_rows.size());
        for (size_t i = 0; i < m_rows.size(); ++i) {
            auto& row = *m_rows[i];
            keys.push_back({ row.Get<uint64_t>(1), i });
        }

        std::sort(std::execution::par_unseq, keys.begin(), keys.end(),
            [](const SortKey& a, const SortKey& b) {
                return a.key < b.key;
            });

        auto new_end = std::unique(keys.begin(), keys.end(),
            [](const SortKey& a, const SortKey& b) {
                return a.key == b.key;
            });
        keys.erase(new_end, keys.end());

        std::vector<std::unique_ptr<PackedRow>> new_rows;
        new_rows.reserve(keys.size());
        for (auto& k : keys)
            new_rows.push_back(std::move(m_rows[k.index]));

        m_rows.swap(new_rows);
    }

    //void PackedTable::RemoveDuplicates()
    //{
    //    auto new_end = std::unique(m_rows.begin(), m_rows.end(),
    //        [](const std::unique_ptr<PackedRow>& r1, const std::unique_ptr<PackedRow>& r2) {
    //            rocprofvis_dm_event_id_t id1, id2;
    //            id1.bitfield.event_op = r1->Get<uint8_t>(0);
    //            id1.bitfield.event_id = r1->Get<uint32_t>(1);
    //            id2.bitfield.event_op = r2->Get<uint8_t>(0);
    //            id2.bitfield.event_id = r2->Get<uint32_t>(1);
    //            return id1.value == id2.value;
    //        });
    //    m_rows.erase(new_end, m_rows.end());
    //}

    void PackedTable::RemoveRowsForSetOfTracks(std::set<uint32_t> tracks, bool remove_all)
    {
        if (remove_all)
        {
            m_rows.clear();
        } else
        if (tracks.size() > 0 && m_merged_columns.size() > 0)
        {
            auto track_info_it = Builder::table_view_schema.find(Builder::TRACK_ID_PUBLIC_NAME);
            auto stream_track_info_it = Builder::table_view_schema.find(Builder::STREAM_TRACK_ID_PUBLIC_NAME);
            uint8_t track_id_size = ColumnTypeSize(track_info_it->second.type);
            uint8_t stream_track_id_size = ColumnTypeSize(stream_track_info_it->second.type);
            auto track_id_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [](MergedColumnDef col) {return col.m_name == Builder::TRACK_ID_PUBLIC_NAME; });
            auto stream_track_id_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [](MergedColumnDef col) {return col.m_name == Builder::STREAM_TRACK_ID_PUBLIC_NAME; });
            m_rows.erase(
                std::remove_if(m_rows.begin(), m_rows.end(),
                    [&](std::unique_ptr<PackedRow>& row) {
                        uint8_t op = row->Get<uint8_t>(0);
                        uint16_t track = row->Get<uint16_t>(track_id_it->m_offset[op], track_id_size);
                        if (tracks.find(track) != tracks.end())
                            return true;
                        if (stream_track_id_it != m_merged_columns.end())
                        {
                            uint16_t stream_track = row->Get<uint16_t>(stream_track_id_it->m_offset[op], stream_track_id_size);
                            if (tracks.find(stream_track) != tracks.end())
                            {
                                return true;
                            }
                        }
                        return false;
                    }
                ),
                m_rows.end()
            );            
        }
    }

    void PackedTable::CreateSortOrderArray()
    {
        m_sort_order.clear();
        m_sort_order.resize(m_rows.size());
        std::iota(m_sort_order.begin(), m_sort_order.end(), 0);
    }

    void PackedTable::ManageColumns(std::vector<std::unique_ptr<PackedTable>>& tables)
    {
        uint32_t op_mask = 0;
        std::unordered_set<uint8_t> op_set;
        for (std::unique_ptr<PackedRow>& row : m_rows)
        {
            uint8_t op = row->Get<uint8_t>(0);
            op_set.insert(op);
            op_mask |= (uint32_t)1 << op;
        }


        m_merged_columns.erase(
            std::remove_if(m_merged_columns.begin(), m_merged_columns.end(),
                [&](MergedColumnDef& column) {
                    return (column.m_op_mask & op_mask) == 0;
                }
            ),
            m_merged_columns.end()
        );

        for (int i = 0; i < tables.size(); i++)
        {
            auto columns = tables[i]->GetColumns();
            if (tables[i]->m_rows.size() > 0)
            {
                uint8_t op = tables[i]->GetOperationValue(0);
                for (auto column : columns)
                {
                    AddMergedColumn(column.m_name, op, column.m_type, column.m_offset, column.m_schema_index);
                }
            }
        }

        std::sort(m_merged_columns.begin(), m_merged_columns.end(),
            [&](MergedColumnDef& a, MergedColumnDef& b) {

                return a.m_max_schema_index < b.m_max_schema_index;
            }
        );
    }

    void PackedTable::Merge(std::vector<std::unique_ptr<PackedTable>>& tables)
    {
        ManageColumns(tables);

        size_t total_row_count = 0;
        for (int j = 0; j < tables.size(); j++)
        {
            total_row_count += tables[j]->m_rows.size();
        }
        m_rows.reserve(total_row_count);
        for (int j = 0; j < tables.size(); j++)
        {
            for (auto& r : tables[j]->m_rows)
            {
                m_rows.push_back(std::move(r));
            }
            tables[j]->Clear();
        }

        tables.clear();

        if (m_rows.size() > 0)
        {
            uint8_t op = GetOperationValue(0);
            if (op > 0)
            {
                RemoveDuplicates();
            }
        }
    }


    uint32_t StringTable::ToInt(const char* str) {
        std::unique_lock lock(m_mutex);

        auto it = m_str_to_id.find(str);
        if (it != m_str_to_id.end())
            return it->second;

        uint32_t id = m_id_to_str.size();
        m_id_to_str.push_back(str);
        m_str_to_id[str] = id;
        return id;
    }

    const char* StringTable::ToString(uint32_t id) const {
        std::shared_lock lock(m_mutex);
        if (id >= m_id_to_str.size()) return "";
        return m_id_to_str[id].c_str();
    }

    void StringTable::Clear() {
        std::unique_lock lock(m_mutex);
        m_id_to_str.clear();
        m_str_to_id.clear();
    }

}  // namespace DataModel
}  // namespace RocProfVis
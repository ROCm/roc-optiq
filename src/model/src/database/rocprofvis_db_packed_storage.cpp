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

    void PackedTable::SortById()
    {
        std::sort(m_rows.begin(), m_rows.end(),
            [](const std::unique_ptr<PackedRow>& r1, const std::unique_ptr<PackedRow>& r2) {
                rocprofvis_dm_event_id_t id1, id2;
                id1.bitfield.event_op = r1->Get<uint8_t>(0);
                id1.bitfield.event_id = r1->Get<uint32_t>(1);
                id2.bitfield.event_op = r2->Get<uint8_t>(0);
                id2.bitfield.event_id = r2->Get<uint32_t>(1);
                return id1.value < id2.value;
            });
    }


    const char* PackedTable::ConvertTableIndexToString(ProfileDatabase* db, uint32_t column_index, uint64_t  index, bool & numeric_string) {
        numeric_string = false;
        if (column_index == Builder::SCHEMA_INDEX_NODE_ID)
        {
            numeric_string = true;
            return db->CachedTables()->GetTableCell("Node", index, "id");
        } else
        if (column_index == Builder::SCHEMA_INDEX_CATEGORY || column_index == Builder::SCHEMA_INDEX_CATEGORY_RPD)
        {
            if (column_index == Builder::SCHEMA_INDEX_CATEGORY_RPD)
            {
                index = db->RemapStringId(index);
            }
            return db->BindObject()->FuncGetString(db->BindObject()->trace_object, index);
        } else
        if (column_index == Builder::SCHEMA_INDEX_EVENT_NAME || column_index == Builder::SCHEMA_INDEX_EVENT_NAME_RPD)
        {
            if (column_index == Builder::SCHEMA_INDEX_EVENT_NAME_RPD)
            {
                index = db->RemapStringId(index);
            }
            return db->BindObject()->FuncGetString(db->BindObject()->trace_object, index);
        } else
        if (column_index == Builder::SCHEMA_INDEX_COUNTER_ID_RPD)
        {
            return db->StringTableReference().ToString(index);
        } else
        if (column_index == Builder::SCHEMA_INDEX_STREAM_NAME)
        {
            return db->CachedTables()->GetTableCell("Stream", index, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_QUEUE_NAME)
        {
            return db->CachedTables()->GetTableCell("Queue", index, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_EVENT_SYMBOL)
        {
            return db->BindObject()->FuncGetString(db->BindObject()->trace_object, index+db->SymbolsOffset());
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_ABS_INDEX || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_ABS_INDEX )
        {
            return db->CachedTables()->GetTableCell("Agent", index, "absolute_index");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_TYPE || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_TYPE)
        {
            return db->CachedTables()->GetTableCell("Agent", index, "type");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_TYPE_INDEX || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_TYPE_INDEX)
        {
            return db->CachedTables()->GetTableCell("Agent", index, "type_index");
        } else
        if (column_index == Builder::SCHEMA_INDEX_AGENT_NAME || column_index == Builder::SCHEMA_INDEX_AGENT_SRC_NAME)
        {
            return db->CachedTables()->GetTableCell("Agent", index, "name");
        } else
        if (column_index == Builder::SCHEMA_INDEX_COUNTER_ID)
        {
            return db->CachedTables()->GetTableCell("PMC", index, "symbol");
        } else 
        if (column_index == Builder::SCHEMA_INDEX_MEM_TYPE)
        {
            return Builder::IntToTypeEnum(index,Builder::mem_alloc_types);
        }else
        if (column_index == Builder::SCHEMA_INDEX_LEVEL)
        {
            return Builder::IntToTypeEnum(index,Builder::mem_alloc_types);
        }
        return nullptr;
    }


    bool PackedTable::SetupAggregation(std::string agg_spec, int num_threads)
    {
        m_aggregation.column_def.clear();
        m_aggregation.agg_params = FilterExpression::ParseAggregationSpec(agg_spec);
        if (m_aggregation.agg_params.size() > 0)
        {
            for (auto it = m_aggregation.agg_params.begin(); it != m_aggregation.agg_params.end(); ) 
            {
                auto column_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [it](MergedColumnDef& cdef) { return cdef.m_name == it->column; });
                if (column_it != m_merged_columns.end())
                {
                    m_aggregation.column_def[it->column] = *column_it;
                    ++it;
                }
                else 
                if (it->command == FilterExpression::SqlCommand::Count)
                {
                    ++it;
                } else
                {
                    it = m_aggregation.agg_params.erase(it);
                }
            }
            if (m_aggregation.agg_params.size() == 0) return false;
            if (m_aggregation.agg_params[0].command != FilterExpression::SqlCommand::Group) return false;
            for (int i=1; i < m_aggregation.agg_params.size();i++)
                if (m_aggregation.agg_params[i].command == FilterExpression::SqlCommand::Group) return false;

            m_aggregation.aggregation_maps.resize(num_threads);
            return true;

        }
        return false;
    }

    void PackedTable::FinalizeAggregation()
    {
        std::map<double, ColumnAggr> result;
        for (auto & map : m_aggregation.aggregation_maps)
        {
            for (auto [key, val] : map)
            {
                auto it = result.find(key);
                if (it == result.end())
                {
                    result.insert({ key, {} });
                    it = result.find(key);
                    it->second.name = val.name;
                    it->second.count = val.count;
                    for (auto param : m_aggregation.agg_params)
                    {
                        it->second.result[param.public_name].numeric.data.u64 = val.result[param.public_name].numeric.data.u64;
                        it->second.result[param.public_name].type = val.result[param.public_name].type;
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
        std::vector<std::pair<double, ColumnAggr>> items(result.begin(), result.end());
        m_aggregation.result.clear();
        m_aggregation.result = std::move(items);
        m_aggregation.sort_order.clear();
        m_aggregation.sort_order.resize(m_aggregation.result.size());
        std::iota(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(), 0);
    }

    void PackedTable::SortAggregationByColumn(ProfileDatabase* db, std::string sort_column, bool sort_order) {


        if (m_aggregation.agg_params[0].public_name == sort_column)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ?
                        std::strcmp(m_aggregation.result[so1].second.name.c_str(), m_aggregation.result[so2].second.name.c_str()) < 0 :
                        std::strcmp(m_aggregation.result[so2].second.name.c_str(), m_aggregation.result[so1].second.name.c_str()) < 0;
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
        uint8_t size = ColumnTypeSize(group_by_column_info.m_type[op]);
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
            const char* str =  PackedTable::ConvertTableIndexToString(db, group_by_column_info.m_schema_index[op], value, numeric_string);
            if (str == nullptr){
                it->second.name = std::to_string(group_by_column_info.m_type[op] == ColumnType::Double ? value : (uint64_t)value);
            }
            else
            {
                it->second.name = str;
            }
            it->second.count = 0;
            for (auto param : m_aggregation.agg_params)
            {
                if (param.command == FilterExpression::SqlCommand::Group) continue;
                switch (param.command)
                {
                case FilterExpression::SqlCommand::Count:
                    it->second.result[param.public_name].numeric.data.d = 0;
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
        it->second.count++;
        for (auto param : m_aggregation.agg_params)
        {
            if (param.command == FilterExpression::SqlCommand::Group) continue;
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
                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](const uint32_t & so1, const uint32_t & so2) {
                        std::unique_ptr<PackedRow>& r1 = m_rows[so1];
                        std::unique_ptr<PackedRow>& r2 = m_rows[so2];
                        uint8_t op1 = r1->Get<uint8_t>(0);
                        uint8_t op2 = r2->Get<uint8_t>(0);
                        uint8_t size1 = ColumnTypeSize(it->m_type[op1]);
                        uint8_t size2 = ColumnTypeSize(it->m_type[op2]);
                        double value1 = size1 > 0 ? r1->Get<double>(it->m_offset[op1], size1) : (ascending ? DBL_MAX : DBL_MIN);
                        double value2 = size2 > 0 ? r2->Get<double>(it->m_offset[op2], size2) : (ascending ? DBL_MAX : DBL_MIN); 
                        return ascending ? value1 < value2 : value2 < value1;
                    });
                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](const uint32_t & so1, const uint32_t & so2) {
                        std::unique_ptr<PackedRow>& r1 = m_rows[so1];
                        std::unique_ptr<PackedRow>& r2 = m_rows[so2];
                        uint8_t op1 = r1->Get<uint8_t>(0);
                        uint8_t op2 = r2->Get<uint8_t>(0);
                        uint8_t size1 = ColumnTypeSize(it->m_type[op1]);
                        uint8_t size2 = ColumnTypeSize(it->m_type[op2]);
                        double value1 = size1 > 0 ? r1->Get<double>(it->m_offset[op1], size1) : (ascending ? DBL_MAX : DBL_MIN);
                        double value2 = size2 > 0 ? r2->Get<double>(it->m_offset[op2], size2) : (ascending ? DBL_MAX : DBL_MIN); 
                        return ascending ? value1 < value2 : value2 < value1;
                    });
            }
            else if (it->m_name == Builder::CATEGORY_PUBLIC_NAME || it->m_name == Builder::NAME_PUBLIC_NAME)
            {

                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](const uint32_t & so1, const uint32_t & so2) {
                        std::unique_ptr<PackedRow>& r1 = m_rows[so1];
                        std::unique_ptr<PackedRow>& r2 = m_rows[so2];
                        uint8_t op1 = r1->Get<uint8_t>(0);
                        uint8_t op2 = r2->Get<uint8_t>(0);
                        uint8_t size1 = ColumnTypeSize(it->m_type[op1]);
                        uint8_t size2 = ColumnTypeSize(it->m_type[op2]);
                        uint64_t value1 = size1 > 0 ? r1->Get<uint64_t>(it->m_offset[op1], size1) : (ascending ? UINT64_MAX : 0);
                        uint64_t value2 = size2 > 0 ? r2->Get<uint64_t>(it->m_offset[op2], size2) : (ascending ? UINT64_MAX : 0);
                        if (value1 == value2)
                            return false;
                        if (size1 > 0)
                        {                           
                            uint64_t string_array_offset1 = it->m_schema_index[op1] == Builder::SCHEMA_INDEX_EVENT_SYMBOL ? db->SymbolsOffset() : 0;
                            value1 = db->BindObject()->FuncGetStringOrder(db->BindObject()->trace_object, value1 + string_array_offset1);
                        } 
                        if (size2 > 0)
                        {
                            uint64_t string_array_offset2 = it->m_schema_index[op2] == Builder::SCHEMA_INDEX_EVENT_SYMBOL ? db->SymbolsOffset() : 0;
                            value2 = db->BindObject()->FuncGetStringOrder(db->BindObject()->trace_object, value2 + string_array_offset2);
                        }
                        return ascending ? value1 < value2 : value2 < value1;
                    });
            }
            else
            {

                std::sort(std::execution::par_unseq, m_sort_order.begin(), m_sort_order.end(),
                    [&](const uint32_t & so1, const uint32_t & so2) {
                        std::unique_ptr<PackedRow>& r1 = m_rows[so1];
                        std::unique_ptr<PackedRow>& r2 = m_rows[so2];
                        uint8_t op1 = r1->Get<uint8_t>(0);
                        uint8_t op2 = r2->Get<uint8_t>(0);
                        uint8_t size1 = ColumnTypeSize(it->m_type[op1]);
                        uint8_t size2 = ColumnTypeSize(it->m_type[op2]);
                        uint64_t value1 = size1 > 0 ? r1->Get<uint64_t>(it->m_offset[op1], size1) : (ascending ? UINT64_MAX : 0);
                        uint64_t value2 = size2 > 0 ? r2->Get<uint64_t>(it->m_offset[op2], size2) : (ascending ? UINT64_MAX : 0);                  
                        return ascending ? value1 < value2 : value2 < value1;
                    });
            }
        }
    }

    void PackedTable::RemoveDuplicates()
    {
        auto new_end = std::unique(m_rows.begin(), m_rows.end(),
            [](const std::unique_ptr<PackedRow>& r1, const std::unique_ptr<PackedRow>& r2) {
                rocprofvis_dm_event_id_t id1, id2;
                id1.bitfield.event_op = r1->Get<uint8_t>(0);
                id1.bitfield.event_id = r1->Get<uint32_t>(1);
                id2.bitfield.event_op = r2->Get<uint8_t>(0);
                id2.bitfield.event_id = r2->Get<uint32_t>(1);
                return id1.value == id2.value;
            });
        m_rows.erase(new_end, m_rows.end());
    }

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

        uint8_t op = GetOperationValue(0);
        if (op > 0)
        {
            SortById();
            RemoveDuplicates();
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

}  // namespace DataModel
}  // namespace RocProfVis
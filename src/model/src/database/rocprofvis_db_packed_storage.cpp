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
            m_merged_columns.push_back({ name });
        }
        m_merged_columns.back().m_offset[op] = offset;
        m_merged_columns.back().m_schema_index[op] = schema_index;
        m_merged_columns.back().m_type[op] = type;
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

    bool PackedTable::SetupAggregation(std::string column, int num_threads)
    {
        auto column_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [column](MergedColumnDef& cdef) { return cdef.m_name == column; });
        auto duration_it = std::find_if(m_merged_columns.begin(), m_merged_columns.end(), [](MergedColumnDef& cdef) { return cdef.m_name == Builder::DURATION_PUBLIC_NAME; });
        if (column_it != m_merged_columns.end() && duration_it != m_merged_columns.end())
        {
            m_aggregation.column_def = *column_it;
            m_aggregation.duration_def = *duration_it;
            m_aggregation.aggregation_maps.resize(num_threads);
            return true;
        }
        return false;
    }

    const char* PackedTable::ConvertTableIndexToString(ProfileDatabase* db, uint32_t column_index, uint64_t & index) {

        if (column_index == Builder::SCHEMA_INDEX_NODE_ID)
        {
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

    void PackedTable::FinalizeAggregation()
    {
        std::map<uint64_t, ColumnAggr> result;
        for (auto & map : m_aggregation.aggregation_maps)
        {
            for (auto [key, val] : map)
            {
                auto it = result.find(key);
                if (it != result.end())
                {

                    it->second.count+=val.count;
                    it->second.min_duration = std::min(it->second.min_duration, val.min_duration);
                    it->second.max_duration = std::max(it->second.max_duration, val.max_duration);
                    it->second.avg_duration += (val.avg_duration - it->second.avg_duration) / it->second.count;
                }
                else
                {
                    result[key] = {val.name, val.count, val.min_duration, val.max_duration, val.avg_duration };
                }
            }
            map.clear();
        }
        std::vector<std::pair<uint64_t, ColumnAggr>> items(result.begin(), result.end());
        m_aggregation.result.clear();
        m_aggregation.result = std::move(items);
        m_aggregation.sort_order.clear();
        m_aggregation.sort_order.resize(m_aggregation.result.size());
        std::iota(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(), 0);
    }

    void PackedTable::SortAggregationByColumn(ProfileDatabase* db, std::string sort_column, bool sort_order) {
        if (sort_column == COLUMN_NAME_COUNT)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ? 
                        m_aggregation.result[so1].second.count < m_aggregation.result[so2].second.count : 
                        m_aggregation.result[so2].second.count < m_aggregation.result[so1].second.count;
                });
        } else
        if (sort_column == COLUMN_NAME_MIN)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ? 
                        m_aggregation.result[so1].second.min_duration < m_aggregation.result[so2].second.min_duration : 
                        m_aggregation.result[so2].second.min_duration < m_aggregation.result[so1].second.min_duration;

                });
        } else
        if (sort_column == COLUMN_NAME_MAX)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ? 
                        m_aggregation.result[so1].second.max_duration < m_aggregation.result[so2].second.max_duration : 
                        m_aggregation.result[so2].second.max_duration < m_aggregation.result[so1].second.max_duration;
                });
        } else
        if (sort_column == COLUMN_NAME_AVG)
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ? 
                        m_aggregation.result[so1].second.avg_duration < m_aggregation.result[so2].second.avg_duration : 
                        m_aggregation.result[so2].second.avg_duration < m_aggregation.result[so1].second.avg_duration;
                });
        }
        else
        {
            std::sort(m_aggregation.sort_order.begin(), m_aggregation.sort_order.end(),
                [&](const uint32_t& so1, const uint32_t& so2) {
                    return sort_order ? 
                      std::strcmp(m_aggregation.result[so1].second.name.c_str(), m_aggregation.result[so2].second.name.c_str()) < 0 : 
                      std::strcmp(m_aggregation.result[so2].second.name.c_str(), m_aggregation.result[so1].second.name.c_str()) < 0;
                });
        }
    }

    void PackedTable::AggregateRow(ProfileDatabase* db, int row_index, int map_index)
    {
        std::unique_ptr<PackedRow>& r = m_rows[row_index];
        uint8_t op = r->Get<uint8_t>(0);
        uint8_t size = ColumnTypeSize(m_aggregation.column_def.m_type[op]);
        uint64_t value = size > 0 ? r->Get<uint64_t>(m_aggregation.column_def.m_offset[op], size) : 0;
        uint8_t duration_size = ColumnTypeSize(m_aggregation.duration_def.m_type[op]);
        uint64_t duration_value = duration_size > 0 ? r->Get<uint64_t>(m_aggregation.duration_def.m_offset[op], size) : 0;

        auto it = m_aggregation.aggregation_maps[map_index].find(value);
        if (it != m_aggregation.aggregation_maps[map_index].end())
        {

            it->second.count++;
            it->second.min_duration = std::min(it->second.min_duration, duration_value);
            it->second.max_duration = std::max(it->second.max_duration, duration_value);
            it->second.avg_duration += (static_cast<double>(duration_value) - it->second.avg_duration) / it->second.count;
        }
        else
        {
            std::string string_value;
            const char* str =  PackedTable::ConvertTableIndexToString(db, m_aggregation.column_def.m_schema_index[op], value);
            if (str == nullptr){
                string_value = std::to_string(value);
            }
            else
            {
                string_value = str;
            }
            m_aggregation.aggregation_maps[map_index][value] = {string_value, 1, duration_value, duration_value, static_cast<double>(duration_value)};
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

    void PackedTable::CreateSortOrderArray()
    {
        m_sort_order.clear();
        m_sort_order.resize(m_rows.size());
        std::iota(m_sort_order.begin(), m_sort_order.end(), 0);
    }

    void PackedTable::CreateMergedTable(std::vector<std::unique_ptr<PackedTable>>& tables, PackedTable & result)
    {
        std::vector<std::string> schema;
        result.Clear();

        for (int i = 0; i < Builder::table_view_schema.size(); i++)
        {
            auto column = Builder::FindColumnNameByIndex(Builder::table_view_schema, i);
            if (column != std::nullopt)
            {
                schema.push_back(*column);
            }
        }

        for (int i = 0; i < schema.size(); i++)
        {
            auto column_data = Builder::table_view_schema[schema[i]];
            for (int j = 0; j < tables.size(); j++)
            {
                auto columns = tables[j]->GetColumns();
                auto it = std::find_if(columns.begin(), columns.end(), [column_data](ColumnDef& c) { return c.m_schema_index == column_data.index; });
                if (it!=columns.end())
                {
                    uint8_t op = tables[j]->GetOperationValue(0);
                    result.AddMergedColumn(column_data.public_name, op, it->m_type, it->m_offset, it->m_schema_index);
                }
            }
        }
        size_t total_row_count = 0;
        for (int j = 0; j < tables.size(); j++)
        {
            total_row_count += tables[j]->m_rows.size();
        }
        result.m_rows.reserve(total_row_count);
        for (int j = 0; j < tables.size(); j++)
        {
            for (auto& r : tables[j]->m_rows)
            {
                result.m_rows.push_back(std::move(r));
            }
            tables[j]->Clear();
        }

        tables.clear();

        uint8_t op = result.GetOperationValue(0);
        if (op > 0)
        {
            result.SortById();
            result.RemoveDuplicates();
        }
        result.CreateSortOrderArray();
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
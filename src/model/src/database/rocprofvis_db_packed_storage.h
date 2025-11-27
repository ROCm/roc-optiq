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

#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <variant>
#include <stdexcept>
#include <cstring>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include "rocprofvis_common_types.h"
#include "rocprofvis_db_expression_filter.h"

namespace RocProfVis
{
namespace DataModel
{

    enum class ColumnType : uint8_t
    {
        Null,
        Byte,
        Word,
        Dword,
        Qword,
        Double
    };

    struct ColumnDef
    {
        std::string m_name;
        uint8_t m_orig_index;
        ColumnType m_type;
        uint8_t m_offset;        
        uint8_t m_schema_index;
    };


    struct MergedColumnDef
    {
        std::string m_name;
        uint32_t m_op_mask;
        uint8_t m_max_schema_index;

        ColumnType m_type[kRocProfVisDmNumOperation];
        uint8_t m_offset[kRocProfVisDmNumOperation];        
        uint8_t m_schema_index[kRocProfVisDmNumOperation];
    };

    typedef enum NumericType {
        NumericUInt64,
        NumericDouble,
        NotNumeric
    } NumericType;

    typedef struct Numeric {
        union {
            uint64_t u64;
            double d;
        } data;
    } Numeric;

    typedef struct NumericWithType {
        Numeric numeric;
        NumericType type;
    } NumericWithType;

    struct ColumnAggr
    {
        std::string name;
        uint32_t count;
        std::unordered_map<std::string, NumericWithType> result;
    };


    class StringTable 
    {
    public:        
        uint32_t ToInt(const char* s);
        const char* ToString(uint32_t id) const;

    private:
        mutable std::shared_mutex m_mutex;
        std::unordered_map<std::string, uint32_t> m_str_to_id;
        std::vector<rocprofvis_dm_string_t> m_id_to_str;
    };

    struct Aggregation
    {
        std::unordered_map<std::string, MergedColumnDef> column_def;
        std::vector<FilterExpression::SqlAggregation> agg_params;
        std::vector<std::unordered_map<double, ColumnAggr>> aggregation_maps;
        std::vector<std::pair<std::string, ColumnAggr>> result;
        std::vector<uint32_t> sort_order;
        StringTable m_string_data;
    };

    class ProfileDatabase;

    class PackedRow
    {
    public:
        PackedRow(size_t size = 0) : m_data(size) {}

        template<typename T>
        void Set(size_t offset, const T& value)
        {
            std::memcpy(m_data.data() + offset, &value, sizeof(T));
        }

        template<typename T>
        T Get(size_t offset) const
        {
            T value{};
            std::memcpy(&value, m_data.data() + offset, sizeof(T));
            return value;
        }

        template<typename T>
        T Get(size_t offset, size_t size) const
        {
            T value{};
            std::memcpy(&value, m_data.data() + offset, size);
            return value;
        }

        size_t Size() const { return m_data.size(); }
        uint8_t* Raw() { return m_data.data(); }
        const uint8_t* Raw() const { return m_data.data(); }

    private:
        std::vector<uint8_t> m_data;
    };

    class PackedTable
    {
    public:

        static constexpr const char* COLUMN_NAME_COUNT = "Count";
        static constexpr const char* COLUMN_NAME_MIN = "Min";
        static constexpr const char* COLUMN_NAME_MAX = "Max";
        static constexpr const char* COLUMN_NAME_AVG = "Avg";

        void AddColumn(const std::string& name, ColumnType type, uint8_t sql_index, uint8_t schema_index);
        void AddMergedColumn(const std::string& name, uint8_t op, ColumnType type, uint8_t offset, uint8_t schema_index);
        void AddRow();

        void Validate(size_t col);

        void PlaceValue(size_t col, double value);
        void PlaceValue(size_t col, uint64_t value);
        Numeric GetMergeTableValue(uint8_t op, size_t row, size_t col, ProfileDatabase* requestor) const;
        uint8_t GetOperationValue(size_t row) const;

        size_t ColumnCount() const { return m_columns.size(); }
        size_t MergedColumnCount() const { return m_merged_columns.size(); }
        size_t RowCount() const { return m_rows.size(); }
        size_t AggregationRowCount() const { return m_aggregation.result.size(); }
        uint8_t GetRowSize() { return m_rowSize; }
        std::pair<std::string, ColumnAggr>& GetAggreagationRow(int index) { return m_aggregation.result[m_aggregation.sort_order[index]]; };
        std::string GetAggregationStringByIndex(uint32_t index) { return m_aggregation.m_string_data.ToString(index); }
        const std::vector<ColumnDef>& GetColumns() const { return m_columns; }
        const std::vector<FilterExpression::SqlAggregation>& GetAggregationSpec() const { return m_aggregation.agg_params; }
        const std::vector<MergedColumnDef>& GetMergedColumns() const { return m_merged_columns; }
        uint32_t SortedIndex(uint32_t index) { return m_sort_order[index]; };

        void Merge(std::vector<std::unique_ptr<PackedTable>>& tables);
        void ManageColumns(std::vector<std::unique_ptr<PackedTable>>& tables);
       
        static uint8_t ColumnTypeSize(ColumnType type);
        void Clear() { m_columns.clear(); m_rows.clear(); m_merged_columns.clear(); m_rowSize = 0; m_currentRow = static_cast<size_t>(-1); };

        void RemoveDuplicates();
        void CreateSortOrderArray();
        void SortByColumn(ProfileDatabase * db, std::string column, bool ascending);
        bool SetupAggregation(std::string agg_spec, int num_threads);
        void FinalizeAggregation();
        void AggregateRow(ProfileDatabase * db, int row_index, int map_index);
        void SortAggregationByColumn(ProfileDatabase* db, std::string sort_column, bool sort_order);
        void RemoveRowsForSetOfTracks(std::set<uint32_t> tracks, bool remove_all);

        static const char* ConvertTableIndexToString(ProfileDatabase* db, uint32_t column_index, uint64_t index, bool & numeric_string);

    private:
        std::vector<ColumnDef> m_columns;
        std::vector<std::unique_ptr<PackedRow>> m_rows;
        std::vector<uint32_t> m_sort_order;
        mutable std::vector<MergedColumnDef> m_merged_columns;
        uint8_t m_rowSize = 0;
        size_t m_currentRow = static_cast<size_t>(-1); 
        Aggregation m_aggregation;
    public:
        int track_id;
        int stream_track_id;
    };




}  // namespace DataModel
}  // namespace RocProfVis
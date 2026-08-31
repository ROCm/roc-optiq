// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_c_interface.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Track;
class Future;
class Trace;

class SystemTable : public Table
{
public:
    SystemTable(uint64_t id);

    virtual ~SystemTable();

    void Reset() override;

    rocprofvis_result_t SetupAndFetch(Trace& controller, Arguments& args, Array& array, Future* future) final;
    rocprofvis_result_t ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future, const char* path) const final;


protected:
    struct SystemTableArguments : TableArguments
    {
        std::string m_where;
        std::string m_filter;
        std::string m_group;
        std::string m_group_cols;
        std::vector<uint32_t> m_tracks;
        rocprofvis_dm_table_use_case_enum_t m_use_case;
        double m_start_ts;
        double m_end_ts;
    };

    rocprofvis_result_t UnpackArguments(Arguments& args, TableArguments*& out) const override;
    void                GetCurrentArguments(TableArguments*& out) const override;
    void                SetCurrentArguments(TableArguments& in) override;

    virtual bool                   ArgumentsChanged(SystemTableArguments& in) const;
    virtual rocprofvis_result_t    UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const;
    virtual rocprofvis_dm_result_t BuildQuery(rocprofvis_dm_database_t db, TableArguments& args, uint64_t index, uint64_t count, bool count_only, char** out) const;

private:
    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future) final;
    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array, Future* future) final;
    rocprofvis_controller_primitive_type_t PrimitiveType(rocprofvis_db_data_type_t db_data_type) const;

    std::vector<uint32_t> m_tracks;
    rocprofvis_dm_table_use_case_enum_t m_use_case;
    double m_start_ts;
    double m_end_ts;
    std::string m_where;
    std::string m_filter;
    std::string m_group;
    std::string m_group_cols;
};

}
}

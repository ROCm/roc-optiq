// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_c_interface.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Arguments;

class EventSearchTable : public SystemTable
{
public:
    EventSearchTable(uint64_t id);

    virtual ~EventSearchTable();

    void Reset() override;

protected:
    struct EventSearchTableArguments : SystemTableArguments
    {       
        std::vector<std::string> m_string_table_filters;
        bool                     m_include_substrings;
        bool                     m_include_category;
        bool                     m_partial_matching;
    };

    rocprofvis_result_t UnpackArguments(Arguments& args, TableArguments*& out) const override;
    void                GetCurrentArguments(TableArguments*& out) const override;
    void                SetCurrentArguments(TableArguments& in) override;

    virtual bool                   ArgumentsChanged(SystemTableArguments& in) const override;
    virtual rocprofvis_result_t    UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const override;
    virtual rocprofvis_dm_result_t BuildQuery(rocprofvis_dm_database_t db, TableArguments& args, uint64_t index, uint64_t count, bool count_only, char** out) const override;

private:
    std::vector<std::string> m_string_table_filters;
    bool                     m_include_substrings;
    bool                     m_include_category;
    bool                     m_partial_matching;

};

}
}

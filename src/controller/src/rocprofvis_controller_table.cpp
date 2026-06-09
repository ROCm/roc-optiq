// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_trace.h"

namespace RocProfVis
{
namespace Controller
{

Table::Table(uint64_t id, uint32_t first_prop_index, uint32_t last_prop_index)
: Handle(first_prop_index, last_prop_index)
, m_num_items(0)
, m_id(id)
, m_sort_column(0)
, m_sort_order(kRPVControllerSortOrderAscending)
{}

Table::~Table() {}

rocprofvis_controller_object_type_t Table::GetType(void)
{
	return kRPVControllerObjectTypeTable;
}

rocprofvis_result_t
Table::SetupAndFetch(Trace& controller, Arguments& args, Array& array, Future* future)
{
    rocprofvis_result_t result = Setup(controller.GetDMHandle(), args, future);
    if(result == kRocProfVisResultSuccess)
    {
        uint64_t start_index = 0;
        uint64_t start_count = 0;
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsStartIndex, 0, &start_index);
        }
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsStartCount, 0, &start_count);
        }
        result = Fetch(controller.GetDMHandle(), start_index, start_count, array, future);
    }
    return result;
}

}
}

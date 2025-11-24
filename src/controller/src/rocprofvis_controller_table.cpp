// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table.h"

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

}
}

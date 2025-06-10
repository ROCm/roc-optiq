// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_table.h"

namespace RocProfVis
{
namespace Controller
{

Table::Table(uint64_t id)
: m_num_items(0)
, m_id(id)
, m_sort_column(0)
, m_sort_order(kRPVControllerSortOrderAscending)
, m_track_type(kRPVControllerTrackTypeEvents)
{
}

Table::~Table()
{
}

rocprofvis_controller_object_type_t Table::GetType(void)
{
	return kRPVControllerObjectTypeTable;
}

}
}

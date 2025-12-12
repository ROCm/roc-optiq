// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace View
{
	constexpr char* COMPUTE_SUMMARY_VIEW_URL = "summary";
	constexpr char* COMPUTE_ROOFLINE_VIEW_URL = "roofline";
	constexpr char* COMPUTE_BLOCK_VIEW_URL = "block";
	constexpr char* COMPUTE_TABLE_VIEW_URL = "table";
	constexpr char* COMPUTE_TABLE_SPEED_OF_LIGHT_URL = "speed_of_light";
	constexpr char* COMPUTE_TABLE_COMMAND_PROCESSOR_URL = "command_processor";
	constexpr char* COMPUTE_TABLE_WORKGROUP_MANAGER_URL = "workgroup_manager";
	constexpr char* COMPUTE_TABLE_WAVEFRONT_URL = "wavefront";
	constexpr char* COMPUTE_TABLE_INSTRUCTION_MIX_URL = "instruction_mix";	
	constexpr char* COMPUTE_TABLE_COMPUTE_PIPELINE_URL = "compute_pipeline";
	constexpr char* COMPUTE_TABLE_LOCAL_DATA_STORE_URL = "local_data_store";
	constexpr char* COMPUTE_TABLE_INSTRUCTION_CACHE_URL = "instruction_cache";
	constexpr char* COMPUTE_TABLE_SCALAR_CACHE_URL = "scaler_cache";
	constexpr char* COMPUTE_TABLE_ADDRESS_PROCESSING_UNIT_URL = "address_processing_unit";
	constexpr char* COMPUTE_TABLE_VECTOR_CACHE_URL = "vector_cache";
	constexpr char* COMPUTE_TABLE_L2_CACHE_URL = "l2_cache";
	constexpr char* COMPUTE_TABLE_L2_CACHE_PER_CHANNEL_URL = "l2_cache_per_channel";
}  // namespace View
}  // namespace RocProfVis

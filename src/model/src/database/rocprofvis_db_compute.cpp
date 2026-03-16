// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_compute.h"

namespace RocProfVis
{
namespace DataModel
{

	static const std::unordered_map<rocprofvis_db_compute_use_case_enum_t, std::string> SupportedUseCases{
		{kRPVComputeFetchListOfWorkloads, "Fetch list of workloads" },
		{kRPVComputeFetchWorkloadRooflineCeiling,  "Fetch roofline ceilings of a workload" },
		{kRPVComputeFetchWorkloadTopKernels, "Fetch top kernels by duration of a workload" },
		{kRPVComputeFetchWorkloadKernelsList, "Fetch list of kernels in a workload" },
		{kRPVComputeFetchWorkloadMetricsDefinition, "Fetch list of metrics in a workload" },
		{kRPVComputeFetchKernelRooflineIntensities, "Fetch roofline intensities of a kernel" },
		{kRPVComputeFetchKernelMetricCategoriesList, "Fetch list of metric categories in a kernel" },
		{kRPVComputeFetchMetricCategoryTablesList, "Fetch list of tables in a category" },
		{kRPVComputeFetchMetricValues, "Fetch values of metrics"},
		{kRPVComputeFetchKernelMetricsMatrix, "Fetch kernel metrics matrix with pivoted metric columns"},
		{kRPVComputeFetchWorkloadMetricValueNames, "Fetch distinct value names per metric in a workload"}
	};

	static const std::unordered_map<std::string, rocprofvis_db_compute_column_enum_t> ColumnNameToEnum {
		{"workload_id", kRPVComputeColumnWorkloadId},
		{"workload_name", kRPVComputeColumnWorkloadName},
		{"workload_sub_name", kRPVComputeColumnWorkloadSubName},
		{"sys_info_extdata", kRPVComputeColumnWorkloadSysInfo},
		{"profiling_config_extdata", kRPVComputeColumnWorkloadProfileConfig},
		{"roofline_bench_extdata", kRPVComputeColumnWorkloadRooflineBenchBlob},
		{"kernel_uuid", kRPVComputeColumnKernelUUID},
		{"kernel_name", kRPVComputeColumnKernelName},
		{"dispatch_count", kRPVComputeColumnKernelsCount},
		{"duration_ns_sum", kRPVComputeColumnKernelDurationsSum},
		{"duration_ns_mean", kRPVComputeColumnKernelDurationsAvg},
		{"duration_ns_median", kRPVComputeColumnKernelDurationsMedian},
		{"duration_ns_min", kRPVComputeColumnKernelDurationsMin},
		{"duration_ns_max", kRPVComputeColumnKernelDurationsMax},
		{"total_flops", kRPVComputeColumnRooflineTotalFlops},
		{"l1_cache_data", kRPVComputeColumnRooflineL1CacheData},
		{"l2_cache_data", kRPVComputeColumnRooflineL2CacheData},
		{"hbm_cache_data", kRPVComputeColumnRooflineHBMCacheData},
		{"table_id", kRPVComputeColumnTableId},
		{"sub_table_id", kRPVComputeColumnSubTableId},
		{"table_name", kRPVComputeColumnMetricTableName},
		{"sub_table_name", kRPVComputeColumnMetricSubTableName},
		{"metric_id", kRPVComputeColumnMetricId},
		{"metric_name", kRPVComputeColumnMetricName},
		{"metric_description", kRPVComputeColumnMetricDescription},
		{"value_name", kRPVComputeColumnMetricValueName},
		{"value", kRPVComputeColumnMetricValue},
		{"unit", kRPVComputeColumnMetricUnit},
		{"__id", kRPVComputeColumnDynamicKernelUUID},
	};

	static const std::unordered_map<std::string, rocprofvis_db_compute_column_enum_t> RooflineBenchParamToEnum{
		{"HBMBw", kRPVComputeColumnWorkloadRooflineBenchHBMBw},
		{"L2Bw", kRPVComputeColumnWorkloadRooflineBenchL2Bw},
		{"L1Bw", kRPVComputeColumnWorkloadRooflineBenchL1Bw},
		{"LDSBw", kRPVComputeColumnWorkloadRooflineBenchLDSBw},
		{"MFMAF4Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF4Flops},
		{"MFMAF6Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF6Flops},
		{"MFMAF8Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF8Flops},
		{"FP16Flops", kRPVComputeColumnWorkloadRooflineBenchFP16Flops},
		{"MFMAF16Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF16Flops},
		{"MFMABF16Flops", kRPVComputeColumnWorkloadRooflineBenchMFMABF16Flops},
		{"FP32Flops", kRPVComputeColumnWorkloadRooflineBenchFP32Flops},
		{"MFMAF32Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF32Flops},
		{"FP64Flops", kRPVComputeColumnWorkloadRooflineBenchFP64Flops},
		{"MFMAF64Flops", kRPVComputeColumnWorkloadRooflineBenchMFMAF64Flops},
		{"I8Ops", kRPVComputeColumnWorkloadRooflineBenchI8Ops},
		{"MFMAI8Ops", kRPVComputeColumnWorkloadRooflineBenchMFMAI8Ops},
		{"I32Ops", kRPVComputeColumnWorkloadRooflineBenchI32Ops},
		{"I64Ops", kRPVComputeColumnWorkloadRooflineBenchI64Ops},
	};

	std::string ComputeQueryFactory::GetComputeListOfWorkloads(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query = "SELECT ";
		query += "workload_id, ";
		query += "name as workload_name, "; 
		query += "sub_name as workload_sub_name, ";
		query += "sys_info_extdata, ";
		query += "profiling_config_extdata "; 
		query += "FROM ";
		query += "compute_workload";
		return query;
	}
	std::string ComputeQueryFactory::GetComputeWorkloadRooflineCeiling(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamWorkloadId)
		{
			query = "SELECT roofline_bench_extdata FROM compute_workload WHERE workload_id = ";
			query += params[0].param_str;
		}
		return query;
	}
	std::string ComputeQueryFactory::GetComputeWorkloadTopKernels(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {

		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamWorkloadId)
		{
			query =
				"SELECT "
				"compute_kernel.kernel_uuid AS kernel_uuid,"
				"compute_kernel.workload_id AS workload_id,"
				"compute_workload.name AS workload_name,"
				"compute_kernel.kernel_name AS kernel_name,"
				"compute_dispatch.dispatch_id AS dispatch_id,"
				"(compute_dispatch.end_timestamp - compute_dispatch.start_timestamp) AS duration_ns "
				"FROM compute_dispatch "
				"JOIN compute_kernel "
				"ON compute_dispatch.kernel_uuid = compute_kernel.kernel_uuid "
				"JOIN compute_workload "
				"ON compute_kernel.workload_id = compute_workload.workload_id "
				"WHERE compute_workload.workload_id = ";
			query += params[0].param_str;
		}
		return query;
		
	}
	std::string ComputeQueryFactory::GetComputeWorkloadKernelsList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamWorkloadId)
		{
			query = "SELECT ";
			query += "kernel_uuid,  ";
			query += "workload_id,  ";
			query += "kernel_name  ";
			query += "FROM  ";
			query += "compute_kernel ";
			query += "WHERE workload_id = ";
			query += params[0].param_str;
		}
		return query;
	}

	std::string ComputeQueryFactory::GetComputeWorkloadMetricsDefinition(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamWorkloadId)
		{
			query = "SELECT ";
			query += "workload_id, ";
			query += "name as metric_name, ";
			query += "description as metric_description, ";
			query += "substr(metric_id, 0, instr(metric_id, '.')) as table_id, ";
			query += "metric_id as sub_table_id, "; //parsed in callback method
			query += "table_name, ";
			query += "sub_table_name, ";
			query += "unit ";
			query += "FROM compute_metric_definition ";
			query += "WHERE workload_id = ";
			query += params[0].param_str;
		}
		return query;
	}

	std::string ComputeQueryFactory::GetComputeWorkloadMetricValueNames(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 2 && params != nullptr &&
			params[0].param_type == kRPVComputeParamWorkloadId &&
			params[1].param_type == kRPVComputeParamMetricId)
		{
			query = "SELECT DISTINCT value_name FROM compute_metric_value WHERE metric_uuid IN (";
			std::string in_query;
			for (auto& [metric_id, metric_uuid] : m_db->m_metric_uuid_lookup[std::atol(params[0].param_str)])
			{
				if (metric_id.find(params[1].param_str) == 0)
				{
					if (!in_query.empty())
					{
						in_query += ",";
					}
					in_query += std::to_string(metric_uuid);
				}
			}
			query += in_query + ")";
		}
		return query;
	}

	std::string ComputeQueryFactory::GetComputeKernelRooflineIntensities(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamKernelId)
		{
			query = "WITH AVG_D AS (SELECT AVG(end_timestamp - start_timestamp), kernel_uuid FROM compute_dispatch WHERE kernel_uuid = ";
			query += params[0].param_str;
			query += " GROUP BY kernel_uuid)";
			query += " SELECT ";
			query += " K.kernel_uuid, ";
			query += " K.kernel_name, ";
			query += " total_flops, ";
			query += " l1_cache_data, ";
			query += " l2_cache_data, ";
			query += " hbm_cache_data ";
			query += " FROM compute_roofline_data CRD ";
			query += " INNER JOIN AVG_D ON CRD.kernel_uuid = AVG_D.kernel_uuid ";
			query += " INNER JOIN compute_kernel K ON AVG_D.kernel_uuid = K.kernel_uuid";
		}
		return query;
	}

	std::string ComputeQueryFactory::GetComputeKernelMetricCategoriesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 1 && params != nullptr && params[0].param_type == kRPVComputeParamKernelId)
		{
			query = "SELECT ";
			query += "substr(metric_id, 0, instr(metric_id, '.')) as table_id, ";
			query += "table_name ";
			query += "FROM compute_metric_view ";
			query += "WHERE kernel_uuid = ";
			query += params[0].param_str;
			query += " GROUP BY table_name";
		}
		return query;
	}

	std::string ComputeQueryFactory::GetComputeMetricCategoryTablesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		if (num == 2 && params != nullptr && params[0].param_type == kRPVComputeParamKernelId && params[1].param_type == kRPVComputeParamMetricId)
		{
			query = "SELECT ";
			query += "metric_id as sub_table_id, "; //parsed in callback method
			query += "sub_table_name ";
			query += "FROM compute_metric_view ";
			query += "WHERE kernel_uuid = ";
			query += params[0].param_str;
			query += " AND metric_id LIKE '";
			query += params[1].param_str;
			query += "%' GROUP BY table_name";
		}
		return query;
	}

	MetricIdFormat ComputeQueryFactory::ClassifyMetricIdFormat(const std::string& s) {
		size_t first = s.find('.');
		if (first == std::string::npos) return MetricIdFormat::Other;

		size_t second = s.find('.', first + 1);

		if (second == std::string::npos) {
			if (first > 0 && first < s.size() - 1)
				return MetricIdFormat::XY;
		} else {
			if (first > 0 &&
				second > first + 1 &&
				second < s.size() - 1 &&
				s.find('.', second + 1) == std::string::npos)
				return MetricIdFormat::XYZ;
		}

		return MetricIdFormat::Other;
	}

	std::string ComputeQueryFactory::SanitizeMetricValueName(const std::string& name) {
		std::string temp;
		temp.reserve(name.length() + 10);

		// Check if the name is purely numeric (handles 0-127 case)
		bool is_numeric = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
		if (is_numeric) {
			temp = "mv_" + name; // Prefix numeric names with 'mv_' for "metric value"
		} else {
			temp = name;
		}

		// Convert to lowercase and replace spaces/special chars with underscores
		std::string result;
		result.reserve(temp.length());
		for (char c : temp) {
			if (std::isalnum(c)) {
				result += std::tolower(c);
			} else {
				result += '_';
			}
		}

		return result;
	}


std::string ComputeQueryFactory::GetComputeKernelMetricsMatrix(
	rocprofvis_db_num_of_params_t num,
	rocprofvis_db_compute_params_t params)
{
	std::string workload_id;
	int         sort_column_index = 2;
	std::string sort_order        = "DESC";

	std::vector<std::pair<std::string,std::string>> metric_selectors;
	std::unordered_map<size_t,std::string> column_filters;

	size_t last_filter_column_index = 0;

	// -----------------------------
	// Parse parameters
	// -----------------------------
	for(size_t i = 0; i < num; i++)
	{
		if(params[i].param_type == kRPVComputeParamWorkloadId)
		{
			workload_id = params[i].param_str;
		}
		else if(params[i].param_type == kRPVComputeParamMetricSelector)
		{
			std::string selector = params[i].param_str;

			size_t colon_pos = selector.find(':');
			if(colon_pos != std::string::npos)
			{
				std::string metric_id  = selector.substr(0, colon_pos);
				std::string value_name = selector.substr(colon_pos + 1);

				std::transform(value_name.begin(), value_name.end(),
					value_name.begin(),
					[](unsigned char c){ return std::tolower(c); });

				metric_selectors.emplace_back(metric_id, value_name);
			}
		}
		else if(params[i].param_type == kRPVComputeParamSortColumnIndex)
		{
			sort_column_index = std::atoi(params[i].param_str);
		}
		else if(params[i].param_type == kRPVComputeParamSortColumnOrder)
		{
			std::string order = params[i].param_str;

			std::transform(order.begin(), order.end(),
				order.begin(),
				[](unsigned char c){ return std::toupper(c); });

			sort_order = (order == "ASC") ? "ASC" : "DESC";
		}
		else if(params[i].param_type == kRPVComputeParamFilterColumnIndex)
		{
			last_filter_column_index = std::stoull(params[i].param_str);
		}
		else if(params[i].param_type == kRPVComputeParamFilterExpression)
		{
			column_filters[last_filter_column_index] = params[i].param_str;
		}
	}

	if(workload_id.empty())
		return "";


	std::vector<std::pair<std::string, rocprofvis_db_data_type_t>> column_names = { 
		{"__id",kRPVDataTypeInt},
		{"kernel_name",kRPVDataTypeString},
		{"dispatch_count",kRPVDataTypeInt},
		{"duration_ns_sum",kRPVDataTypeInt}};


	for(const auto& [metric_id,value_name] : metric_selectors)
	{
		std::string metric_alias = metric_id;
		std::replace(metric_alias.begin(), metric_alias.end(), '.', '_');

		std::string sanitized_value_name = SanitizeMetricValueName(value_name);

		std::string column_name =
			"metric_" + metric_alias + "_" + sanitized_value_name;

		column_names.push_back({ column_name, kRPVDataTypeDouble});
	}

	// -----------------------------
	// Build JSON plan
	// -----------------------------
	jt::Json plan;
	plan.setObject();

	plan["type"] = "compute_kernel_metrics_matrix";
	plan["workload_id"] = std::stoull(workload_id);
	plan["sort_column_index"] = sort_column_index;
	plan["sort_order"] = sort_order;

	// metric selectors
	jt::Json metrics;
	metrics.setArray();

	for(const auto& [metric_id,value_name] : metric_selectors)
	{
		jt::Json m;
		m.setObject();

		m["metric_id"]  = metric_id;
		m["value_name"] = value_name;

		metrics.getArray().push_back(std::move(m));
	}

	plan["metric_selectors"] = std::move(metrics);

	// column names
	jt::Json cols;
	cols.setArray();

	for(const auto& name : column_names)
	{
		cols.getArray().push_back(jt::Json(name.first));
	}

	plan["column_names"] = std::move(cols);

	jt::Json types;
	types.setArray();

	for(const auto& name : column_names)
	{
		types.getArray().push_back(name.second);
	}

	plan["column_types"] = std::move(types);

	// filters
	jt::Json filters;
	filters.setObject();

	for(const auto& [idx,expr] : column_filters)
	{
		filters[std::to_string(idx)] = expr;
	}

	plan["filters"] = std::move(filters);

	return plan.toString();
}



    std::string ComputeQueryFactory::GetComputeMetricValues(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
		std::string query;
		std::set<std::string> metric_ids;
		std::set<std::string> kernel_ids;
		for (int i = 0; i < num; i++)
		{
			if (params[i].param_type == kRPVComputeParamKernelId)
			{
				kernel_ids.insert(params[i].param_str);
			} else
			if (params[i].param_type == kRPVComputeParamMetricId)
			{
				metric_ids.insert(params[i].param_str);
			} 
		}
		if (metric_ids.size() > 0 && kernel_ids.size() > 0)
		{
			bool use_in_clause_for_metric_ids = true;

			if (metric_ids.size() > 1)
			{
				for (auto metric_id : metric_ids)
				{
					MetricIdFormat f = ClassifyMetricIdFormat(metric_id);
					if (f == MetricIdFormat::Other)
					{
						spdlog::debug("Invalid metric format {}", metric_id);
						return query;
					}
					else
						if (f != MetricIdFormat::XYZ)
						{
							use_in_clause_for_metric_ids = false;
							break;
						}
				}
			}
			else
			{
				use_in_clause_for_metric_ids = false;
			}

			query = "SELECT metric_id, metric_name, kernel_uuid, value_name, value from compute_metric_view WHERE (";
			if (use_in_clause_for_metric_ids)
			{
				query += "metric_id IN (";
				int count = 0;
				for (auto metric_id : metric_ids)
				{
					if (count > 0)
					{
						query += ",";
					}
					query += "'";
					query += metric_id;
					query += "'";
					count++;
				}
				query += ")";
			}
			else
			{
				int count = 0;
				for (auto metric_id : metric_ids)
				{
					if (count > 0)
					{
						query += " OR ";
					}
					query += "metric_id LIKE '";
					query += metric_id;
					query += "%' ";
					count++;
				}
			}

			query += ") AND kernel_uuid IN (";
			int count = 0;
			for (auto kernel_id : kernel_ids)
			{
				if (count > 0)
				{
					query += ",";
				}
				query += kernel_id;
				count++;
			}
			query += ")";
		}
		return query;		
	}

	rocprofvis_dm_result_t ComputeDatabase::CreateIndexes()
	{ 
		std::vector<std::string> vec;
		uint32_t file_node_id = 0;
		rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
		std::vector<std::thread> threads;
		auto task = [&](std::vector<std::string> queries, uint32_t db_node_id) {   
			result = ExecuteTransaction( vec, db_node_id);
			};

		vec.push_back("CREATE INDEX IF NOT EXISTS idx_dispatch_kernel ON compute_dispatch(kernel_uuid);");
		vec.push_back("CREATE INDEX IF NOT EXISTS idx_dispatch_kernel_duration ON compute_dispatch(kernel_uuid, end_timestamp - start_timestamp);");
		vec.push_back("CREATE INDEX IF NOT EXISTS idx_metric_value_metric_uuid ON compute_metric_value(metric_uuid);");
			
	    threads.emplace_back(task, vec, file_node_id);      

		for (auto& t : threads)
			t.join();
		return kRocProfVisDmResultSuccess;
	}

	rocprofvis_dm_result_t  ComputeDatabase::ExecuteQuery(
		rocprofvis_dm_charptr_t query,
		rocprofvis_dm_charptr_t description,
		Future* future){

		ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
		while (true)
		{
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
			rocprofvis_dm_table_t table = BindObject()->FuncAddTable(BindObject()->trace_object, query, description);
			TemporaryDbInstance tmp_db_instance(0);
			ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
			if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, query, table, &CallbackRunQuery)) break;
			ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
			return future->SetPromise(kRocProfVisDmResultSuccess);
		}
		ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
		return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
	}

	rocprofvis_dm_result_t  ComputeDatabase::ReadTraceMetadata(Future* future)
	{
		ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
		DbInstances().push_back({SingleNodeDbInstance(), ""});
		TemporaryDbInstance tmp_db_instance(0);
		while (true)
		{			
			if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, "SELECT * FROM compute_metadata;", &CallbackParseMetadata)) break;
			m_query_factory.SetVersion(m_db_version.c_str());
			if (kRocProfVisDmResultSuccess != CreateIndexes()) break;
			TraceProperties()->metadata_loaded=true;
			ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
			
			std::string store_metrics_lookup_table_query = "SELECT workload_id, metric_uuid, metric_id FROM compute_metric_definition";
			if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, store_metrics_lookup_table_query.c_str(), CallbackStoreMetricsLookupTable)) break;

			return future->SetPromise(kRocProfVisDmResultSuccess);
		}
		
		ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
		return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
	}

	rocprofvis_dm_result_t
		ComputeDatabase::BuildComputeQuery(
			rocprofvis_db_compute_use_case_enum_t use_case, rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params,
			rocprofvis_dm_string_t& query) {
		if (m_query_factory.IsVersionGreaterOrEqual("1.2.0"))
		{
			switch (use_case)
			{
			case kRPVComputeFetchListOfWorkloads:
				query = m_query_factory.GetComputeListOfWorkloads(num, params);
				break;
			case kRPVComputeFetchWorkloadRooflineCeiling:
				query = m_query_factory.GetComputeWorkloadRooflineCeiling(num, params);
				break;
			case kRPVComputeFetchWorkloadTopKernels:
				query = m_query_factory.GetComputeWorkloadTopKernels(num, params);
				break;
			case kRPVComputeFetchWorkloadKernelsList:
				query = m_query_factory.GetComputeWorkloadKernelsList(num, params);
				break;
			case kRPVComputeFetchWorkloadMetricsDefinition:
				query = m_query_factory.GetComputeWorkloadMetricsDefinition(num, params);
				break;
			case kRPVComputeFetchKernelRooflineIntensities:
				query = m_query_factory.GetComputeKernelRooflineIntensities(num, params);
				break;
			case kRPVComputeFetchKernelMetricCategoriesList:
				query = m_query_factory.GetComputeKernelMetricCategoriesList(num, params);
				break;
			case kRPVComputeFetchMetricCategoryTablesList:
				query = m_query_factory.GetComputeMetricCategoryTablesList(num, params);
				break;
			case kRPVComputeFetchMetricValues:
				query = m_query_factory.GetComputeMetricValues(num, params);
				break;
			case kRPVComputeFetchKernelMetricsMatrix:
				query = m_query_factory.GetComputeKernelMetricsMatrix(num, params);
				break;
			case kRPVComputeFetchWorkloadMetricValueNames:
				query = m_query_factory.GetComputeWorkloadMetricValueNames(num, params);
				break;
			default:
				return kRocProfVisDmResultInvalidParameter;
			}

			if (query.empty())
			{
				return kRocProfVisDmResultInvalidParameter;
			}
			else
			{
				return kRocProfVisDmResultSuccess;
			}
		}
		else
		{
			return kRocProfVisDmResultNotSupported;
		}
	}


	rocprofvis_dm_result_t  ComputeDatabase::ExecuteComputeQuery(
		rocprofvis_db_compute_use_case_enum_t use_case,
		rocprofvis_dm_charptr_t query,
		Future* future){
		rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
		std::string temp_query = query;
		jt::Json kernel_metrics_matrix_plan;
		bool refresh_cached_data = true;
		bool refresh_cached_metrics_data = true;
		ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
		while (true)
		{
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
			auto it = SupportedUseCases.find(use_case);
			if (it == SupportedUseCases.end())
				break;
			rocprofvis_dm_table_t table = nullptr;
			{
				std::lock_guard lock(m_mutex);
				table = BindObject()->FuncAddTable(BindObject()->trace_object, query, it->second.c_str());
			}
			TemporaryDbInstance tmp_db_instance(0);
			ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
			RpvSqliteExecuteQueryCallback callback = nullptr;
			switch (use_case)
			{
				case kRPVComputeFetchListOfWorkloads:
				
				case kRPVComputeFetchWorkloadKernelsList:
				case kRPVComputeFetchKernelRooflineIntensities:
				case kRPVComputeFetchWorkloadMetricsDefinition:
				case kRPVComputeFetchKernelMetricCategoriesList:
				case kRPVComputeFetchMetricCategoryTablesList:
				case kRPVComputeFetchMetricValues:
				case kRPVComputeFetchWorkloadMetricValueNames:
					callback = CallbackGetComputeGeneric;
					break;
				case kRPVComputeFetchWorkloadRooflineCeiling:
					callback = CallbackGetComputeRooflineCeiling;
					break;
				case kRPVComputeFetchKernelMetricsMatrix:
				{
					callback = nullptr;
					auto [status, plan] = jt::Json::parse(query);
					if (status != jt::Json::success)
						break;
					kernel_metrics_matrix_plan = plan;
					rocprofvis_db_compute_param_t fetch_kernels_params;
					fetch_kernels_params.param_type = kRPVComputeParamWorkloadId;
					uint32_t workload_id = plan["workload_id"].getLong();
					if (m_last_matrix_workload_id != workload_id)
					{
						std::string workload_id_str = std::to_string(workload_id);
						fetch_kernels_params.param_str = workload_id_str.c_str();
						temp_query = m_query_factory.GetComputeWorkloadTopKernels(1, &fetch_kernels_params);
						if (m_last_top_kernels_query != temp_query)
						{
							m_kernel_stats.clear();
							query = temp_query.c_str();
							callback = CallbackGetComputeWorkloadTopKernels;
						}
						else
						{
							refresh_cached_data = false;
						}
						m_last_matrix_workload_id = workload_id;
					}
					else
					{
						refresh_cached_data = false;
						refresh_cached_metrics_data = false;
					}
					break;
				}
				case kRPVComputeFetchWorkloadTopKernels:
					if (m_last_top_kernels_query != query)
					{
						m_kernel_stats.clear();
						callback = CallbackGetComputeWorkloadTopKernels;
						m_last_top_kernels_query = query;
					}
					else
					{
						refresh_cached_data = false;
					}
					break;
				default:
					break;
			}

			if (refresh_cached_data)
			{
				if (callback == nullptr)
					break;
				future->ResetRowCount();
				if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, query, table, callback))
					break;
			}
			
			switch (use_case)
			{
			case kRPVComputeFetchWorkloadTopKernels:
				result = ComputeWorkloadTopKernelsMeanAndMedian(table);
				break;
			case kRPVComputeFetchKernelMetricsMatrix:
				if (refresh_cached_metrics_data)
				{
					m_metric_rows.clear();
					future->ResetRowCount();
					temp_query = "SELECT M.kernel_uuid, metric_uuid, LOWER(value_name), value FROM compute_metric_value M JOIN compute_kernel K ON M.kernel_uuid = K.kernel_uuid WHERE value IS NOT NULL AND K.workload_id = ";
					temp_query += std::to_string(m_last_matrix_workload_id);
					result = ExecuteSQLQuery(future, &tmp_db_instance, temp_query.c_str(), table, CallbackGetComputeMetricsData);
				}
				else
				{
					result = kRocProfVisDmResultSuccess;
				}
				if (kRocProfVisDmResultSuccess == result)
				{
					result = BuildKernelMetricsMatrix(table, kernel_metrics_matrix_plan);
				}
				break;
			}
			if (kRocProfVisDmResultSuccess != result) break;
			ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
			return future->SetPromise(kRocProfVisDmResultSuccess);
		}
		ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
		return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
	}

	int ComputeDatabase::CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt,
		char** azColName) {
		ROCPROFVIS_ASSERT_MSG_RETURN(argc==4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		void* func = (void*)&CallbackParseMetadata;
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		db->m_db_version = "0.0.0";
		for (int i = 0; i < argc; i++)
		{
			if (strcmp(azColName[i], "schema_version") == 0)
			{
				db->m_db_version = db->Sqlite3ColumnText(func, stmt, azColName, i);
				break;
			}
		}
		callback_params->future->CountThisRow();
		return 0;
	}

	int ComputeDatabase::CallbackGetComputeGeneric(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		void* func = (void*)&CallbackGetComputeGeneric;
		if (callback_params->future->Interrupted()) return 1;
		rocprofvis_dm_table_row_t row =
			db->BindObject()->FuncAddTableRow(callback_params->handle);
		ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);

		if(0 == callback_params->future->GetProcessedRowsCount())
		{
			for (int i=0; i < argc; i++)
			{
				rocprofvis_db_data_type_t type = (rocprofvis_db_data_type_t) sqlite3_column_type(stmt, i);
				auto it = ColumnNameToEnum.find(azColName[i]);
				if (it != ColumnNameToEnum.end())
				{
					if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle, azColName[i])) return 1;
					if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumnEnum(callback_params->handle,it->second)) return 1;
					if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumnType(callback_params->handle,type)) return 1;
				}
			}
		}
		for (int i=0; i < argc; i++)
		{
			auto it = ColumnNameToEnum.find(azColName[i]);
			if (it != ColumnNameToEnum.end())
			{
				std::string column_text = db->Sqlite3ColumnText(func, stmt, azColName, i);
				if (strcmp(azColName[i], "sub_table_id") == 0)
				{
					auto first = column_text.find('.');
					auto second = column_text.find('.', first + 1);
					column_text = column_text.substr(first + 1, second - first - 1);
				}
				if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, column_text.c_str())) return 1;
			}
		}

		callback_params->future->CountThisRow();
		return 0;
	}

	int ComputeDatabase::CallbackGetComputeRooflineCeiling(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		void* func = (void*)&CallbackGetComputeRooflineCeiling;
		if (callback_params->future->Interrupted()) return 1;
		rocprofvis_dm_table_row_t row =
			db->BindObject()->FuncAddTableRow(callback_params->handle);
		ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);

		std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(db->Sqlite3ColumnText(func, stmt, azColName, 0));
		if (parsed.first == jt::Json::Status::success)
		{
			if(0 == callback_params->future->GetProcessedRowsCount())
			{
				for (auto& [name, enumeration] : RooflineBenchParamToEnum) {
					if (parsed.second.contains(name))
					{
						if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle, name.c_str())) return 1;
						if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumnEnum(callback_params->handle,enumeration)) return 1;
						if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumnType(callback_params->handle,kRPVDataTypeDouble)) return 1;
					}
				}
			}
			for (auto& [name, enumeration] : RooflineBenchParamToEnum) {
				if (parsed.second.contains(name))
				{
					if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, parsed.second[name].toString().c_str())) return 1;
				}
			}
		}

		callback_params->future->CountThisRow();
		return 0;
	}

    int ComputeDatabase::CallbackGetComputeKernelMetricsMatrix(void* data, int argc,
                                                               sqlite3_stmt* stmt,
                                                               char**        azColName)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        rocprofvis_db_sqlite_callback_parameters* callback_params =
            (rocprofvis_db_sqlite_callback_parameters*) data;
        ComputeDatabase* db   = (ComputeDatabase*) callback_params->db;
        void*            func = (void*) &CallbackGetComputeKernelMetricsMatrix;
        if(callback_params->future->Interrupted()) return 1;

        rocprofvis_dm_table_row_t row =
            db->BindObject()->FuncAddTableRow(callback_params->handle);
        ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);

        // Add column definitions on first row
        if(0 == callback_params->future->GetProcessedRowsCount())
        {
            for(int i = 0; i < argc; i++)
            {
                rocprofvis_db_data_type_t type =
                    (rocprofvis_db_data_type_t) sqlite3_column_type(stmt, i);

                // Check if this is a known static column
                auto it = ColumnNameToEnum.find(azColName[i]);
                if(it != ColumnNameToEnum.end())
                {
                    // Static column with known enum
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumn(callback_params->handle,
                                                            azColName[i]))
                        return 1;
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumnEnum(callback_params->handle,
                                                                it->second))
                        return 1;
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumnType(callback_params->handle,
                                                                type))
                        return 1;
                }
                else
                {
                    // Dynamic metric column - use generic enum
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumn(callback_params->handle,
                                                            azColName[i]))
                        return 1;
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumnEnum(
                           callback_params->handle, kRPVComputeColumnDynamicMetricValue))
                        return 1;
                    if(kRocProfVisDmResultSuccess !=
                       db->BindObject()->FuncAddTableColumnType(callback_params->handle,
                                                                type))
                        return 1;
                }
            }
        }

        // Add cell values for this row
        for(int i = 0; i < argc; i++)
        {
            std::string column_text = db->Sqlite3ColumnText(func, stmt, azColName, i);
            if(kRocProfVisDmResultSuccess !=
               db->BindObject()->FuncAddTableRowCell(row, column_text.c_str()))
                return 1;
        }

        callback_params->future->CountThisRow();
        return 0;
    }

	int ComputeDatabase::CallbackGetComputeWorkloadTopKernels(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		void* func = (void*)&CallbackGetComputeWorkloadTopKernels;
		if (callback_params->future->Interrupted()) return 1;
		uint32_t kernel_uuid = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
		std::string kernel_name = db->Sqlite3ColumnText(func, stmt, azColName, 3);
		uint64_t duration = db->Sqlite3ColumnInt64(func, stmt, azColName, 5);
		auto& s = db->m_kernel_stats[kernel_uuid];
		s.count++;
		s.sum += duration;
		s.min = std::min(s.min, duration);
		s.max = std::max(s.max, duration);
		s.name = kernel_name;
		s.durations.push_back(duration);

		callback_params->future->CountThisRow();
		return 0;
	}

	int ComputeDatabase::CallbackStoreMetricsLookupTable(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		void* func = (void*)&CallbackStoreMetricsLookupTable;
		if (callback_params->future->Interrupted()) return 1;
		MetricRow m;
		uint32_t workload_id = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
		uint32_t metric_uuid = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
		std::string metric_id = db->Sqlite3ColumnText(func, stmt, azColName, 2);
		db->m_metric_id_lookup[workload_id][metric_uuid] = metric_id;
		db->m_metric_uuid_lookup[workload_id].push_back({metric_id, metric_uuid});
		callback_params->future->CountThisRow();
		return 0;
	}

	int ComputeDatabase::CallbackGetComputeMetricsData(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
		ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
		rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
		ComputeDatabase* db = (ComputeDatabase*)callback_params->db;
		void* func = (void*)&CallbackGetComputeMetricsData;
		if (callback_params->future->Interrupted()) return 1;
		MetricRow m;
		m.kernel_uuid = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
		m.metric_uuid = db->Sqlite3ColumnInt(func, stmt, azColName, 1);
		m.value_name = db->Sqlite3ColumnText(func, stmt, azColName, 2);
		m.value = db->Sqlite3ColumnDouble(func, stmt, azColName, 3);
		db->m_metric_rows.push_back(m);
		callback_params->future->CountThisRow();
		return 0;
	}

	rocprofvis_dm_result_t ComputeDatabase::ComputeWorkloadTopKernelsMeanAndMedian(rocprofvis_dm_table_t table)
	{
		rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
		std::pair<std::string, rocprofvis_db_data_type_t> columns[] = { 
			{"kernel_uuid",kRPVDataTypeInt},
			{"kernel_name",kRPVDataTypeString},
			{"dispatch_count",kRPVDataTypeInt},
			{"duration_ns_sum",kRPVDataTypeInt},
			{"duration_ns_mean",kRPVDataTypeDouble},
			{"duration_ns_min",kRPVDataTypeInt},
			{"duration_ns_max",kRPVDataTypeInt},
			{"duration_ns_median",kRPVDataTypeDouble} };

		for (auto& column : columns)
		{
			auto it = ColumnNameToEnum.find(column.first);
			if (it != ColumnNameToEnum.end())
			{
				result = BindObject()->FuncAddTableColumn(table, column.first.c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableColumnEnum(table, it->second);
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableColumnType(table, column.second);
				if (kRocProfVisDmResultSuccess != result) break;
			}
		}
		if (kRocProfVisDmResultSuccess == result)
		{
			for (auto& [kernel, s] : m_kernel_stats) {
				s.mean = static_cast<double>(s.sum) / s.count;

				std::sort(s.durations.begin(), s.durations.end());
				size_t n = s.durations.size();

				if (n % 2 == 0)
					s.median = (s.durations[n / 2 - 1] + s.durations[n / 2]) / 2.0;
				else
					s.median = s.durations[n / 2];
				rocprofvis_dm_table_row_t row = BindObject()->FuncAddTableRow(table);

				result = BindObject()->FuncAddTableRowCell(row, std::to_string(kernel).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, s.name.c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.count).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.sum).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.mean).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.min).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.max).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(s.median).c_str());
				if (kRocProfVisDmResultSuccess != result) break;
			}
		}
		return result;
	}

	rocprofvis_dm_result_t ComputeDatabase::BuildKernelMetricsMatrix(rocprofvis_dm_table_t table, jt::Json& plan)
	{
		rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
		std::vector<MetricSelector> selectors;

		struct column_data_t
		{
			std::string name;
			rocprofvis_db_data_type_t type;
			bool has_filter;
			FilterExpression filter;
			FilterExpression::Value eval_value;
		};

		std::vector<column_data_t> columns;

		uint32_t workload_id = plan["workload_id"].getLong();

		if (plan.contains("column_names"))
		{
			auto& arr = plan["column_names"].getArray();
			columns.resize(arr.size());
			
			for (int i=0; i < arr.size(); i++)
			{
				columns[i].name = arr[i].getString();
				columns[i].has_filter = false;
			}
		}
		if (plan.contains("column_types"))
		{
			auto& arr = plan["column_types"].getArray();
			for (int i=0; i < arr.size(); i++)
			{
				columns[i].type = (rocprofvis_db_data_type_t)arr[i].getLong();
			}
		}

		if (plan.contains("filters"))
		{
			auto& obj = plan["filters"].getObject();

			for (auto& s : obj)
			{
				size_t column_index = std::atoll(s.first.c_str());
				columns[column_index].filter = FilterExpression::Parse(columns[column_index].name + " " + s.second.getString());
				columns[column_index].has_filter = true;
			}
		}
			

		if (plan.contains("metric_selectors"))
		{
			auto& arr = plan["metric_selectors"].getArray();

			for (auto& s : arr)
			{
				MetricSelector sel;
				sel.metric_id = s["metric_id"].getString();
				sel.value_name = s["value_name"].getString();

				selectors.push_back(std::move(sel));

			}
		}

		size_t metric_count = selectors.size();

		std::unordered_map<uint32_t, KernelMetricsRow> matrix;

		for (const auto& [uuid, stats] : m_kernel_stats)
		{
			KernelMetricsRow row;
			row.kernel_uuid = uuid;
			row.stats = stats;
			row.metrics.resize(metric_count, std::numeric_limits<double>::quiet_NaN());

			matrix.emplace(uuid, std::move(row));
		}

		// -----------------------------
		// Pivot metrics
		// -----------------------------
		for (const auto& m : m_metric_rows)
		{
			auto kernel_it = matrix.find(m.kernel_uuid);
			if (kernel_it == matrix.end())
				continue;

			for (size_t i = 0; i < selectors.size(); ++i)
			{
				if (m_metric_id_lookup[workload_id][m.metric_uuid] == selectors[i].metric_id &&
					m.value_name == selectors[i].value_name)
				{
					kernel_it->second.metrics[i] = m.value;
				}
			}
		}


		for (auto& column : columns)
		{
			auto it = ColumnNameToEnum.find(column.name);
			rocprofvis_db_compute_column_enum_t enum_id = kRPVComputeColumnDynamicMetricValue;
			if (it != ColumnNameToEnum.end())
			{
				enum_id = it->second;
			}
			result = BindObject()->FuncAddTableColumn(table, column.name.c_str());
			if (kRocProfVisDmResultSuccess != result) break;
			result = BindObject()->FuncAddTableColumnEnum(table, enum_id);
			if (kRocProfVisDmResultSuccess != result) break;
			result = BindObject()->FuncAddTableColumnType(table, column.type);
			if (kRocProfVisDmResultSuccess != result) break;			
		}
		if (kRocProfVisDmResultSuccess == result)
		{
			for (auto& [uuid, mrow] : matrix)
			{
				uint32_t column_index = 0;
				columns[column_index++].eval_value = (double)mrow.kernel_uuid;
				columns[column_index++].eval_value = mrow.stats.name;
				columns[column_index++].eval_value = (double)mrow.stats.count;
				columns[column_index++].eval_value = (double)mrow.stats.sum;
				for (double value : mrow.metrics)
				{
					columns[column_index++].eval_value = value;
				}
				bool passed_evaluation = true;
				for (auto column : columns)
				{
					if (column.has_filter)
					{
						if (!column.filter.Evaluate({ {column.name, column.eval_value} }))
						{
							passed_evaluation = false;
							break;
						}
					}
				}
				if (!passed_evaluation) continue;

				rocprofvis_dm_table_row_t row = BindObject()->FuncAddTableRow(table);
				result = BindObject()->FuncAddTableRowCell(row, std::to_string(mrow.kernel_uuid).c_str());
				if (kRocProfVisDmResultSuccess != result) break;

				result = BindObject()->FuncAddTableRowCell(row, mrow.stats.name.c_str());
				if (kRocProfVisDmResultSuccess != result) break;

				result = BindObject()->FuncAddTableRowCell(row, std::to_string(mrow.stats.count).c_str());
				if (kRocProfVisDmResultSuccess != result) break;

				result = BindObject()->FuncAddTableRowCell(row, std::to_string(mrow.stats.sum).c_str());
				if (kRocProfVisDmResultSuccess != result) break;

				for (double value : mrow.metrics)
				{
					result = BindObject()->FuncAddTableRowCell(row, std::to_string(value).c_str());
					if (kRocProfVisDmResultSuccess != result) break;

				}
				if (kRocProfVisDmResultSuccess != result) break;
			}
		}
		return result;
	}


}  // namespace DataModel
}  // namespace RocProfVis
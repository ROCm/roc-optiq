// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_compute.h"
#include "json.h"

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
		{kRPVComputeFetchKernelMetricsMatrix, "Fetch kernel metrics matrix with pivoted metric columns"}
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
	};

	static const std::unordered_map<std::string, rocprofvis_db_compute_column_enum_t> RooflineBenchParamToEnum{
		{"HBMBw", kRPVComputeColumnWorkloadRooflineBenchHBMBw},
		{"L2Bw", kRPVComputeColumnWorkloadRooflineBenchL2Bw},
		{"L1Bw", kRPVComputeColumnWorkloadRooflineBenchL1Bw},
		{"LDSBw", kRPVComputeColumnWorkloadRooflineBenchLDSBw},
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
			query = "SELECT ";
			query += "kernel_uuid, ";
			query += "kernel_name, ";
			query += "dispatch_count, ";
			query += "duration_ns_sum, ";
			query += "duration_ns_mean, ";
			query += "duration_ns_min, ";
			query += "duration_ns_max, ";
			query += "duration_ns_median ";
			query += "FROM ";
			query += "compute_kernel_view WHERE workload_id = ";
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

	std::string ComputeQueryFactory::GetComputeKernelMetricsMatrix(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params) {
        std::string query;
        std::string workload_id;
        int         sort_column_index = 1;       // default to duration_ns_sum (index 1)
        std::string sort_order        = "DESC";  // default to descending
        
		// (metric_id, value_name) pairs
		std::vector<std::pair<std::string, std::string>> metric_selectors;
        std::vector<std::string> column_names;  // Track column order for sorting

        // Parse parameters
        for(size_t i = 0; i < num; i++)
        {
            if(params[i].param_type == kRPVComputeParamWorkloadId)
            {
                workload_id = params[i].param_str;
            }
            else if(params[i].param_type == kRPVComputeParamMetricSelector)
            {
                // Expected format: "metric_id:value_name" e.g., "2.1.4:peak"
                std::string selector_str = params[i].param_str;
                size_t      colon_pos    = selector_str.find(':');
                if(colon_pos != std::string::npos)
                {
                    std::string metric_id  = selector_str.substr(0, colon_pos);
                    std::string value_name = selector_str.substr(colon_pos + 1);
                    metric_selectors.push_back({ metric_id, value_name });
                }
            }
            else if(params[i].param_type == kRPVComputeParamSortColumnIndex)
            {
                // Parse string to integer, TODO: make safer with error handling
                sort_column_index = std::atoi(params[i].param_str);
            }
            else if(params[i].param_type == kRPVComputeParamSortColumnOrder)
            {
                std::string sort_order_in = params[i].param_str;
                // convert to uppercase
                std::transform(sort_order_in.begin(), sort_order_in.end(),
                               sort_order_in.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                if(sort_order_in == "ASC")
                {
                    sort_order = "ASC";
                }
                else
                {
                    // Default to DESC if not ASC
                    sort_order = "DESC";
                }
            }
        }

        // Validate required parameters
        if(workload_id.empty())
        {
            return query;  // Return empty on invalid input
        }

        // Build column name list (index 0 = kernel_name, index 1 = duration_ns_sum, index
        // 2+ = metrics)
        column_names.push_back("kernel_name");
        column_names.push_back("duration_ns_sum");

        // Build the SELECT clause
        query = "SELECT \n";
        query += "    kernel_name,\n";
        query += "    duration_ns_sum";

        // Add CASE statements for each metric selector
        for(const auto& [metric_id, value_name] : metric_selectors)
        {
            query += ",\n    MAX(CASE WHEN LOWER(value_name) = '";
            query += value_name;
            query += "' AND metric_id = '";
            query += metric_id;
            query += "' THEN value END) AS ";

            // Generate column alias by replacing dots with underscores and sanitizing value name
            std::string metric_alias = metric_id;
            std::replace(metric_alias.begin(), metric_alias.end(), '.', '_');
            std::string sanitized_value_name = SanitizeMetricValueName(value_name);
            std::string column_name = "metric_" + metric_alias + "_" + sanitized_value_name;
            query += column_name;

            // Track column name for sorting
            column_names.push_back(column_name);
        }

        // Add FROM clause
        query += "\nFROM compute_kernel_metrics_view\n";

        // Add WHERE clause
        query += "WHERE workload_id = ";
        query += workload_id;
        // TODO: allow other filtering ?
        query += "\n";

        // Add GROUP BY clause
        query += "GROUP BY kernel_name, duration_ns_sum\n";

        // Add ORDER BY clause with validated index
        query += "ORDER BY ";
        if(sort_column_index >= 0 &&
           sort_column_index < static_cast<int>(column_names.size()))
        {
            query += column_names[sort_column_index];
        }
        else
        {
            query += "duration_ns_sum";  // fallback to default
        }
        query += " " + sort_order;

        return query;
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
			TraceProperties()->metadata_loaded=true;
			ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
			
			//add "kernel metrics" view to database
			std::string create_view_query = "CREATE VIEW IF NOT EXISTS compute_kernel_metrics_view AS "
			"SELECT "
				"ckv.workload_id, "
				"ckv.workload_name, "
				"ckv.kernel_uuid, "
				"ckv.kernel_name, "
				"ckv.duration_ns_sum, "
				"ckv.duration_ns_mean, "
				"ckv.duration_ns_median, "
				"ckv.duration_ns_min, "
				"ckv.duration_ns_max, "
				"ckv.dispatch_count, "
				"cmv.metric_id, "
				"cmv.metric_name, "
				"cmv.value_name, "
				"cmv.value "
			"FROM compute_kernel_view ckv "
			"LEFT JOIN compute_metric_view cmv ON ckv.kernel_uuid = cmv.kernel_uuid; ";
			if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, create_view_query.c_str(), nullptr)) break;

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

		ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
		while (true)
		{
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
			ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
			auto it = SupportedUseCases.find(use_case);
			if (it == SupportedUseCases.end())
				break;
			rocprofvis_dm_table_t table = BindObject()->FuncAddTable(BindObject()->trace_object, query, it->second.c_str());
			TemporaryDbInstance tmp_db_instance(0);
			ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
			RpvSqliteExecuteQueryCallback callback = nullptr;
			switch (use_case)
			{
				case kRPVComputeFetchListOfWorkloads:
				case kRPVComputeFetchWorkloadTopKernels:
				case kRPVComputeFetchWorkloadKernelsList:
				case kRPVComputeFetchWorkloadMetricsDefinition:
				case kRPVComputeFetchKernelRooflineIntensities:
				case kRPVComputeFetchKernelMetricCategoriesList:
				case kRPVComputeFetchMetricCategoryTablesList:
				case kRPVComputeFetchMetricValues:
					callback = CallbackGetComputeGeneric;
					break;
				case kRPVComputeFetchWorkloadRooflineCeiling:
					callback = CallbackGetComputeRooflineCeiling;
					break;
				case kRPVComputeFetchKernelMetricsMatrix:
					callback = CallbackGetComputeKernelMetricsMatrix;
					break;
				default:
					break;
			}
			if (callback == nullptr)
				break;
			future->ResetRowCount();
			if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, &tmp_db_instance, query, table, callback)) 
				break;
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

}  // namespace DataModel
}  // namespace RocProfVis
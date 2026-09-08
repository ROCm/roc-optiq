// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db.h"
#include "rocprofvis_db_profile.h"
#include <cstdio>
#include <sstream>
#include <cfloat>

namespace RocProfVis
{
namespace DataModel
{


bool Database::SanitizeFilePath(const std::string& filename, std::filesystem::path& out_path) {
    std::filesystem::path input(filename);

    if (!input.is_absolute())
        return false;

    std::error_code ec;
    std::filesystem::path canonical_path = std::filesystem::canonical(input, ec);
    if (ec)
        return false;

    if (!std::filesystem::is_regular_file(canonical_path, ec) || ec)
        return false;

    out_path = canonical_path;
    return true;
}

bool Database::IsNumber(const std::string& s) {
    std::istringstream iss(s);
    uint64_t d;
    return iss >> std::noskipws >> d && iss.eof();
}


void  Database::ShowProgress(
                                                    double step, 
                                                    rocprofvis_dm_charptr_t action, 
                                                    rocprofvis_db_status_t status, 
                                                    Future* future){
    future->ShowProgress(Path(), step, action, status);
}


rocprofvis_dm_result_t Database::BindTrace(rocprofvis_dm_db_bind_struct * binding_info){
    m_binding_info = binding_info;
    m_binding_info->FuncFindCachedTableValue = FindCachedTableValue;
    m_binding_info->FuncGetInfoTableNumColumns = GetInfoTableNumColumns;
    m_binding_info->FuncGetInfoTableNumRows = GetInfoTableNumRows;
    m_binding_info->FuncGetInfoTableColumnName = GetInfoTableColumnName;
    m_binding_info->FuncGetInfoTableRowHandle = GetInfoTableRowHandle;
    m_binding_info->FuncGetInfoTableRowCellValue = GetInfoTableRowCellValue;
    m_binding_info->FuncGetInfoTableRowNumCells = GetInfoTableRowNumCells;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::CleanupAsync(
    rocprofvis_db_future_t object, bool rebuild){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(Database::CleanupStatic, this, future, rebuild)));
    }
    catch (const std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataAsync(
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceMetadataStatic, this, future)));
    }
    catch (const std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t  Database::CleanupStatic(Database* db, Future* future, bool rebuild) {
    return db->Cleanup(future, rebuild);
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataStatic(
                                                    Database* db, 
                                                    Future* object){
    return db->ReadTraceMetadata(object);
}


rocprofvis_dm_result_t   Database::FindCachedTableValue(  
                                                        const rocprofvis_dm_database_t object, 
                                                        rocprofvis_dm_charptr_t table, 
                                                        const rocprofvis_dm_id_t id, 
                                                        rocprofvis_dm_charptr_t column,
                                                        rocprofvis_dm_node_id_t node,
                                                        rocprofvis_dm_charptr_t* value){
    Database* db = (Database*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    *value = db->CachedTables(static_cast<uint32_t>(node))->GetTableCell(table, id, column); 
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_table_t Database::GetInfoTableHandle(const rocprofvis_dm_database_t object, rocprofvis_dm_node_id_t node, rocprofvis_dm_charptr_t table_name){
    Database* db = (Database*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, nullptr);
    return db->CachedTables(static_cast<uint32_t>(node))->GetTableHandle(table_name);
}
size_t Database::GetInfoTableNumColumns(rocprofvis_dm_table_t object){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->NumColumns();
}
size_t Database::GetInfoTableNumRows(rocprofvis_dm_table_t object){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->NumRows();
}
const char* Database::GetInfoTableColumnName(rocprofvis_dm_table_t object, size_t column_index){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->GetColumnName(static_cast<uint32_t>(column_index));
}
rocprofvis_dm_table_row_t Database::GetInfoTableRowHandle(rocprofvis_dm_table_t object, size_t row_index){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->GetRow(row_index);
}
const char* Database::GetInfoTableRowCellValue(rocprofvis_dm_table_row_t object, size_t column_index){
    TableCache::Row * row = (TableCache::Row*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 0);
    return row->values[column_index].c_str();
}

const size_t Database::GetInfoTableRowNumCells(rocprofvis_dm_table_row_t object){
    TableCache::Row * row = (TableCache::Row*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 0);
    return row->values.size();
}

rocprofvis_dm_size_t    Database::GetMemoryFootprint(void) {
    rocprofvis_dm_size_t size = 0;
    for (auto guid : m_db_instances)
    {
        size+= m_cached_tables[guid.first.GuidIndex()].GetMemoryFootprint();
    }
    return size;
}


}  // namespace DataModel
}  // namespace RocProfVis
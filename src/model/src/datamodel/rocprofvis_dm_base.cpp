// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_dm_base.h"

namespace RocProfVis
{
namespace DataModel
{

rocprofvis_dm_result_t DmBase::GetPropertyAsUint64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, uint64_t* value){
    (void) property;
    (void) index;
    (void) value;
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}
rocprofvis_dm_result_t DmBase::GetPropertyAsInt64(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, int64_t* value){
    (void) property;
    (void) index;
    (void) value;
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}
rocprofvis_dm_result_t DmBase::GetPropertyAsCharPtr(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, char** value){
    (void) property;
    (void) index;
    (void) value;
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}
rocprofvis_dm_result_t DmBase::GetPropertyAsDouble(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, double* value){
    (void) property;
    (void) index;
    (void) value;
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}
rocprofvis_dm_result_t DmBase::GetPropertyAsHandle(rocprofvis_dm_property_t property, rocprofvis_dm_property_index_t index, rocprofvis_dm_handle_t* value){
    (void) property;
    (void) index;
    (void) value;
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_INVALID_PROPERTY_GETTER, kRocProfVisDmResultInvalidProperty);
}

#ifdef TEST
const char*  DmBase::GetPropertySymbol(rocprofvis_dm_property_t property) {

    return "????????";
}
#endif

}  // namespace DataModel
}  // namespace RocProfVis

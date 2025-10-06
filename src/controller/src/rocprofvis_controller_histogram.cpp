#include "rocprofvis_controller_histogram.h"

namespace RocProfVis
{
namespace Controller
{

HistogramController::HistogramController(std::vector<uint64_t> bins)
: m_bins(bins)
{}

HistogramController::~HistogramController() {}

rocprofvis_controller_object_type_t
HistogramController::GetType(void)
{
    // You may want to define a new enum value for histogram, e.g.,
    // kRPVControllerObjectTypeHistogram
    return kRPVControllerObjectTypeHistogram;
}

rocprofvis_result_t
HistogramController::GetUInt64(rocprofvis_property_t property, uint64_t index,
                               uint64_t* value)
{
    if(!value) return kRocProfVisResultInvalidArgument;

    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
        case kRPVControllerHistogramBinCount:
        {
            *value = m_bins.size();
            result = kRocProfVisResultSuccess;
            return result;
        }
        case kRPVControllerHistogramBinNumber:
        {
            if(index < m_bins.size())
            {
                *value = m_bins[index];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
            return result;
        }
        }
    
    }
    return kRocProfVisResultInvalidEnum;
}

rocprofvis_result_t
HistogramController::GetDouble(rocprofvis_property_t property, uint64_t index,
                               double* value)
{
    return kRocProfVisResultInvalidType;
}

rocprofvis_result_t
HistogramController::GetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t** value)
{
    return kRocProfVisResultInvalidType;
}

rocprofvis_result_t
HistogramController::GetString(rocprofvis_property_t property, uint64_t index,
                               char* value,
                     uint32_t* length)
{
    return kRocProfVisResultInvalidType;
}

rocprofvis_result_t
HistogramController::SetUInt64(rocprofvis_property_t property, uint64_t index,
                               uint64_t value)
{
    switch(property)
    {
   
        default: return kRocProfVisResultInvalidEnum;
    }
}

rocprofvis_result_t
HistogramController::SetDouble(rocprofvis_property_t property, uint64_t index,
                               double value)
{
    return kRocProfVisResultInvalidType;
}

rocprofvis_result_t
HistogramController::SetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t* value)
{
    return kRocProfVisResultInvalidType;
}

rocprofvis_result_t
HistogramController::SetString(rocprofvis_property_t property, uint64_t index,
                               char const* value,
                     uint32_t length)
{
    return kRocProfVisResultInvalidType;
}

}  // namespace Controller
}  // namespace RocProfVis

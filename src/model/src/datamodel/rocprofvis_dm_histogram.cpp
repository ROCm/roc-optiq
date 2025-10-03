#include "rocprofvis_dm_histogram.h"

namespace RocProfVis
{
namespace DataModel
{

Histogram::Histogram(Trace* ctx, rocprofvis_dm_charptr_t description)
: m_ctx(ctx)
, m_description(description ? description : "")
{
    m_id = std::hash<std::string>{}(m_description);
}

void
Histogram::AddBin(uint64_t value)
{
    m_bins.push_back(value);
}

rocprofvis_dm_size_t
Histogram::GetNumberOfBins() const
{
    return static_cast<rocprofvis_dm_size_t>(m_bins.size());
}

rocprofvis_dm_size_t
Histogram::GetMemoryFootprint() const
{
    return sizeof(Histogram) + m_bins.size() * sizeof(uint64_t) +
           m_description.capacity();
}
void
Histogram::SetBins(const std::vector<uint64_t>& bins)
{
    m_bins = bins;
  
}

rocprofvis_dm_charptr_t
Histogram::Description() const
{
    return m_description.c_str();
}

rocprofvis_dm_result_t
Histogram::GetBinAt(rocprofvis_dm_property_index_t index, uint64_t& value) const
{
    if(index >= m_bins.size()) return kRocProfVisDmResultNotLoaded;
    value = m_bins[index];
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
Histogram::GetPropertyAsUint64(rocprofvis_dm_property_t       property,
                               rocprofvis_dm_property_index_t index, uint64_t* value)
{
    if(!value) return kRocProfVisDmResultInvalidParameter;
    switch(property)
    {
        case kRPVDMHistogramBinValue:
        {
            m_bins;
            return GetBinAt(index, *value);
        }
        case kRPVDMHistogramBinCount:
        {
            *value = GetNumberOfBins();
            return kRocProfVisDmResultSuccess;  
        }
          
        // Add histogram-specific property cases here as needed
        default: return kRocProfVisDmResultInvalidProperty;
    }
}

rocprofvis_dm_result_t
Histogram::GetPropertyAsCharPtr(rocprofvis_dm_property_t property,
                                rocprofvis_dm_property_index_t /*index*/, char** value)
{
    if(!value) return kRocProfVisDmResultInvalidParameter;
    switch(property)
    {
        // Add histogram-specific property cases here as needed
        default: return kRocProfVisDmResultInvalidProperty;
    }
}

rocprofvis_dm_result_t
Histogram::GetPropertyAsHandle(rocprofvis_dm_property_t /*property*/,
                               rocprofvis_dm_property_index_t /*index*/,
                               rocprofvis_dm_handle_t* /*value*/)
{
    // No handle properties for Histogram by default
    return kRocProfVisDmResultInvalidProperty;
}

#ifdef TEST
const char*
Histogram::GetPropertySymbol(rocprofvis_dm_property_t property)
{
    switch(property)
    {
        // Add histogram-specific property symbols here as needed
        default: return "Unknown property";
    }
}
#endif

}  // namespace DataModel
}  // namespace RocProfVis

#include "MetricTrack.h"
#include "MetricRecord.h"
#include "Trace.h"
#include <cassert>


MetricTrack::MetricTrack(Trace* ctx, 
                const uint32_t gpuId, 
                const uint32_t metricId,
                const uint64_t numRecords): 
                    Track(ctx,TrackType::METRICS, numRecords),
                    m_gpuId(gpuId),
                    m_metricId(metricId) {
    
    m_group_name = "GPU " + std::to_string(m_gpuId);
    m_name = m_ctx->getStringAt(m_metricId);

};

TrackArray*   MetricTrack::addTrackArray(uint64_t startTimestamp, uint64_t endTimestamp)
{
    MetricArray* array =  new MetricArray(this,startTimestamp, endTimestamp);
    m_chunks.push_back(array);
    return array;
}



uint64_t MetricArray::addRecord(DataRecord & data) 
{
    try {
        m_samples.push_back(MetricRecord(data.Metric.timestamp,data.Metric.value));
    }
    catch(std::exception ex)
    {
        return INVALID_RECORD_INDEX;
    }
    return m_samples.back().getTimestamp();
};


size_t  MetricArray::getRecordArrayMemoryFootprint()
{
    return sizeof(std::vector<MetricRecord>) + (sizeof(MetricRecord) * m_samples.size());
};


uint32_t MetricArray::timestampToIndex(const uint64_t timestamp)
{
    std::vector<MetricRecord>::iterator it = std::find_if( m_samples.begin(), m_samples.end(),
            [&timestamp](MetricRecord & x) { return x.getTimestamp() >= timestamp;});
    if (it != m_samples.end())
    {
        return (uint32_t)(it - m_samples.begin());

    }
    return INVALID_RECORD_INDEX;
            
}

uint64_t  MetricArray::getRecordTimestampAt(const int index)
{
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getTimestamp();
    } 
    return 0;
}

double  MetricArray::getRecordValueAt(const int index)
{
    assert(index < m_samples.size() && "Index out of range");
    if (index < m_samples.size())
    {
        return m_samples[index].getValue();
    } 
    return 0;
}





// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_model_types.h"

using namespace RocProfVis::View;

RawTrackData::RawTrackData(rocprofvis_controller_track_type_t track_type,
                           uint64_t track_id, double start_ts, double end_ts,
                           uint64_t data_group_id, size_t chunk_count)
: m_track_type(track_type)
, m_track_id(track_id)
, m_start_ts(start_ts)
, m_end_ts(end_ts)
, m_data_group_id(data_group_id)
, m_expected_chunk_count(chunk_count)
{}

void
RawTrackData::SetDataRequestTimePoint(
    const std::chrono::steady_clock::time_point& request_time)
{
    m_request_time = request_time;
}

const std::chrono::steady_clock::time_point& RawTrackData::GetDataRequestTimePoint() const
{
    return m_request_time;
}

rocprofvis_controller_track_type_t
RawTrackData::GetType() const
{
    return m_track_type;
}

double
RawTrackData::GetStartTs() const
{
    return m_start_ts;
}
double
RawTrackData::GetEndTs() const
{
    return m_end_ts;
}
uint64_t
RawTrackData::GetTrackID() const
{
    return m_track_id;
}
uint64_t
RawTrackData::GetDataGroupID() const
{
    return m_data_group_id;
}

size_t
RawTrackData::GetChunkCount() const
{
    return m_chunk_info.size();
}

bool
RawTrackData::AllDataReady() const
{
    // Check if the number of chunks received matches the expected count
    return m_chunk_info.size() == m_expected_chunk_count;
}

// Explicit template instantiation
template class RocProfVis::View::TemplatedRawTrackData<TraceCounter>;
template class RocProfVis::View::TemplatedRawTrackData<TraceEvent>;

template <typename T>
TemplatedRawTrackData<T>::TemplatedRawTrackData(uint64_t track_id, double start_ts, double end_ts,
                                                 uint64_t data_group_id, size_t chunk_count)
: RawTrackData(data_traits<T>::track_type, track_id, start_ts, end_ts, data_group_id, chunk_count)
{}

template <typename T>
TemplatedRawTrackData<T>::~TemplatedRawTrackData() { m_data.clear(); }

template <typename T>
const std::vector<T>&
TemplatedRawTrackData<T>::GetData() const
{
    return m_data;
}

template <typename T>
void
TemplatedRawTrackData<T>::SetData(std::vector<T>&& data)
{
    m_data = std::move(data);
}

template <typename T>
std::unordered_set<typename TemplatedRawTrackData<T>::id_type>&
TemplatedRawTrackData<T>::GetWritableIdSet()
{
    return m_ids;
}

template <typename T>
bool TemplatedRawTrackData<T>::AddChunk(size_t chunk_index, const std::vector<T> &&chunk_data) {
        // Prevent adding same chunk index
        if (m_chunk_info.count(chunk_index)) {
            return false;
        }

        // Calculate the insertion position (offset)
        size_t insertion_offset = 0;
        for (const auto& pair : m_chunk_info) {
            if (pair.first < chunk_index) {
                // pair.second is the chunk size
                insertion_offset += pair.second; 
            } else {
                break;
            }
        }

        // Insert the new chunk's data
        if (!chunk_data.empty()) {
            m_data.insert(
                m_data.begin() + insertion_offset,
                std::make_move_iterator(chunk_data.begin()),
                std::make_move_iterator(chunk_data.end())
            );
        }

        // Record the new chunk's size
        m_chunk_info[chunk_index] = chunk_data.size();

        return true;
    }

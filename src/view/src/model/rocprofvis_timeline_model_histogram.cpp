// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Kept apart from rocprofvis_timeline_model.cpp because it reads the timeline's
// TrackItems, and the rest of the model is built into the headless data library.

#include "rocprofvis_timeline_model.h"
#include "rocprofvis_track_item.h"

#include <algorithm>
#include <cfloat>

namespace RocProfVis
{
namespace View
{

void
TimelineModel::UpdateHistogram(const std::vector<TrackItem*>& tracks)
{
    /*
    This function updates histogram and mini_map.
    */

    // Recompute histogram from all visible tracks
    if(!m_histogram.empty())
    {
        std::fill(m_histogram.begin(), m_histogram.end(), 0.0);
        for(const TrackItem* track : tracks)
        {
            // If it's a counter/sample, or hidden track, skip adding it to the global
            // histogram
            bool is_included =
                track && track->GetTrackInfo() && m_mini_map.count(track->GetID()) &&
                track->IsDisplayed() &&
                track->GetTrackInfo()->track_type == kRPVControllerTrackTypeEvents;
            if(is_included)
            {
                const std::vector<double>& mini_data = m_mini_map.at(track->GetID());
                for(size_t i = 0; i < mini_data.size() && i < m_histogram.size(); ++i)
                {
                    m_histogram[i] += mini_data[i];
                }
            }
        }

        if(m_histogram_max_value_global == DBL_MIN)
        {
            /*This block only exists to get the global maximum value for
             * normalization*/
            std::vector<double> global_sum_histogram(m_histogram.size(), 0.0);

            m_minimap_global_min = DBL_MAX;
            m_minimap_global_max = -DBL_MAX;

            for(const TrackItem* track : tracks)
            {
                // If it's a counter/sample track skip adding it to the global histogram
                if(!track || !track->GetTrackInfo() ||
                   !m_mini_map.count(track->GetID()) ||
                   track->GetTrackInfo()->track_type != kRPVControllerTrackTypeEvents)
                {
                    continue;
                }

                const std::vector<double>& mini_data = m_mini_map.at(track->GetID());

                for(double val : mini_data)
                {
                    if(val != 0)  // Skip zeros to match local min/max calculation
                    {
                        if(val < m_minimap_global_min) m_minimap_global_min = val;
                        if(val > m_minimap_global_max) m_minimap_global_max = val;
                    }
                }

                for(size_t i = 0; i < mini_data.size() && i < global_sum_histogram.size(); ++i)
                {
                    global_sum_histogram[i] += mini_data[i];
                }
            }
            if(!global_sum_histogram.empty())
            {
                m_histogram_max_value_global =
                    *std::max_element(global_sum_histogram.begin(),
                                      global_sum_histogram.end());
            }
            if(m_minimap_global_min == DBL_MAX) m_minimap_global_min = 0.0;
            if(m_minimap_global_max == -DBL_MAX) m_minimap_global_max = 0.0;
        }

        // Normalize histogram to [0, 1]
        NormalizeHistogram();
    }
}

}  // namespace View
}  // namespace RocProfVis

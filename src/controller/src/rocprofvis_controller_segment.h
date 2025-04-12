// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <map>
#include <vector>
#include <memory>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Event;
class Sample;

class Segment
{
    class LOD
    {
    public:
        LOD();

        LOD(LOD const& other);

        LOD(LOD&& other);
        
        ~LOD();

        LOD& operator=(LOD const& other);

        LOD& operator=(LOD&& other);

        std::multimap<double, Handle*>& GetEntries();

        void SetValid(bool valid);

        bool IsValid() const;

    private:
        std::multimap<double, Handle*> m_entries;
        bool m_valid;
    };

    void GenerateEventLOD(std::vector<Event*>& events, double event_start, double event_end, uint32_t lod_to_generate);

    void GenerateSampleLOD(std::vector<Sample*>& samples, rocprofvis_controller_primitive_type_t type, double timestamp, uint32_t lod_to_generate);

public:
    Segment();

    Segment(rocprofvis_controller_track_type_t type);

    ~Segment();

    void GenerateLOD(uint32_t lod_to_generate);

    double GetStartTimestamp();
    double GetEndTimestamp();
    double GetMinTimestamp();
    double GetMaxTimestamp();

    void SetStartEndTimestamps(double start, double end);

    void SetMinTimestamp(double value);

    void SetMaxTimestamp(double value);

    void Insert(double timestamp, Handle* event);

    rocprofvis_result_t Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index);

    rocprofvis_result_t GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property);

private:
    std::map<uint32_t, std::unique_ptr<LOD>> m_lods;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_timestamp;
    double m_max_timestamp;
    rocprofvis_controller_track_type_t m_type;
};

}
}

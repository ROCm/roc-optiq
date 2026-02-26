// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_cache.h"
#include <vector>
#include <unordered_map>


namespace RocProfVis
{
namespace DataModel
{

 typedef std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>>::iterator rocprofvis_dm_track_params_it;

class Database;

class TrackLookup
{

public:

    TrackLookup(Database* db) : m_db(db) {};

    struct TrackKey
    {
        uint64_t id0;
        uint64_t id1;
        uint64_t id2;

        bool operator==(const TrackKey & other) const
        {
            return id0 == other.id0 &&
                id1 == other.id1 &&
                id2 == other.id2;
        }
    };

    struct TrackKeyHash
    {
        std::size_t operator()(const TrackKey & k) const
        {
            // simple fast hash combine
            size_t h = std::hash<uint64_t>()(k.id0);
            h ^= std::hash<uint64_t>()(k.id1) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
            h ^= std::hash<uint64_t>()(k.id2) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
            return h;
        }
    };

    struct TrackEntry
    {
        rocprofvis_dm_track_category_t category;
        uint32_t track;
    };

    void AddTrack(rocprofvis_dm_track_identifiers_t& ids, uint32_t db_instance, uint32_t track);

    bool FindTrack(const TrackKey& key, rocprofvis_dm_track_category_t category, uint32_t& out_track);


    bool FindTrack(rocprofvis_dm_track_identifiers_t& ids, uint32_t db_instance, uint32_t& out_track);

    bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track);

    bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, const char* id_subprocess, uint32_t db_instance, uint32_t& out_track);

    rocprofvis_dm_track_params_it FindTrackParamsIterator(rocprofvis_dm_track_identifiers_t& track_indentifiers, uint32_t db_instance);

    uint32_t GetStringIdentifierIndex(const char* str_id) { return m_string_lookup.ToInt(str_id); }


private:

    TrackKey MakeKey(const rocprofvis_dm_track_identifiers_t& ids, uint32_t db_instance);

    TrackKey MakeKey(uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance);

    TrackKey MakeKey(uint64_t id_process, const char* id_subprocess, uint32_t db_instance);

    TrackKey MakeKey(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint64_t id_stream, uint32_t db_instance);
 
    std::unordered_map<TrackKey, std::vector<TrackEntry>, TrackKeyHash> m_track_lookup;
    StringTable m_string_lookup;
    Database* m_db;

};

}  // namespace DataModel
}  // namespace RocProfVis
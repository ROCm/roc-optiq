// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db.h"

namespace RocProfVis
{
namespace DataModel
{

    void TrackLookup::AddTrack(rocprofvis_dm_track_identifiers_t & ids, 
        uint32_t db_instance,
        uint32_t track)
    {
        TrackKey key = MakeKey(ids, db_instance);
        m_track_lookup[key].push_back({ ids.category, track });
    }

    bool TrackLookup::FindTrack(const TrackKey & key,
        rocprofvis_dm_track_category_t category,
        uint32_t & out_track)
    {
        auto it = m_track_lookup.find(key);
        if (it == m_track_lookup.end())
            return false;

        const auto & vec = it->second;

        for (const auto & entry : vec)
        {
            if (entry.category & category)
            {
                out_track = entry.track;
                return true;
            }
        }

        return false;
    }

    bool TrackLookup::FindTrack(rocprofvis_dm_track_identifiers_t & ids,
        uint32_t db_instance,
        uint32_t& out_track)
    {
        TrackKey key = MakeKey(ids, db_instance);
        return FindTrack(key, ids.category, out_track);
    }

    bool TrackLookup::FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance,
        uint32_t& out_track)
    {
        TrackKey key = MakeKey(id_process, id_subprocess, db_instance);
        return FindTrack(key, category, out_track);
    }

    bool TrackLookup::FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, const char* id_subprocess, uint32_t db_instance,
        uint32_t& out_track)
    {
        TrackKey key = MakeKey(id_process, id_subprocess, db_instance);
        return FindTrack(key, category, out_track);
    }

    rocprofvis_dm_track_params_it TrackLookup::FindTrackParamsIterator(rocprofvis_dm_track_identifiers_t& track_indentifiers, uint32_t db_instance) {
        uint32_t track_id;
        if (!FindTrack(track_indentifiers, db_instance,track_id))
        {
            return m_db->TrackPropertiesEnd();
        }
        return std::find_if(
            m_db->TrackPropertiesBegin(), m_db->TrackPropertiesEnd(),
            [track_id](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
                return params.get()->track_indentifiers.track_id == track_id;
            });
    }

    TrackLookup::TrackKey TrackLookup::MakeKey(const rocprofvis_dm_track_identifiers_t & ids, uint32_t db_instance)
    {
        TrackKey key;
        key.id0 = ids.id[TRACK_ID_PID_OR_AGENT];
        if (ids.is_numeric[TRACK_ID_COUNTER] == false)
        {
            key.id1 = m_string_lookup.ToInt(ids.name[TRACK_ID_COUNTER].c_str());
        }
        else
        {
            key.id1 = ids.id[TRACK_ID_TID_OR_QUEUE];
        }
        key.id2 = db_instance;
        return key;
    }

    TrackLookup::TrackKey TrackLookup::MakeKey(uint64_t id_process, const char* id_subprocess, uint32_t db_instance)
    {
        TrackKey key;
        key.id0 = id_process;
        key.id1 = m_string_lookup.ToInt(id_subprocess);
        key.id2 = db_instance;
        return key;
    }

    TrackLookup::TrackKey TrackLookup::MakeKey(uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance)
    {
        TrackKey key;
        key.id0 = id_process;
        key.id1 = id_subprocess;
        key.id2 = db_instance;
        return key;
    }

    TrackLookup::TrackKey TrackLookup::MakeKey(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint64_t id_stream, uint32_t db_instance)
    {
        if (category & kRocProfVisDmStreamTrack)
        {
            return MakeKey(id_stream, -1, db_instance);
        }
        else
        {
            return MakeKey(id_process, id_subprocess, db_instance);
        }
    }

}  // namespace DataModel
}  // namespace RocProfVis
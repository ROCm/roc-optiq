// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_trace_links.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "spdlog/spdlog.h"

#include "rocprofvis_json_utils.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr const char* TRACE_LINK_ID_PREFIX    = "link-";
constexpr const char* TRACE_LINKS_TEMP_SUFFIX = ".tmp";
constexpr uint64_t    TRACE_LINK_FIRST_ID     = 1;

bool
GroupHas(const TraceLinkGroup& group, const std::string& path)
{
    for(const TraceLinkMember& member : group.members)
    {
        if(member.path == path)
        {
            return true;
        }
    }
    return false;
}

// The number in a "link-N" id, or zero for an id this build did not make.
uint64_t
GroupIdNumber(const std::string& id)
{
    const std::string prefix = TRACE_LINK_ID_PREFIX;
    if(id.compare(0, prefix.size(), prefix) != 0)
    {
        return 0;
    }
    return std::strtoull(id.c_str() + prefix.size(), nullptr, 10);
}

jt::Json
MemberToJson(const TraceLinkMember& member)
{
    jt::Json json;
    json.setObject();
    json["path"]          = member.path;
    json["role"]          = member.role;
    json["label"]         = member.label;
    jt::Json& fingerprint = json["fingerprint"];
    fingerprint.setObject();
    fingerprint["start_ns"]    = member.fingerprint.start_ns;
    fingerprint["end_ns"]      = member.fingerprint.end_ns;
    fingerprint["track_count"] = static_cast<long long>(member.fingerprint.track_count);
    return json;
}

// False for an entry that names no trace, which cannot be one end of a link.
bool
MemberFromJson(jt::Json& json, TraceLinkMember& member_out)
{
    if(!json.isObject())
    {
        return false;
    }
    member_out.path  = JsonUtils::GetString(json, "path", "");
    member_out.role  = JsonUtils::GetString(json, "role", "");
    member_out.label = JsonUtils::GetString(json, "label", "");
    if(json.contains("fingerprint") && json["fingerprint"].isObject())
    {
        jt::Json& fingerprint = json["fingerprint"];
        member_out.fingerprint.start_ns =
            JsonUtils::GetDouble(fingerprint, "start_ns", 0.0);
        member_out.fingerprint.end_ns = JsonUtils::GetDouble(fingerprint, "end_ns", 0.0);
        const double tracks = JsonUtils::GetDouble(fingerprint, "track_count", 0.0);
        member_out.fingerprint.track_count =
            tracks > 0.0 ? static_cast<uint64_t>(tracks) : 0;
    }
    return !member_out.path.empty();
}

// Atomically replaces target with temp. std::filesystem::rename has POSIX
// replacement semantics on Unix, but Windows refuses when target exists.
bool
ReplaceFile(const std::filesystem::path& temp, const std::filesystem::path& target,
            std::error_code& error_out)
{
#ifdef _WIN32
    if(MoveFileExW(temp.c_str(), target.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
    {
        error_out =
            std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return false;
    }
    error_out.clear();
#else
    std::filesystem::rename(temp, target, error_out);
    if(error_out)
    {
        return false;
    }
#endif
    return true;
}

}  // namespace

bool
TraceFingerprint::IsSet() const
{
    return track_count > 0 || end_ns > start_ns;
}

// Exact comparison is intended: both sides come from the same timeline values,
// directly or through a lossless JSON round trip.
bool
TraceFingerprint::Matches(const TraceFingerprint& other) const
{
    if(!IsSet() || !other.IsSet())
    {
        return true;
    }
    return start_ns == other.start_ns && end_ns == other.end_ns &&
           track_count == other.track_count;
}

TraceLinkStore::TraceLinkStore(const std::string& file_path)
: m_file_path(file_path)
, m_next_id(TRACE_LINK_FIRST_ID)
, m_read_only(false)
{
    Load();
}

const std::vector<TraceLinkGroup>&
TraceLinkStore::Groups() const
{
    return m_groups;
}

std::vector<TraceLink>
TraceLinkStore::LinksOf(const std::string& path) const
{
    std::vector<TraceLink> links;
    if(path.empty())
    {
        return links;
    }
    for(const TraceLinkGroup& group : m_groups)
    {
        const TraceLinkMember* self = nullptr;
        for(const TraceLinkMember& member : group.members)
        {
            if(member.path == path)
            {
                self = &member;
                break;
            }
        }
        if(self == nullptr)
        {
            continue;
        }
        for(const TraceLinkMember& member : group.members)
        {
            if(&member == self)
            {
                continue;
            }
            TraceLink link;
            link.group_id = group.id;
            link.self     = *self;
            link.other    = member;
            links.push_back(link);
        }
    }
    return links;
}

TraceLinkResult
TraceLinkStore::Link(const TraceLinkMember& baseline, const TraceLinkMember& target)
{
    if(baseline.path.empty() || target.path.empty() || baseline.path == target.path)
    {
        return TraceLinkResult::kInvalid;
    }
    for(const TraceLinkGroup& group : m_groups)
    {
        if(GroupHas(group, baseline.path) && GroupHas(group, target.path))
        {
            return TraceLinkResult::kAlreadyLinked;
        }
    }

    TraceLinkGroup group;
    group.id    = TRACE_LINK_ID_PREFIX + std::to_string(m_next_id);
    group.kind  = TRACE_LINK_KIND_BASELINE_TARGET;
    group.label = baseline.label + " vs " + target.label;
    group.members.push_back(baseline);
    group.members.back().role = TRACE_LINK_ROLE_BASELINE;
    group.members.push_back(target);
    group.members.back().role = TRACE_LINK_ROLE_TARGET;
    m_groups.push_back(group);
    ++m_next_id;
    return Save() ? TraceLinkResult::kLinked : TraceLinkResult::kLinkedNotSaved;
}

TraceUnlinkResult
TraceLinkStore::Unlink(const std::string& group_id)
{
    for(std::vector<TraceLinkGroup>::iterator it = m_groups.begin(); it != m_groups.end();
        ++it)
    {
        if(it->id == group_id)
        {
            m_groups.erase(it);
            return Save() ? TraceUnlinkResult::kUnlinked
                          : TraceUnlinkResult::kUnlinkedNotSaved;
        }
    }
    return TraceUnlinkResult::kNotFound;
}

jt::Json
TraceLinkStore::ToJson(const std::vector<TraceLinkGroup>& groups, uint64_t next_id)
{
    jt::Json root;
    root.setObject();
    root["version"] = TRACE_LINKS_VERSION;
    root["next_id"] = static_cast<long long>(next_id);
    jt::Json& out   = root["groups"];
    out.setArray();
    for(size_t i = 0; i < groups.size(); ++i)
    {
        const TraceLinkGroup& group = groups[i];
        jt::Json&             node  = out[i];
        node.setObject();
        node["id"]        = group.id;
        node["kind"]      = group.kind;
        node["label"]     = group.label;
        node["note"]      = group.note;
        jt::Json& members = node["members"];
        members.setArray();
        for(size_t m = 0; m < group.members.size(); ++m)
        {
            members[m] = MemberToJson(group.members[m]);
        }
    }
    return root;
}

void
TraceLinkStore::ParseGroups(const jt::Json& root, std::vector<TraceLinkGroup>& groups_out,
                            uint64_t& next_id_out, bool& newer_out)
{
    groups_out.clear();
    next_id_out = TRACE_LINK_FIRST_ID;
    newer_out   = false;

    jt::Json& mutable_root = const_cast<jt::Json&>(root);
    if(!mutable_root.isObject())
    {
        return;
    }
    newer_out =
        JsonUtils::GetInt(root, "version", TRACE_LINKS_VERSION) > TRACE_LINKS_VERSION;
    const double stored_next = JsonUtils::GetDouble(root, "next_id", 0.0);
    if(stored_next > static_cast<double>(next_id_out))
    {
        next_id_out = static_cast<uint64_t>(stored_next);
    }
    if(!mutable_root.contains("groups") || !mutable_root["groups"].isArray())
    {
        return;
    }

    std::unordered_set<std::string>               seen_ids;
    std::set<std::pair<std::string, std::string>> seen_pairs;
    for(jt::Json& node : mutable_root["groups"].getArray())
    {
        if(!node.isObject())
        {
            continue;
        }
        TraceLinkGroup group;
        group.id    = JsonUtils::GetString(node, "id", "");
        group.kind  = JsonUtils::GetString(node, "kind", TRACE_LINK_KIND_BASELINE_TARGET);
        group.label = JsonUtils::GetString(node, "label", "");
        group.note  = JsonUtils::GetString(node, "note", "");
        // Even a malformed group may have been named in an earlier
        // conversation, so its id is never handed out again.
        next_id_out = std::max(next_id_out, GroupIdNumber(group.id) + 1);
        if(node.contains("members") && node["members"].isArray())
        {
            for(jt::Json& member_node : node["members"].getArray())
            {
                TraceLinkMember member;
                if(MemberFromJson(member_node, member))
                {
                    group.members.push_back(member);
                }
            }
        }
        // Version 1 knows exactly one relationship shape. Being strict here
        // prevents compare_traces from guessing roles in malformed or future
        // groups.
        if(group.id.empty() || group.kind != TRACE_LINK_KIND_BASELINE_TARGET ||
           group.members.size() != 2 || group.members[0].path == group.members[1].path ||
           !seen_ids.insert(group.id).second)
        {
            continue;
        }
        TraceLinkMember* baseline = nullptr;
        TraceLinkMember* target   = nullptr;
        for(TraceLinkMember& member : group.members)
        {
            if(member.role == TRACE_LINK_ROLE_BASELINE && baseline == nullptr)
            {
                baseline = &member;
            }
            else if(member.role == TRACE_LINK_ROLE_TARGET && target == nullptr)
            {
                target = &member;
            }
        }
        if(baseline == nullptr || target == nullptr)
        {
            continue;
        }
        const std::string& first_path =
            baseline->path < target->path ? baseline->path : target->path;
        const std::string& second_path =
            baseline->path < target->path ? target->path : baseline->path;
        if(!seen_pairs.emplace(first_path, second_path).second)
        {
            continue;
        }
        // Keep the stored order canonical too, so every consumer sees baseline
        // then target regardless of how a hand-edited file listed them.
        if(group.members[0].role != TRACE_LINK_ROLE_BASELINE)
        {
            std::swap(group.members[0], group.members[1]);
        }
        groups_out.push_back(group);
    }
}

void
TraceLinkStore::Load()
{
    m_groups.clear();
    m_next_id   = TRACE_LINK_FIRST_ID;
    m_read_only = false;

    jt::Json root;
    if(!JsonUtils::ReadFile(m_file_path, root))
    {
        return;
    }
    bool newer = false;
    ParseGroups(root, m_groups, m_next_id, newer);
    m_read_only = newer;
    if(newer)
    {
        spdlog::warn("{} was written by a newer Optiq, so links made here last only for "
                     "this session",
                     m_file_path);
    }
}

bool
TraceLinkStore::Save() const
{
    if(m_read_only)
    {
        return false;
    }

    const std::filesystem::path target(m_file_path);
    std::filesystem::path       temp = target;
    temp += TRACE_LINKS_TEMP_SUFFIX;

    std::error_code ec;
    if(target.has_parent_path())
    {
        std::filesystem::create_directories(target.parent_path(), ec);
    }

    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if(!out.is_open())
    {
        spdlog::warn("Could not write {}", temp.string());
        return false;
    }
    const std::string text = ToJson(m_groups, m_next_id).toStringPretty();
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    // Close and check before the rename, so a short write never replaces the
    // links that are already on disk.
    out.close();
    if(out.fail())
    {
        spdlog::warn("Could not finish writing {}", temp.string());
        std::filesystem::remove(temp, ec);
        return false;
    }

    if(!ReplaceFile(temp, target, ec))
    {
        spdlog::warn("Could not replace {}: {}", target.string(), ec.message());
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

}  // namespace View
}  // namespace RocProfVis

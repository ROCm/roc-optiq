// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr const char* TRACE_LINK_ROLE_BASELINE        = "baseline";
constexpr const char* TRACE_LINK_ROLE_TARGET          = "target";
constexpr const char* TRACE_LINK_KIND_BASELINE_TARGET = "baseline_target";
constexpr const char* TRACE_LINKS_FILE_NAME           = "trace_links.json";
constexpr int32_t     TRACE_LINKS_VERSION             = 1;

/*
 * What a trace looked like when it was linked, read from the trace rather than
 * the file. Optiq writes into the database it opens, so the file's size and
 * modification time change just from opening it, and cannot say whether the
 * capture behind a path was replaced.
 */
struct TraceFingerprint
{
    double   start_ns    = 0.0;
    double   end_ns      = 0.0;
    uint64_t track_count = 0;

    // False for a member saved without one, which then matches anything.
    bool IsSet() const;
    bool Matches(const TraceFingerprint& other) const;
};

// One trace in a link group, named by Project::GetID().
struct TraceLinkMember
{
    std::string      path;
    std::string      role;
    std::string      label;
    TraceFingerprint fingerprint;
};

// Traces the user said belong together, such as two runs of one program.
struct TraceLinkGroup
{
    std::string                  id;
    std::string                  kind;
    std::string                  label;
    std::string                  note;
    std::vector<TraceLinkMember> members;
};

// One link seen from a trace in it: the trace asked about, and the other end.
struct TraceLink
{
    std::string     group_id;
    TraceLinkMember self;
    TraceLinkMember other;
};

enum class TraceLinkResult : uint8_t
{
    kLinked,
    // Linked for this session, but the file could not be written.
    kLinkedNotSaved,
    kAlreadyLinked,
    // The two ends were the same trace, or one had no path.
    kInvalid
};

enum class TraceUnlinkResult : uint8_t
{
    kUnlinked,
    // Removed for this session, but the file could not be written.
    kUnlinkedNotSaved,
    kNotFound
};

/**
 * @brief The links between traces, persisted in {config}/trace_links.json.
 *
 * Every change is written through at once, to a temporary file that is then
 * renamed over the old one, so a crash mid-write leaves the previous links
 * rather than a truncated file. A file written by a newer build is read for
 * what this one understands and never written back, so nothing it added is
 * lost. Group ids are never reused, because a model can still hold an old one
 * from earlier in a conversation.
 */
class TraceLinkStore
{
public:
    explicit TraceLinkStore(const std::string& file_path);

    const std::vector<TraceLinkGroup>& Groups() const;
    // Every link that names this trace, oldest first.
    std::vector<TraceLink> LinksOf(const std::string& path) const;

    TraceLinkResult Link(const TraceLinkMember& baseline, const TraceLinkMember& target);
    TraceUnlinkResult Unlink(const std::string& group_id);

    // Serialization, public so it can be exercised without a file.
    static jt::Json ToJson(const std::vector<TraceLinkGroup>& groups, uint64_t next_id);
    // Reads what it can and skips the rest. Sets newer_out when the document
    // comes from a later version than this build writes.
    static void ParseGroups(const jt::Json& root, std::vector<TraceLinkGroup>& groups_out,
                            uint64_t& next_id_out, bool& newer_out);

private:
    friend struct TraceLinkStoreTestPeer;

    void Load();
    bool Save() const;

    std::string                 m_file_path;
    std::vector<TraceLinkGroup> m_groups;
    uint64_t                    m_next_id;
    // Set when the file came from a newer build, so it is never overwritten.
    bool m_read_only;
};

}  // namespace View
}  // namespace RocProfVis

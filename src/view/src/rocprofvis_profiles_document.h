// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <string>

namespace RocProfVis
{
namespace View
{

enum class ConnectionType;

// Where the profiler tools and their Python environment live on one run
// destination: this machine, or one SSH connection.
struct LaunchEnvironment
{
    std::string tool_directory;
    std::string python_environment;
};

// Single on-disk document ({config}/profiles.json) holding SSH connection
// profiles, profiler launch profiles and per-destination launch environments:
//
//   {
//     "version": 1,
//     "ssh_connections": { "<id>": { ...SshConnectionConfig... } },
//     "launch_profiles": { "<profiler_id>": { "<name>": { ...LaunchConfig... } } },
//     "launch_environments": { "<id>": { "id": "<id>", "tool_directory": "...",
//                                        "python_environment": "..." } }
//   }
//
// This is a thin persistence helper: it loads/saves the whole document and
// hands out references to the two sections. The SshConnectionStore and the
// launch preset storage build their typed views on top of these sections.
//
// A process-wide singleton keeps the two stores consistent (both edit the same
// in-memory document and persist the whole thing).
class ProfilesDocument
{
public:
    static ProfilesDocument& Get();

    // Reloads the document from disk (called lazily on first Get()).
    void Reload();

    // Persists the whole document to disk, dropping top-level keys and old flat
    // launch presets this build does not read. Returns false on I/O failure.
    bool Persist();

    // Mutable access to the sections (each is a JSON object). Created on
    // demand if absent.
    jt::Json& SshConnections();
    jt::Json& LaunchProfiles();
    jt::Json& LaunchEnvironments();

    /*
     * Returns the launch environment of a run destination, or empty values when
     * it has no entry or is an SSH destination with no connection.
     * @param connection_type: whether the destination is this machine or SSH.
     * @param ssh_connection_id: the SSH connection's id. Ignored for this machine.
     */
    LaunchEnvironment GetLaunchEnvironment(ConnectionType     connection_type,
                                           const std::string& ssh_connection_id);
    /*
     * Writes the launch environment of a run destination and persists the
     * document. Returns false on I/O failure or for an SSH destination with no
     * connection.
     * @param connection_type: whether the destination is this machine or SSH.
     * @param ssh_connection_id: the SSH connection's id. Ignored for this machine.
     */
    bool SetLaunchEnvironment(ConnectionType connection_type, const std::string& ssh_connection_id,
                              const LaunchEnvironment& environment);
    /*
     * Adds an empty launch environment for an SSH connection that has none.
     * Does not persist.
     */
    void AddLaunchEnvironment(const std::string& ssh_connection_id);
    /*
     * Removes an SSH connection's launch environment. Does not persist.
     */
    void RemoveLaunchEnvironment(const std::string& ssh_connection_id);

private:
    ProfilesDocument();

    std::string GetFilePath() const;
    // Makes sure the version, the sections and the local launch environment exist.
    // @param clear: also drop what this build does not read - top-level keys
    //   other than "version" and the sections, and
    //   entries in "launch_profiles" that are presets in the old flat layout
    //   rather than per-profiler groups. Erased in place, so references to the
    //   sections stay valid.
    void        EnsureSections(bool clear);

    jt::Json m_root;
};

}  // namespace View
}  // namespace RocProfVis

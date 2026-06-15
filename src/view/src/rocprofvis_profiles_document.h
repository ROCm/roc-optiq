// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <string>

namespace RocProfVis
{
namespace View
{

// Single on-disk document ({config}/profiles.json) holding both SSH connection
// profiles and profiler launch profiles in two top-level sections:
//
//   {
//     "version": 1,
//     "ssh_connections": { "<id>": { ...SshConnectionConfig... } },
//     "launch_profiles": { "<name>": { ...LaunchConfig... } }
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

    // Persists the whole document to disk. Returns false on I/O failure.
    bool Persist();

    // Mutable access to the two sections (each is a JSON object). Created on
    // demand if absent.
    jt::Json& SshConnections();
    jt::Json& LaunchProfiles();

private:
    ProfilesDocument();

    std::string GetFilePath() const;
    void        EnsureSections();

    jt::Json m_root;
};

}  // namespace View
}  // namespace RocProfVis

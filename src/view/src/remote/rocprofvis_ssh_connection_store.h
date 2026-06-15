// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_connection_config.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Registry of named SSH connection profiles, persisted as the "ssh_connections"
// section of the unified {config}/profiles.json document (see ProfilesDocument).
//
// Profiles are keyed by their stable SshConnectionConfig::id. Display names are
// for the UI only and need not be unique (though the dialog encourages it).
class SshConnectionStore
{
public:
    SshConnectionStore();

    // Loads connections from disk (clears current in-memory state first).
    // Returns false if the file is missing/unreadable; the store is then empty.
    bool Load();

    // Persists all connections to disk. Returns false on I/O failure.
    bool Persist();

    // All connections in insertion order.
    const std::vector<SshConnectionConfig>& List() const { return m_connections; }

    // Returns a pointer to the connection with the given id, or nullptr.
    const SshConnectionConfig* Get(const std::string& id) const;

    // Inserts or updates a connection (matched by id) and persists. If the
    // config has no id, one is generated and written back into the argument.
    void Save(SshConnectionConfig& config);

    // Removes the connection with the given id (if present) and persists.
    void Remove(const std::string& id);

    bool Empty() const { return m_connections.empty(); }

private:
    std::vector<SshConnectionConfig> m_connections;
};

}  // namespace View
}  // namespace RocProfVis

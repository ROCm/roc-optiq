// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_connection_store.h"
#include "rocprofvis_profiles_document.h"

namespace RocProfVis
{
namespace View
{

SshConnectionStore::SshConnectionStore()
{
}

bool SshConnectionStore::Load()
{
    m_connections.clear();

    jt::Json& connections = ProfilesDocument::Get().SshConnections();
    if(!connections.isObject())
    {
        return false;
    }

    for(auto& entry : connections.getObject())
    {
        SshConnectionConfig cfg = SshConnectionConfig::FromJson(entry.second);
        // Trust the on-disk object key as the id of record.
        cfg.id = entry.first;
        m_connections.push_back(std::move(cfg));
    }

    return true;
}

bool SshConnectionStore::Persist()
{
    jt::Json& connections = ProfilesDocument::Get().SshConnections();
    connections.setObject();
    for(const auto& cfg : m_connections)
    {
        connections[cfg.id] = cfg.ToJson();
    }

    return ProfilesDocument::Get().Persist();
}

const SshConnectionConfig* SshConnectionStore::Get(const std::string& id) const
{
    for(const auto& cfg : m_connections)
    {
        if(cfg.id == id)
        {
            return &cfg;
        }
    }
    return nullptr;
}

void SshConnectionStore::Save(SshConnectionConfig& config)
{
    if(config.id.empty())
    {
        config.id = SshConnectionConfig::GenerateId();
    }

    for(auto& cfg : m_connections)
    {
        if(cfg.id == config.id)
        {
            cfg = config;
            Persist();
            return;
        }
    }

    m_connections.push_back(config);
    Persist();
}

void SshConnectionStore::Remove(const std::string& id)
{
    for(auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        if(it->id == id)
        {
            m_connections.erase(it);
            Persist();
            return;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis

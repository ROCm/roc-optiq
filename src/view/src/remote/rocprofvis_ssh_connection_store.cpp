// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_connection_store.h"
#include "rocprofvis_profiles_document.h"
#include "rocprofvis_secret_store.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// Vault keys for a connection's secrets, derived from its stable id.
std::string password_key(const std::string& id)
{
    return "ssh/" + id + "/password";
}

std::string passphrase_key(const std::string& id)
{
    return "ssh/" + id + "/passphrase";
}

// Writes the config's secrets to the vault, erasing entries whose field is now
// empty. Returns false when the vault is unavailable so the caller knows the
// secrets were not persisted.
bool store_secrets(const SshConnectionConfig& config)
{
    if(!SecretStore::IsAvailable())
    {
        return false;
    }

    bool ok = true;
    if(!config.password.empty())
    {
        ok = SecretStore::Set(password_key(config.id), config.password) && ok;
    }
    else
    {
        SecretStore::Erase(password_key(config.id));
    }

    if(!config.passphrase.empty())
    {
        ok = SecretStore::Set(passphrase_key(config.id), config.passphrase) && ok;
    }
    else
    {
        SecretStore::Erase(passphrase_key(config.id));
    }
    return ok;
}

// Populates the config's in-memory secrets from the OS vault, if present.
void load_secrets(SshConnectionConfig& config)
{
    std::string value;
    if(SecretStore::Get(password_key(config.id), value))
    {
        config.password = value;
    }
    if(SecretStore::Get(passphrase_key(config.id), value))
    {
        config.passphrase = value;
    }
}

void erase_secrets(const std::string& id)
{
    SecretStore::Erase(password_key(id));
    SecretStore::Erase(passphrase_key(id));
}

}  // namespace

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

    bool had_legacy_plaintext = false;
    for(auto& entry : connections.getObject())
    {
        // Old builds stored plaintext secrets here. We don't migrate them; the
        // field stays empty (the user re-enters on connect) and the plaintext is
        // stripped from disk below.
        if(entry.second.contains("password") || entry.second.contains("passphrase"))
        {
            had_legacy_plaintext = true;
        }

        SshConnectionConfig cfg = SshConnectionConfig::FromJson(entry.second);
        // Trust the on-disk object key as the id of record.
        cfg.id = entry.first;
        load_secrets(cfg);
        m_connections.push_back(std::move(cfg));
    }

    // Rewrite the document so any leftover plaintext is dropped (ToJson no
    // longer emits secrets).
    if(had_legacy_plaintext)
    {
        Persist();
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

    bool replaced = false;
    for(auto& cfg : m_connections)
    {
        if(cfg.id == config.id)
        {
            cfg      = config;
            replaced = true;
            break;
        }
    }
    if(!replaced)
    {
        m_connections.push_back(config);
    }

    // Secrets go to the OS credential vault, never into profiles.json.
    store_secrets(config);
    Persist();
}

void SshConnectionStore::Remove(const std::string& id)
{
    for(auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        if(it->id == id)
        {
            m_connections.erase(it);
            erase_secrets(id);
            Persist();
            return;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis

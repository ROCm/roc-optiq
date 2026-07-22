// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_fetch.h"

namespace RocProfVis
{
namespace View
{


    void PromptRequest::Update(std::string name,
        std::string instruction,
        std::vector<PromptItem> prompts)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_name = std::move(name);
        m_instruction = std::move(instruction);
        m_prompts = std::move(prompts);

        m_updated = true;
    }

    std::optional<PromptRequest::Snapshot> PromptRequest::ConsumeIfUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_updated)
        {
            return std::nullopt;
        }

        return Snapshot{
            m_name,
            m_instruction,
            m_prompts
        };
    }

    void PromptRequest::ClearUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_updated = false;
    }

    PromptRequest::Snapshot PromptRequest::Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return Snapshot{
            m_name,
            m_instruction,
            m_prompts
        };
    }

    void PromptRequest::ClearPrompts()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_prompts.clear();
        m_updated = true;
    }

    void HostKeyRequest::Update(std::string host,
        uint64_t port,
        std::string fingerprint,
        std::string key_type,
        HostKeyState state)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_host = std::move(host);
        m_port = port;
        m_fingerprint_sha256_b64 = std::move(fingerprint);
        m_key_type = std::move(key_type);
        m_state = state;

        m_updated = true;
    }

    std::optional<HostKeyRequest::Snapshot> HostKeyRequest::ConsumeIfUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_updated)
        {
            return std::nullopt;
        }

        return Snapshot{
            m_host,
            m_port,
            m_fingerprint_sha256_b64,
            m_key_type,
            m_state
        };
    }

    void HostKeyRequest::ClearUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_updated = false;
    }

    HostKeyRequest::Snapshot HostKeyRequest::Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return Snapshot{
            m_host,
            m_port,
            m_fingerprint_sha256_b64,
            m_key_type,
            m_state
        };
    }


    void ExecutionOutput::Append(std::string text)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_text += text;
        m_updated = true;
    }

    std::optional<ExecutionOutput::Snapshot> ExecutionOutput::ConsumeIfUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_updated)
        {
            return std::nullopt;
        }

        // The terminal (finished) snapshot must reach the UI exactly once so the
        // output popup can auto-close; clear the latch after delivering it so it
        // is not re-emitted every frame.
        Snapshot snapshot{ m_text, m_finished };
        if (m_finished)
        {
            m_updated = false;
        }
        return snapshot;
    }

    void ExecutionOutput::ClearUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_updated = false;
        m_finished = false;
    }

    void ExecutionOutput::Finish()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_finished = true;
        m_updated = true;
    }

    ExecutionOutput::Snapshot ExecutionOutput::Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return Snapshot{ m_text };
    }

    void ExecutionOutput::Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_text.clear();
        m_updated = true;
    }

    void FileStat::Update(std::string name,
        uint64_t size,
        uint64_t time,
        uint64_t downloaded)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_name = std::move(name);
        m_size = size;
        m_time = time;
        m_downloaded = downloaded;

        m_updated = true;
    }

    std::optional<FileStat::Snapshot> FileStat::ConsumeIfUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_updated)
        {
            return std::nullopt;
        }

        m_updated = false;

        return Snapshot{
            m_name,
            m_size,
            m_time,
            m_downloaded
        };
    }

    FileStat::Snapshot FileStat::Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return Snapshot{
            m_name,
            m_size,
            m_time,
            m_downloaded
        };
    }

    void FileStat::SetDownloaded(uint64_t downloaded)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_downloaded = downloaded;
        m_updated = true;
    }


    void RemoteDir::Update(std::string name,
        uint64_t size,
        uint64_t time,
        uint64_t attrs)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_list_dir.push_back({ std::move(name), size, time, attrs & FileAttrs::Directory ? true : false });
        m_updated = true;
    }

    std::optional<RemoteDir::Snapshot> RemoteDir::ConsumeIfUpdated()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_updated)
        {
            return std::nullopt;
        }

        m_updated = false;

        return Snapshot{
            m_list_dir
        };
    }

    RemoteDir::Snapshot RemoteDir::Get() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        return Snapshot{
            m_list_dir
        };
    }

    void RemoteDir::Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_list_dir.clear();
        m_updated = true;
    }


}  // namespace View
}  // namespace RocProfVis

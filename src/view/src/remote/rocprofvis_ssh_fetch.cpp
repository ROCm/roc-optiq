// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_fetch.h"

namespace RocProfVis
{
namespace View
{


    void PromptRequest::update(std::string name,
        std::string instruction,
        std::vector<PromptItem> prompts)
    {
        std::lock_guard<std::mutex> lock(m_);

        name_ = std::move(name);
        instruction_ = std::move(instruction);
        prompts_ = std::move(prompts);

        updated_ = true;
    }

    std::optional<PromptRequest::Snapshot> PromptRequest::consume_if_updated()
    {
        std::lock_guard<std::mutex> lock(m_);

        if (!updated_)
            return std::nullopt;

        return Snapshot{
            name_,
            instruction_,
            prompts_
        };
    }

    void PromptRequest::clear_updated() {
        updated_ = false;
    }

    PromptRequest::Snapshot PromptRequest::get() const
    {
        std::lock_guard<std::mutex> lock(m_);

        return Snapshot{
            name_,
            instruction_,
            prompts_
        };
    }

    void PromptRequest::clear_prompts()
    {
        std::lock_guard<std::mutex> lock(m_);

        prompts_.clear();
        updated_ = true;
    }

    void HostKeyRequest::update(std::string host,
        uint64_t port,
        std::string fingerprint,
        std::string key_type,
        HostKeyState state)
    {
        std::lock_guard<std::mutex> lock(m_);

        host_ = std::move(host);
        port_ = port;
        fingerprint_sha256_b64_ = std::move(fingerprint);
        key_type_ = std::move(key_type);
        state_ = state;

        updated_ = true;
    }

    std::optional<HostKeyRequest::Snapshot> HostKeyRequest::consume_if_updated()
    {
        std::lock_guard<std::mutex> lock(m_);

        if (!updated_)
            return std::nullopt;

        return Snapshot{
            host_,
            port_,
            fingerprint_sha256_b64_,
            key_type_,
            state_
        };
    }

    void HostKeyRequest::clear_updated() {
        updated_ = false;
    }

    HostKeyRequest::Snapshot HostKeyRequest::get() const
    {
        std::lock_guard<std::mutex> lock(m_);

        return Snapshot{
            host_,
            port_,
            fingerprint_sha256_b64_,
            key_type_,
            state_
        };
    }


    void ExecutionOutput::append(std::string text)
    {
        std::lock_guard<std::mutex> lock(m_);

        text_ += text;
        updated_ = true;
    }

    std::optional<ExecutionOutput::Snapshot> ExecutionOutput::consume_if_updated()
    {
        std::lock_guard<std::mutex> lock(m_);

        if (!updated_ || finished_)
            return std::nullopt;

        return Snapshot{ text_, finished_ };
    }

    void ExecutionOutput::clear_updated() {
        updated_ = false;
    }

    ExecutionOutput::Snapshot ExecutionOutput::get() const
    {
        std::lock_guard<std::mutex> lock(m_);
        return Snapshot{ text_ };
    }

    void ExecutionOutput::clear()
    {
        std::lock_guard<std::mutex> lock(m_);

        text_.clear();
        updated_ = true;
    }

    void FileStat::update(std::string name,
        uint64_t size,
        uint64_t time,
        uint64_t downloaded)
    {
        std::lock_guard<std::mutex> lock(m_);

        name_ = std::move(name);
        size_ = size;
        time_ = time;
        downloaded_ = downloaded;

        updated_ = true;
    }

    std::optional<FileStat::Snapshot> FileStat::consume_if_updated()
    {
        std::lock_guard<std::mutex> lock(m_);

        if (!updated_)
            return std::nullopt;

        updated_ = false;

        return Snapshot{
            name_,
            size_,
            time_,
            downloaded_
        };
    }

    FileStat::Snapshot FileStat::get() const
    {
        std::lock_guard<std::mutex> lock(m_);

        return Snapshot{
            name_,
            size_,
            time_,
            downloaded_
        };
    }

    void FileStat::set_downloaded(uint64_t downloaded)
    {
        std::lock_guard<std::mutex> lock(m_);

        downloaded_ = downloaded;
        updated_ = true;
    }


}  // namespace DataModel
}  // namespace RocProfVis

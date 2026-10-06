// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>


namespace RocProfVis
{
namespace View
{

    struct PromptItem
    {
        std::string text;
        bool echo = true;  // false => password-style
    };

    class PromptRequest
    {
    public:
        struct Snapshot
        {
            std::string name;
            std::string instruction;
            std::vector<PromptItem> prompts;
        };

        PromptRequest() = default;
        ~PromptRequest() = default;

        PromptRequest(const PromptRequest&) = delete;
        PromptRequest& operator=(const PromptRequest&) = delete;

        PromptRequest(PromptRequest&&) = default;
        PromptRequest& operator=(PromptRequest&&) = default;

    public:
        void Update(std::string name,
            std::string instruction,
            std::vector<PromptItem> prompts);

        std::optional<Snapshot> ConsumeIfUpdated();

        Snapshot Get() const;

        void ClearPrompts();
        void ClearUpdated();

    private:
        mutable std::mutex m_mutex;

        std::string m_name;
        std::string m_instruction;
        std::vector<PromptItem> m_prompts;

        bool m_updated = false;
    };


    enum class HostKeyState
    {
        NotFound,
        Mismatch
    };

    enum class HostKeyDecision
    {
        Reject,
        TrustOnce,
        TrustPermanently
    };

    class HostKeyRequest
    {
    public:
        struct Snapshot
        {
            std::string host;
            uint64_t port;
            std::string fingerprint_sha256_b64;
            std::string key_type;
            HostKeyState state;
        };

        HostKeyRequest() = default;
        ~HostKeyRequest() = default;

        HostKeyRequest(const HostKeyRequest&) = delete;
        HostKeyRequest& operator=(const HostKeyRequest&) = delete;

        HostKeyRequest(HostKeyRequest&&) = default;
        HostKeyRequest& operator=(HostKeyRequest&&) = default;

    public:
        void Update(std::string host,
            uint64_t port,
            std::string fingerprint,
            std::string key_type,
            HostKeyState state);

        std::optional<Snapshot> ConsumeIfUpdated();

        void ClearUpdated();

        Snapshot Get() const;

    private:
        mutable std::mutex m_mutex;

        std::string m_host;
        uint64_t m_port = 22;
        std::string m_fingerprint_sha256_b64;
        std::string m_key_type;
        HostKeyState m_state = HostKeyState::NotFound;

        bool m_updated = false;
    };


    class ExecutionOutput
    {
    public:
        struct Snapshot
        {
            std::string text;
            bool finished;
        };

        ExecutionOutput() = default;
        ~ExecutionOutput() = default;

        ExecutionOutput(const ExecutionOutput&) = delete;
        ExecutionOutput& operator=(const ExecutionOutput&) = delete;

        ExecutionOutput(ExecutionOutput&&) = default;
        ExecutionOutput& operator=(ExecutionOutput&&) = default;

    public:

        void Append(std::string text);

        std::optional<Snapshot> ConsumeIfUpdated();
        void ClearUpdated();

        Snapshot Get() const;

        void Clear();

        void Finish();

    private:
        mutable std::mutex m_mutex;
        std::string m_text;
        bool m_updated = false;
        bool m_finished = false;
    };


    class FileStat
    {
    public:
        struct Snapshot
        {
            std::string name;
            uint64_t size;
            uint64_t time;
            uint64_t downloaded;
        };

        // Constructors
        FileStat() = default;
        ~FileStat() = default;

        // Non-copyable (recommended due to mutex)
        FileStat(const FileStat&) = delete;
        FileStat& operator=(const FileStat&) = delete;

        // Movable (optional)
        FileStat(FileStat&&) = default;
        FileStat& operator=(FileStat&&) = default;

    public:
        // Update all fields
        void Update(std::string name,
            uint64_t size,
            uint64_t time,
            uint64_t downloaded);

        // Consume update flag
        std::optional<Snapshot> ConsumeIfUpdated();

        // Always get snapshot
        Snapshot Get() const;

        // Partial update
        void SetDownloaded(uint64_t downloaded);

    private:
        mutable std::mutex m_mutex;

        std::string m_name;
        uint64_t m_size = 0;
        uint64_t m_time = 0;
        uint64_t m_downloaded = 0;

        bool m_updated = false;
    };


    class RemoteDir
    {
    public:
        enum FileAttrs
        {
            Directory = 1
        };


        struct FileEntry
        {
            std::string name;
            uint64_t size;
            uint64_t time;
            bool is_dir;
        };


        struct Snapshot
        {
            std::vector<FileEntry> list_dir;
            std::string            path;
        };

        // Constructors
        RemoteDir() = default;
        ~RemoteDir() = default;

        // Non-copyable (recommended due to mutex)
        RemoteDir(const RemoteDir&) = delete;
        RemoteDir& operator=(const RemoteDir&) = delete;

        // Movable (optional)
        RemoteDir(RemoteDir&&) = default;
        RemoteDir& operator=(RemoteDir&&) = default;

    public:
        // Update all fields
        void Update(std::string name,
            uint64_t size,
            uint64_t time,
            uint64_t attrs);

        // Sets the absolute path of the directory these entries belong to.
        void SetPath(std::string path);

        // Consume update flag
        std::optional<Snapshot> ConsumeIfUpdated();

        // Always get snapshot
        Snapshot Get() const;

        void Clear();


    private:
        mutable std::mutex m_mutex;

        std::vector<FileEntry> m_list_dir;
        std::string            m_path;

        bool m_updated = false;
    };

}  // namespace View
}  // namespace RocProfVis

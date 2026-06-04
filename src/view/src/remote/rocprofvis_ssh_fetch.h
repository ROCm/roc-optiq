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
        void update(std::string name,
            std::string instruction,
            std::vector<PromptItem> prompts);

        std::optional<Snapshot> consume_if_updated();

        Snapshot get() const;

        void clear_prompts();
        void clear_updated();

    private:
        mutable std::mutex m_;

        std::string name_;
        std::string instruction_;
        std::vector<PromptItem> prompts_;

        bool updated_ = false;
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
        void update(std::string host,
            uint64_t port,
            std::string fingerprint,
            std::string key_type,
            HostKeyState state);

        std::optional<Snapshot> consume_if_updated();

        void clear_updated();

        Snapshot get() const;

    private:
        mutable std::mutex m_;

        std::string host_;
        uint64_t port_ = 22;
        std::string fingerprint_sha256_b64_;
        std::string key_type_;
        HostKeyState state_ = HostKeyState::NotFound;

        bool updated_ = false;
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

        void append(std::string text);

        std::optional<Snapshot> consume_if_updated();
        void clear_updated();

        Snapshot get() const;

        void clear();

        void finish() { finished_ = true; }

    private:
        mutable std::mutex m_;
        std::string text_;
        bool updated_ = false; 
        bool finished_ = false;
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
        void update(std::string name,
            uint64_t size,
            uint64_t time,
            uint64_t downloaded);

        // Consume update flag
        std::optional<Snapshot> consume_if_updated();

        // Always get snapshot
        Snapshot get() const;

        // Partial update
        void set_downloaded(uint64_t downloaded);

    private:
        mutable std::mutex m_;

        std::string name_;
        uint64_t size_ = 0;
        uint64_t time_ = 0;
        uint64_t downloaded_ = 0;

        bool updated_ = false;
    };


    class RemoteDir
    {
    public:
        enum FileAttrs
        {
            Directory = 1
        };


        struct FileEntry {
            std::string name;           
            uint64_t size;
            uint64_t time;
            bool is_dir;
        };


        struct Snapshot
        {
            std::vector<FileEntry> list_dir;
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
        void update(std::string name,
            uint64_t size,
            uint64_t time,
            uint64_t attrs);

        // Consume update flag
        std::optional<Snapshot> consume_if_updated();

        // Always get snapshot
        Snapshot get() const;

        void clear();


    private:
        mutable std::mutex m_;

        std::vector<FileEntry> m_list_dir;

        bool updated_ = false;
    };

}  // namespace DataModel
}  // namespace RocProfVis

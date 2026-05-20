// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <string>

#include "json.h"

namespace RocProfVis
{
namespace View
{

    class RemoteUri
    {
    public:
        RemoteUri();

        bool SaveToJson();
        bool LoadFromJson();

        const std::array<char, 256>& GetRemoteHostArray() const;
        std::string GetRemoteHostString() const;

        const std::array<char, 16>& GetRemotePortArray() const;
        std::string GetRemotePortString() const;
        int GetRemotePortInt() const;

        const std::array<char, 128>& GetRemoteUserArray() const;
        std::string GetRemoteUserString() const;

        const std::array<char, 256>& GetRemotePasswordArray() const;
        std::string GetRemotePasswordString() const;

        const std::array<char, 1024>& GetRemoteCommandLineArray() const;
        std::string GetRemoteCommandLineString() const;

        const std::array<char, 1024>& GetRemoteResultPathArray() const;
        std::string GetRemoteResultPathString() const;

        const std::array<char, 1024>& GetRemoteIdentityFileArray() const;
        std::string GetRemoteIdentityFileString() const;

        const std::array<char, 256>& GetPassphraseArray() const;
        std::string GetPassphraseString() const;

        char* GetRemoteHostBuffer();
        char* GetRemotePortBuffer();
        char* GetRemoteUserBuffer();
        char* GetRemotePasswordBuffer();
        char* GetRemoteCommandLineBuffer();
        char* GetRemoteResultPathBuffer();
        char* GetRemoteIdentityFileBuffer();
        char* GetPassphraseBuffer();

        size_t GetRemoteHostBufferSize() const;
        size_t GetRemotePortBufferSize() const;
        size_t GetRemoteUserBufferSize() const;
        size_t GetRemotePasswordBufferSize() const;
        size_t GetRemoteCommandLineBufferSize() const;
        size_t GetRemoteResultPathBufferSize() const;
        size_t GetRemoteIdentityFileBufferSize() const;
        size_t GetPassphraseBufferSize() const;

        std::string GetRemoteCacheKey();

        std::string RemoteUri::GetLocalResultPathString();

    private:
        void SetDefaults();
        std::string GetConfigPath();

        template <size_t N>
        static void CopyToArray(
            std::array<char, N>& dst,
            const std::string& src);

        template <size_t N>
        static std::string ToString(
            const std::array<char, N>& value);

        template <size_t N>
        static void LoadField(
            const jt::Json& j,
            const std::string& key,
            std::array<char, N>& out);

    private:
        std::array<char, 256>  m_remote_host{};
        std::array<char, 16>   m_remote_port{};
        std::array<char, 128>  m_remote_user{};
        std::array<char, 256>  m_remote_password{};
        std::array<char, 1024> m_remote_command_line{};
        std::array<char, 1024> m_remote_result_path{};
        std::array<char, 1024> m_remote_identity_file{};
        std::array<char, 256>  m_passphrase{};
    };


}  // namespace DataModel
}  // namespace RocProfVis

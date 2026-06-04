// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <variant>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

struct PromptItem
{
    std::string text;
    bool        echo = true;  // false => password-style
};

struct PromptRequest
{
    std::string             name;
    std::string             instruction;
    std::vector<PromptItem> prompts;
};

enum class HostKeyState
{
    NotFound,
    Mismatch
};

struct HostKeyRequest
{
    std::string  host;
    int          port = 22;
    std::string  fingerprint_sha256_b64;  // user-friendly display
    std::string  key_type;                // "ssh-rsa", "ssh-ed25519", ...
    HostKeyState state = HostKeyState::NotFound;
};

enum class HostKeyDecision
{
    Reject,
    TrustOnce,
    TrustPermanently
};

typedef std::variant<std::monostate, PromptRequest, HostKeyRequest> rocprofvis_controller_user_prompt_t;

struct FileStat
{
    std::string             name;
    uint64_t                size;
    uint64_t                time;
    uint64_t                downloaded;
    uint64_t                attrs;
};


}  // namespace DataModel
}  // namespace RocProfVis

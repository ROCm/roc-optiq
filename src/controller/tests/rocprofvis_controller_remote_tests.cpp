// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The SSH client header pulls in winsock2.h, which must come before anything
// that drags in windows.h.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "remote/rocprofvis_controller_ssh_client.h"
#include "remote/rocprofvis_controller_ssh_known_hosts.h"
#include "rocprofvis_controller.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Tests for the parts of the SSH stack that decide what is trusted and what is
// sent, and that need no server to exercise: how long a host or a prompt
// answer may be before the C ABI mangles it, and what a "trust this host"
// answer writes to the user's known_hosts file.

using RocProfVis::Controller::AppendKnownHostLine;
using RocProfVis::Controller::KnownHostName;
using RocProfVis::Controller::PromptItem;
using RocProfVis::Controller::PromptRequest;
using RocProfVis::Controller::SshConnection;

namespace
{

// How long a test waits on a worker blocked at a prompt before giving up on
// it, and how often it looks.
constexpr std::chrono::seconds      PROMPT_WAIT(10);
constexpr std::chrono::milliseconds PROMPT_POLL_INTERVAL(1);

// Waits until the connection has a prompt waiting for an answer (`pending`
// true) or no longer has one (false). False if the wait ran out first.
bool wait_for_prompt(rocprofvis_handle_t* connection, bool pending)
{
    std::chrono::steady_clock::time_point const deadline =
        std::chrono::steady_clock::now() + PROMPT_WAIT;
    bool matched = false;
    while(!matched && std::chrono::steady_clock::now() < deadline)
    {
        uint64_t status = kRPVControllerSshIdle;
        rocprofvis_controller_get_uint64(connection, kRPVControllerRemoteStatus, 0, &status);
        matched = (status == kRPVControllerSshAuthRequest) == pending;
        if(!matched)
        {
            std::this_thread::sleep_for(PROMPT_POLL_INTERVAL);
        }
    }
    return matched;
}

// A scratch folder that is removed again, standing in for the user's ~/.ssh.
class ScratchFolder
{
public:
    explicit ScratchFolder(char const* name)
        : m_dir(std::filesystem::temp_directory_path() / name)
    {
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
    }

    ~ScratchFolder() { std::filesystem::remove_all(m_dir); }

    ScratchFolder(ScratchFolder const&)            = delete;
    ScratchFolder& operator=(ScratchFolder const&) = delete;

    std::filesystem::path const& Path() const { return m_dir; }

private:
    std::filesystem::path m_dir;
};

std::string read_file(std::filesystem::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void write_file(std::filesystem::path const& path, std::string const& contents)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
}

}  // namespace

// ==================================================================================
// String arguments crossing the C ABI
// ==================================================================================

TEST_CASE("A host name longer than a fixed buffer reaches the connection whole",
          "[remote][abi]")
{
    // 200 bytes: a legal DNS name, and longer than any small fixed buffer.
    // Arguments::GetString leaves a buffer it fills exactly without a
    // terminator, so a copy cut short there would also read whatever follows
    // it in memory as more of the name.
    std::string host;
    for(int label = 0; label < 4; label++)
    {
        host += std::string(49, static_cast<char>('a' + label)) + ".";
    }
    REQUIRE(host.size() == 200);

    rocprofvis_controller_arguments_t* args   = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_array_t*     output = rocprofvis_controller_array_alloc(1);
    REQUIRE(args != nullptr);
    REQUIRE(output != nullptr);
    REQUIRE(rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeHost, 0, host.c_str()) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_uint64(args, kRPVControllerRemoteTypePort, 0, 2222) ==
            kRocProfVisResultSuccess);

    rocprofvis_handle_t* handle = nullptr;
    REQUIRE(rocprofvis_controller_ssh_connection_alloc(args, output) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_get_object(output, kRPVControllerArrayEntryIndexed, 0, &handle) ==
            kRocProfVisResultSuccess);
    REQUIRE(handle != nullptr);

    SshConnection* connection = reinterpret_cast<SshConnection*>(handle);
    CHECK(connection->GetHost() == host);
    CHECK(connection->GetPort() == 2222);

    CHECK(rocprofvis_controller_ssh_connection_free(handle) == kRocProfVisResultSuccess);
    rocprofvis_controller_array_free(output);
    rocprofvis_controller_arguments_free(args);
}

TEST_CASE("A prompt answer longer than a fixed buffer reaches the waiting worker whole",
          "[remote][abi]")
{
    // What the user types at a password or one-time-code prompt crosses the C
    // ABI as a string argument too. Cut short and left unterminated, it would
    // be sent to the server with whatever followed it in memory.
    std::string const answer(300, 'p');

    rocprofvis_controller_arguments_t* args      = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_arguments_t* responses = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_array_t*     output    = rocprofvis_controller_array_alloc(1);
    REQUIRE(args != nullptr);
    REQUIRE(responses != nullptr);
    REQUIRE(output != nullptr);
    REQUIRE(rocprofvis_controller_set_string(args, kRPVControllerRemoteTypeHost, 0,
                                             "gpu-node.example.com") == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_uint64(responses, kRPVControllerUserNumResponses, 0, 1) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_set_string(responses, kRPVControllerUserResponseIndexed, 0,
                                             answer.c_str()) == kRocProfVisResultSuccess);

    rocprofvis_handle_t* handle = nullptr;
    REQUIRE(rocprofvis_controller_ssh_connection_alloc(args, output) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_get_object(output, kRPVControllerArrayEntryIndexed, 0, &handle) ==
            kRocProfVisResultSuccess);
    REQUIRE(handle != nullptr);
    SshConnection* connection = reinterpret_cast<SshConnection*>(handle);

    // The worker's side of a prompt: blocked until the UI answers or cancels.
    PromptItem prompt;
    prompt.text = "Password: ";
    prompt.echo = false;
    PromptRequest request;
    request.prompts.push_back(prompt);
    std::optional<std::vector<std::string>> received;
    std::thread worker([&]() { received = connection->GetSshBridge()->AskPrompts(request); });

    // No REQUIRE until the worker is joined: leaving this scope while it runs
    // would end the test binary. A worker that never got its answer is let go
    // by cancelling the prompt.
    bool const                asked     = wait_for_prompt(handle, true);
    rocprofvis_result_t const submitted =
        asked ? rocprofvis_controller_remote_submit_responses(handle, responses)
              : kRocProfVisResultUnknownError;
    if(!asked || !wait_for_prompt(handle, false))
    {
        rocprofvis_controller_remote_cancel_prompt(handle);
    }
    worker.join();

    CHECK(asked);
    CHECK(submitted == kRocProfVisResultSuccess);
    REQUIRE(received.has_value());
    REQUIRE(received->size() == 1);
    CHECK(received->front() == answer);

    CHECK(rocprofvis_controller_ssh_connection_free(handle) == kRocProfVisResultSuccess);
    rocprofvis_controller_array_free(output);
    rocprofvis_controller_arguments_free(responses);
    rocprofvis_controller_arguments_free(args);
}

// ==================================================================================
// known_hosts
// ==================================================================================

TEST_CASE("A host key is filed under its port", "[remote][known_hosts]")
{
    // The bare name is the port-22 entry, for OpenSSH as much as for Optiq. A
    // key accepted on another port must not be written under it, or whoever
    // holds that key is trusted as the host's own sshd.
    CHECK(KnownHostName("gpu-node.example.com", 22) == "gpu-node.example.com");
    CHECK(KnownHostName("gpu-node.example.com", 2222) == "[gpu-node.example.com]:2222");
    CHECK(KnownHostName("10.0.0.7", 8022) == "[10.0.0.7]:8022");
    CHECK(KnownHostName("fe80::1", 22) == "fe80::1");
    CHECK(KnownHostName("fe80::1", 2222) == "[fe80::1]:2222");
}

TEST_CASE("Trusting a host adds one line and leaves the rest of known_hosts alone",
          "[remote][known_hosts]")
{
    ScratchFolder               ssh_folder("roc-optiq known_hosts test");
    std::filesystem::path const known_hosts = ssh_folder.Path() / "known_hosts";
    std::string const           entry       = "[lab-box]:2222 ecdsa-sha2-nistp256 AAAAE2Vj added by roc-optiq\n";

    SECTION("everything already in the file survives byte for byte")
    {
        // Lines a parse-and-rewrite would drop or reshape: a comment, a CA
        // marker, a hashed host, and a multi-host entry.
        std::string const existing =
            "# hosts for the lab\n"
            "@cert-authority *.example.com ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGNhLWtleQ== lab CA\n"
            "|1|JfKTdBh7rNbXkVAQCRp4OQoPfmI=|USECr3SWf1JUPsms5AqfD5QfxkM= ssh-rsa AAAAB3NzaC1yc2E=\n"
            "alpha,10.0.0.5 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGFscGhh\n";
        write_file(known_hosts, existing);

        REQUIRE(AppendKnownHostLine(known_hosts.string(), entry));
        CHECK(read_file(known_hosts) == existing + entry);
    }

    SECTION("a last line without a newline is not run into")
    {
        std::string const existing = "alpha ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIGFscGhh";
        write_file(known_hosts, existing);

        REQUIRE(AppendKnownHostLine(known_hosts.string(), entry));
        CHECK(read_file(known_hosts) == existing + "\n" + entry);
    }

    SECTION("a missing file and folder are created")
    {
        std::filesystem::path const fresh = ssh_folder.Path() / "new-home" / ".ssh" / "known_hosts";

        REQUIRE(AppendKnownHostLine(fresh.string(), entry));
        CHECK(read_file(fresh) == entry);
    }

    SECTION("a path that cannot be written reports failure")
    {
        // The folder itself, which cannot be opened as a file.
        CHECK_FALSE(AppendKnownHostLine(ssh_folder.Path().string(), entry));
    }
}

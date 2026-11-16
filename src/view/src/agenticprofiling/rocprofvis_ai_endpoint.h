// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace RocProfVis
{
namespace View
{

// Which endpoint the URL implies. Everything that differs between them - the
// path, the auth header, and the token-limit field - keys off this, so the two
// shapes are described in one place rather than sniffed at each use.
enum class EndpointFlavour
{
    kOpenAi,
    kAzure
};

// Azure-style bases name the deployment in the path, which an /azure, /engines/
// or /openai/deployments segment identifies.
EndpointFlavour
FlavourFromUrl(const std::string& url);

// Turns the configured base into the chat-completions URL. Any query string
// comes off first so the path suffix is not buried inside it. Empty return
// means an Azure base needs a deployment and the Model field was blank.
std::string
ChatCompletionsUrl(std::string base_url, const std::string& model,
                   EndpointFlavour flavour);

// httplib::Client wants the origin ("https://host:port") and the path
// ("/v1/chat/completions") as separate arguments. False when there is no scheme.
bool
SplitUrl(const std::string& url, std::string& origin_out, std::string& path_out);

}  // namespace View
}  // namespace RocProfVis

// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_endpoint.h"

#include <cstring>
#include <string>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr char ASSISTANT_CHAT_COMPLETIONS_SUFFIX[]  = "/chat/completions";
constexpr char ASSISTANT_AZURE_OPENAI_API_VERSION[] = "2024-09-01-preview";

// True when the value ends with the given suffix.
bool
EndsWith(const std::string& value, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    return value.size() >= n && value.compare(value.size() - n, n, suffix) == 0;
}

// True when the base URL already names the chat-completions route.
bool
HasCompletionsSuffix(const std::string& url)
{
    return EndsWith(url, ASSISTANT_CHAT_COMPLETIONS_SUFFIX);
}

// Azure's /openai deployments want apiVersion on the query when the user did
// not already supply one. /azure/engines does not.
void
EnsureAzureOpenAiVersion(const std::string& path, std::string& query)
{
    if(!query.empty() || path.find("/openai/deployments/") == std::string::npos)
    {
        return;
    }
    query  = "?apiVersion=";
    query += ASSISTANT_AZURE_OPENAI_API_VERSION;
}

// Undoes a /chat/completions that was appended to an Azure base without the
// deployment in between. Azure answers that with a 404, so a user who pasted
// the failing URL back into settings would otherwise be stuck with it.
void
StripDeploymentlessCompletions(std::string& base_url)
{
    if(!HasCompletionsSuffix(base_url))
    {
        return;
    }
    std::string without_suffix = base_url;
    without_suffix.resize(without_suffix.size() -
                          std::strlen(ASSISTANT_CHAT_COMPLETIONS_SUFFIX));
    if(EndsWith(without_suffix, "/azure") || EndsWith(without_suffix, "/openai") ||
       EndsWith(without_suffix, "/azure/engines") ||
       EndsWith(without_suffix, "/openai/deployments"))
    {
        base_url = without_suffix;
    }
}

// Inserts the deployment segment an Azure base is missing, e.g. /azure becomes
// /azure/engines/<model>. Empty return means the model field was not filled in.
bool
AppendAzureDeployment(std::string& base_url, const std::string& model)
{
    // A base that already names the deployment only needs the route appended.
    if(base_url.find("/engines/") != std::string::npos ||
       base_url.find("/deployments/") != std::string::npos)
    {
        return true;
    }

    const bool azure_base              = EndsWith(base_url, "/azure");
    const bool openai_base             = EndsWith(base_url, "/openai");
    const bool azure_engines_base      = EndsWith(base_url, "/azure/engines");
    const bool openai_deployments_base = EndsWith(base_url, "/openai/deployments");
    if(!azure_base && !openai_base && !azure_engines_base && !openai_deployments_base)
    {
        return true;
    }
    if(model.empty())
    {
        return false;
    }

    if(azure_base)
    {
        base_url += "/engines/";
    }
    else if(openai_base)
    {
        base_url += "/deployments/";
    }
    else
    {
        base_url += "/";
    }
    base_url += model;
    return true;
}

}  // namespace

EndpointFlavour
FlavourFromUrl(const std::string& url)
{
    // Only the path counts: the scheme's "//" would otherwise make a host named
    // azure-something match "/azure" and get the Azure auth header.
    std::string origin;
    std::string path = url;
    SplitUrl(url, origin, path);
    const bool azure = path.find("/azure") != std::string::npos ||
                       path.find("/engines/") != std::string::npos ||
                       path.find("/openai/deployments") != std::string::npos;
    return azure ? EndpointFlavour::kAzure : EndpointFlavour::kOpenAi;
}

std::string
ChatCompletionsUrl(std::string base_url, const std::string& model,
                   EndpointFlavour flavour)
{
    std::string  query;
    const size_t query_start = base_url.find('?');
    if(query_start != std::string::npos)
    {
        query = base_url.substr(query_start);
        base_url.resize(query_start);
    }

    while(!base_url.empty() && base_url.back() == '/')
    {
        base_url.pop_back();
    }

    if(flavour == EndpointFlavour::kAzure)
    {
        StripDeploymentlessCompletions(base_url);
    }

    if(!HasCompletionsSuffix(base_url))
    {
        if(flavour == EndpointFlavour::kAzure &&
           !AppendAzureDeployment(base_url, model))
        {
            return std::string();
        }
        base_url += ASSISTANT_CHAT_COMPLETIONS_SUFFIX;
    }

    EnsureAzureOpenAiVersion(base_url, query);
    return base_url + query;
}

bool
SplitUrl(const std::string& url, std::string& origin_out, std::string& path_out)
{
    const std::string separator  = "://";
    const size_t      scheme_end = url.find(separator);
    if(scheme_end == std::string::npos)
    {
        return false;
    }

    const size_t path_start = url.find('/', scheme_end + separator.size());
    origin_out              = url.substr(0, path_start);
    path_out = (path_start == std::string::npos) ? "/" : url.substr(path_start);
    return true;
}

}  // namespace View
}  // namespace RocProfVis

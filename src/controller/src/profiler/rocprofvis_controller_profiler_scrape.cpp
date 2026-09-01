// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_scrape.h"

#include "spdlog/spdlog.h"

#include <cctype>
#include <filesystem>
#include <sstream>

namespace RocProfVis
{
namespace Controller
{

void strip_ansi(std::string& text)
{
    std::string out;
    out.reserve(text.size());

    for (size_t i = 0; i < text.size();)
    {
        unsigned char const c = static_cast<unsigned char>(text[i]);
        if (c == 0x1b)
        {
            if (i + 1 < text.size() && text[i + 1] == '[')
            {
                i += 2;
                while (i < text.size())
                {
                    unsigned char const f = static_cast<unsigned char>(text[i]);
                    ++i;
                    if (f >= 0x40 && f <= 0x7e)
                    {
                        break;
                    }
                }
                continue;
            }
            ++i;
            if (i < text.size())
            {
                ++i;
            }
            continue;
        }
        out.push_back(text[i]);
        ++i;
    }

    text.swap(out);
}

bool is_absolute_path(std::string const& path)
{
    if (path.empty())
    {
        return false;
    }
#ifdef _WIN32
    return std::filesystem::path(path).is_absolute();
#else
    return path.front() == '/';
#endif
}

std::string resolve_scraped_path(std::string const& value, std::string const& working_directory)
{
    if (value.empty() || is_absolute_path(value) || working_directory.empty())
    {
        return value;
    }

    std::filesystem::path combined = std::filesystem::path(working_directory) / value;
    return combined.lexically_normal().string();
}

rocprofvis_result_t ProfilerScrapeEngine::Compile(std::vector<ProfilerStageSpec> const& stages)
{
    m_rules.clear();
    m_slots.clear();
    m_remainder.clear();
    m_current_stage = 0;

    m_rules.resize(stages.size());

    try
    {
        for (uint32_t stage_index = 0; stage_index < stages.size(); ++stage_index)
        {
            ProfilerStageSpec const& stage = stages[stage_index];
            std::map<std::string, size_t> key_rule_count;

            for (ProfilerScrapeRuleSpec const& spec : stage.scrape_rules)
            {
                if (spec.key.empty() || spec.pattern.empty())
                {
                    spdlog::error("Scrape rule missing key or pattern (stage {})", stage_index);
                    return kRocProfVisResultInvalidArgument;
                }

                m_slots.emplace(ProfilerScrapeSlotKey(stage_index, spec.key),
                                ProfilerScrapeSlot());

                CompiledRule rule;
                rule.key          = spec.key;
                rule.group        = spec.group;
                rule.policy       = spec.policy;
                rule.regex        = std::regex(spec.pattern, std::regex::ECMAScript);
                rule.index_in_key = key_rule_count[spec.key];
                key_rule_count[spec.key] += 1;

                if (rule.group > static_cast<uint32_t>(rule.regex.mark_count()))
                {
                    spdlog::error("Scrape rule '{}' group {} exceeds pattern mark count {}",
                                  spec.key, spec.group, rule.regex.mark_count());
                    return kRocProfVisResultInvalidArgument;
                }

                m_rules[stage_index].push_back(std::move(rule));
            }

            for (auto const& kv : stage.expected)
            {
                if (kv.first.empty())
                {
                    return kRocProfVisResultInvalidArgument;
                }
                // An expected value with no rule is still a usable answer: it
                // is simply never cross-checked.
                auto slot_it =
                    m_slots.emplace(ProfilerScrapeSlotKey(stage_index, kv.first),
                                    ProfilerScrapeSlot())
                        .first;
                slot_it->second.expected     = kv.second;
                slot_it->second.has_expected = true;
            }
        }
    }
    catch (std::regex_error const& e)
    {
        spdlog::error("Invalid scrape-rule pattern: {}", e.what());
        m_rules.clear();
        m_slots.clear();
        return kRocProfVisResultInvalidArgument;
    }

    return kRocProfVisResultSuccess;
}

void ProfilerScrapeEngine::SetDiagnosticSink(std::string* output_text)
{
    m_diagnostic_sink = output_text;
}

void ProfilerScrapeEngine::inject_diagnostic(std::string const& text)
{
    spdlog::error("{}", text);
    if (m_diagnostic_sink != nullptr)
    {
        if (!m_diagnostic_sink->empty() && m_diagnostic_sink->back() != '\n')
        {
            m_diagnostic_sink->push_back('\n');
        }
        *m_diagnostic_sink += "[optiq] ";
        *m_diagnostic_sink += text;
        m_diagnostic_sink->push_back('\n');
    }
}

void ProfilerScrapeEngine::BeginStage(uint32_t stage_index)
{
    m_current_stage = stage_index;
    m_remainder.clear();
}

void ProfilerScrapeEngine::Feed(std::string const& chunk)
{
    if (chunk.empty() || m_current_stage >= m_rules.size())
    {
        return;
    }

    m_remainder += chunk;

    size_t start = 0;
    while (start < m_remainder.size())
    {
        size_t const nl = m_remainder.find('\n', start);
        if (nl == std::string::npos)
        {
            break;
        }
        std::string line = m_remainder.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        match_line(line);
        start = nl + 1;
    }

    if (start > 0)
    {
        m_remainder.erase(0, start);
    }
}

void ProfilerScrapeEngine::match_line(std::string const& raw_line)
{
    std::string line = raw_line;
    strip_ansi(line);

    if (line.size() > kProfilerScrapeLineMaxBytes)
    {
        return;
    }

    if (m_current_stage >= m_rules.size())
    {
        return;
    }

    for (CompiledRule& rule : m_rules[m_current_stage])
    {
        if (rule.disabled)
        {
            continue;
        }

        auto slot_it = m_slots.find(ProfilerScrapeSlotKey(m_current_stage, rule.key));
        if (slot_it == m_slots.end())
        {
            continue;
        }
        ProfilerScrapeSlot& slot = slot_it->second;

        if (slot.has_match && slot.winning_rule < rule.index_in_key)
        {
            continue;
        }
        if (slot.has_match && slot.winning_rule == rule.index_in_key &&
            rule.policy == ScrapePolicy::kFirstMatch)
        {
            continue;
        }

        std::smatch match;
        try
        {
            if (!std::regex_search(line, match, rule.regex))
            {
                continue;
            }
        }
        catch (std::regex_error const& e)
        {
            rule.disabled = true;
            slot.status   = kRPVProfilerScrapeRuleFailed;
            if (!rule.logged_error)
            {
                rule.logged_error = true;
                std::ostringstream oss;
                oss << "Scrape rule '" << rule.key << "' failed during matching: " << e.what();
                inject_diagnostic(oss.str());
            }
            continue;
        }
        catch (...)
        {
            rule.disabled = true;
            slot.status   = kRPVProfilerScrapeRuleFailed;
            if (!rule.logged_error)
            {
                rule.logged_error = true;
                inject_diagnostic("Scrape rule '" + rule.key +
                                  "' failed during matching (unknown error).");
            }
            continue;
        }

        if (rule.group >= match.size() || !match[rule.group].matched)
        {
            continue;
        }

        slot.value        = match[rule.group].str();
        slot.has_match    = true;
        slot.winning_rule = rule.index_in_key;
        slot.status       = kRPVProfilerScrapeResolved;
    }
}

void ProfilerScrapeEngine::EndStage(std::string const& working_directory)
{
    if (!m_remainder.empty())
    {
        std::string line = m_remainder;
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        match_line(line);
        m_remainder.clear();
    }

    for (auto& kv : m_slots)
    {
        if (kv.first.first != m_current_stage)
        {
            continue;
        }

        std::string const&  key  = kv.first.second;
        ProfilerScrapeSlot& slot = kv.second;

        if (slot.status == kRPVProfilerScrapeResolved)
        {
            slot.value = resolve_scraped_path(slot.value, working_directory);
            if (slot.has_expected &&
                slot.value != resolve_scraped_path(slot.expected, working_directory))
            {
                spdlog::warn("Scraped '{}' is '{}' but the expected value was '{}'; "
                             "using the scraped value",
                             key, slot.value, slot.expected);
            }
            continue;
        }

        if (slot.status == kRPVProfilerScrapeRuleFailed || slot.status == kRPVProfilerScrapePending)
        {
            if (slot.has_expected)
            {
                spdlog::warn("Key '{}' was not scraped; using unverified expected value '{}'", key,
                             slot.expected);
                // Resolved the same way a scraped value would be, so a caller
                // cannot tell the two apart by shape.
                slot.value  = resolve_scraped_path(slot.expected, working_directory);
                slot.status = kRPVProfilerScrapeResolved;
            }
            else if (slot.status == kRPVProfilerScrapePending)
            {
                slot.status = kRPVProfilerScrapeUnmatched;
            }
        }
    }
}

void ProfilerScrapeEngine::SkipRemainingFrom(uint32_t first_unstarted_stage)
{
    for (auto& kv : m_slots)
    {
        if (kv.first.first >= first_unstarted_stage &&
            kv.second.status == kRPVProfilerScrapePending)
        {
            kv.second.status = kRPVProfilerScrapeStageSkipped;
        }
    }
}

ProfilerScrapeSlot const* ProfilerScrapeEngine::find_latest(std::string const& key) const
{
    ProfilerScrapeSlot const* latest = nullptr;
    for (auto const& kv : m_slots)
    {
        // m_slots is ordered by (stage, key), so the last hit is the highest
        // stage that declares this key.
        if (kv.first.second == key)
        {
            latest = &kv.second;
        }
    }
    return latest;
}

rocprofvis_result_t ProfilerScrapeEngine::GetValue(std::string const& key, std::string& out) const
{
    ProfilerScrapeSlot const* slot = find_latest(key);
    if (slot == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    switch (slot->status)
    {
        case kRPVProfilerScrapeResolved:
            out = slot->value;
            return kRocProfVisResultSuccess;
        case kRPVProfilerScrapePending:
            return kRocProfVisResultPending;
        default:
            return kRocProfVisResultNotAvailable;
    }
}

rocprofvis_profiler_scrape_status_t ProfilerScrapeEngine::GetStatus(std::string const& key) const
{
    ProfilerScrapeSlot const* slot = find_latest(key);
    if (slot == nullptr)
    {
        return kRPVProfilerScrapeUnmatched;
    }
    return slot->status;
}

bool ProfilerScrapeEngine::HasKey(std::string const& key) const
{
    return find_latest(key) != nullptr;
}

bool ProfilerScrapeEngine::GetStageValue(uint32_t stage_index, std::string const& key,
                                         std::string& out) const
{
    auto it = m_slots.find(ProfilerScrapeSlotKey(stage_index, key));
    if (it == m_slots.end() || it->second.status != kRPVProfilerScrapeResolved)
    {
        return false;
    }
    out = it->second.value;
    return true;
}

rocprofvis_profiler_scrape_status_t ProfilerScrapeEngine::GetStageStatus(
    uint32_t stage_index, std::string const& key) const
{
    auto it = m_slots.find(ProfilerScrapeSlotKey(stage_index, key));
    if (it == m_slots.end())
    {
        return kRPVProfilerScrapeUnmatched;
    }
    return it->second.status;
}

void ProfilerScrapeEngine::SetValue(uint32_t stage_index, std::string const& key,
                                    std::string const& value)
{
    ProfilerScrapeSlot& slot = m_slots[ProfilerScrapeSlotKey(stage_index, key)];
    slot.value               = value;
    slot.has_match           = true;
    slot.status              = kRPVProfilerScrapeResolved;
}

namespace
{

bool parse_placeholder(std::string const& token, size_t pos, uint32_t& stage_index,
                       std::string& key, size_t& end)
{
    // {stage<N>.<key>}
    static char const kPrefix[] = "{stage";
    if (token.compare(pos, 6, kPrefix) != 0)
    {
        return false;
    }

    size_t i = pos + 6;
    if (i >= token.size() || !std::isdigit(static_cast<unsigned char>(token[i])))
    {
        return false;
    }

    uint32_t stage = 0;
    while (i < token.size() && std::isdigit(static_cast<unsigned char>(token[i])))
    {
        stage = stage * 10u + static_cast<uint32_t>(token[i] - '0');
        ++i;
    }
    if (i >= token.size() || token[i] != '.')
    {
        return false;
    }
    ++i;

    size_t key_start = i;
    if (i >= token.size() ||
        !(std::isalpha(static_cast<unsigned char>(token[i])) || token[i] == '_'))
    {
        return false;
    }
    ++i;
    while (i < token.size() &&
           (std::isalnum(static_cast<unsigned char>(token[i])) || token[i] == '_'))
    {
        ++i;
    }
    if (i >= token.size() || token[i] != '}')
    {
        return false;
    }

    stage_index = stage;
    key.assign(token, key_start, i - key_start);
    end = i + 1;
    return true;
}

std::string placeholder_error_message(std::string const& key,
                                      rocprofvis_profiler_scrape_status_t status)
{
    switch (status)
    {
        case kRPVProfilerScrapeUnmatched:
            return "The profiler finished but did not report '" + key +
                   "'. The profiler's output format may have changed.";
        case kRPVProfilerScrapeRuleFailed:
            return "Optiq could not parse '" + key +
                   "' (internal pattern error).";
        case kRPVProfilerScrapeStageSkipped:
            return "Placeholder '" + key + "' refers to a stage that did not run.";
        case kRPVProfilerScrapePending:
            return "Placeholder '" + key + "' refers to a value that is not yet available.";
        default:
            return "Placeholder '" + key + "' could not be resolved.";
    }
}

} // namespace

rocprofvis_result_t resolve_stage_placeholders(std::vector<std::string>&    argv,
                                               ProfilerScrapeEngine const& scrape,
                                               std::string&                 error_message)
{
    error_message.clear();

    for (std::string& token : argv)
    {
        std::string resolved;
        resolved.reserve(token.size());
        size_t pos = 0;
        while (pos < token.size())
        {
            size_t const brace = token.find("{stage", pos);
            if (brace == std::string::npos)
            {
                resolved.append(token, pos, std::string::npos);
                break;
            }
            resolved.append(token, pos, brace - pos);

            uint32_t    stage_index = 0;
            std::string key;
            size_t      end = 0;
            if (!parse_placeholder(token, brace, stage_index, key, end))
            {
                resolved.push_back(token[brace]);
                pos = brace + 1;
                continue;
            }

            std::string value;
            if (!scrape.GetStageValue(stage_index, key, value))
            {
                error_message =
                    placeholder_error_message(key, scrape.GetStageStatus(stage_index, key));
                return kRocProfVisResultInvalidArgument;
            }
            resolved += value;
            pos = end;
        }
        token.swap(resolved);
    }

    return kRocProfVisResultSuccess;
}

} // namespace Controller
} // namespace RocProfVis

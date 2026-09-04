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
    m_stage_open    = false;

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

void ProfilerScrapeEngine::sink_line(std::string const& text)
{
    if (m_diagnostic_sink == nullptr)
    {
        return;
    }
    if (!m_diagnostic_sink->empty() && m_diagnostic_sink->back() != '\n')
    {
        m_diagnostic_sink->push_back('\n');
    }
    *m_diagnostic_sink += "[optiq] ";
    *m_diagnostic_sink += text;
    m_diagnostic_sink->push_back('\n');
}

void ProfilerScrapeEngine::inject_diagnostic(std::string const& text)
{
    spdlog::error("{}", text);
    sink_line(text);
}

void ProfilerScrapeEngine::BeginStage(uint32_t stage_index)
{
    m_current_stage = stage_index;
    m_remainder.clear();
    m_line_overflow = false;
    m_lines_skipped = 0;
    m_stage_open    = true;
}

void ProfilerScrapeEngine::Feed(std::string const& chunk)
{
    if (chunk.empty() || !m_stage_open || m_current_stage >= m_rules.size())
    {
        return;
    }

    m_remainder += chunk;

    size_t start = 0;
    while (start < m_remainder.size())
    {
        // '\r' ends a line as much as '\n' does. Not for the profilers' own
        // output - both log whole lines - but the profiled application shares
        // this stream, and a progress bar that redraws in place is ordinary in
        // the ML workloads this tool profiles. Treating only '\n' as a
        // terminator would make such a run one unbounded line.
        size_t const term = m_remainder.find_first_of("\r\n", start);
        if (term == std::string::npos)
        {
            break;
        }
        // A '\r' at the very end may be the first half of a CRLF split across
        // chunks. Wait for the next byte rather than emitting a short line and
        // then a spurious empty one.
        if (m_remainder[term] == '\r' && term + 1 >= m_remainder.size())
        {
            break;
        }

        emit_line(m_remainder.substr(start, term - start));

        start = term + 1;
        if (m_remainder[term] == '\r' && m_remainder[start] == '\n')
        {
            ++start;
        }
    }

    if (start > 0)
    {
        m_remainder.erase(0, start);
    }

    // Past the cap this line can never match, so stop holding it. Dropping the
    // tail bounds the buffer at one line rather than letting output with no
    // terminator at all grow for the length of the run.
    if (m_remainder.size() > kProfilerScrapeLineMaxBytes)
    {
        m_remainder.clear();
        m_line_overflow = true;
    }
}

void ProfilerScrapeEngine::emit_line(std::string const& line)
{
    if (m_line_overflow)
    {
        // The head of this line was already discarded; the tail on its own
        // would match rules against a fragment.
        m_line_overflow = false;
        ++m_lines_skipped;
        return;
    }
    match_line(line);
}

void ProfilerScrapeEngine::match_line(std::string const& raw_line)
{
    std::string line = raw_line;
    strip_ansi(line);

    if (line.size() > kProfilerScrapeLineMaxBytes)
    {
        ++m_lines_skipped;
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
    if (!m_stage_open)
    {
        return;
    }
    m_stage_open = false;

    if (!m_remainder.empty())
    {
        std::string line = m_remainder;
        if (line.back() == '\r')
        {
            line.pop_back();
        }
        emit_line(line);
        m_remainder.clear();
    }
    if (m_line_overflow)
    {
        // Output that ran past the cap and then stopped without a terminator,
        // so no later line arrived to account for it.
        ++m_lines_skipped;
        m_line_overflow = false;
    }

    std::string unmatched_keys;

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
                if (!unmatched_keys.empty())
                {
                    unmatched_keys += "', '";
                }
                unmatched_keys += key;
            }
        }
    }

    // Long lines are ordinary on their own - compute prints demangled kernel
    // names - so they are only worth reporting when something also went
    // missing. Saying it once per stage keeps a chatty target from burying the
    // rest of the log.
    if (m_lines_skipped > 0 && !unmatched_keys.empty())
    {
        std::string const text =
            "'" + unmatched_keys + "' not found in the profiler output, and " +
            std::to_string(m_lines_skipped) + " line(s) were too long to scan (over " +
            std::to_string(kProfilerScrapeLineMaxBytes) +
            " bytes); the value may have been on one of them";
        spdlog::warn("{}", text);
        sink_line(text);
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

rocprofvis_result_t ProfilerScrapeEngine::GetStatus(
    std::string const& key, rocprofvis_profiler_scrape_status_t& out) const
{
    ProfilerScrapeSlot const* slot = find_latest(key);
    if (slot == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }
    out = slot->status;
    return kRocProfVisResultSuccess;
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

rocprofvis_result_t ProfilerScrapeEngine::GetStageStatus(
    uint32_t stage_index, std::string const& key,
    rocprofvis_profiler_scrape_status_t& out) const
{
    auto it = m_slots.find(ProfilerScrapeSlotKey(stage_index, key));
    if (it == m_slots.end())
    {
        return kRocProfVisResultInvalidArgument;
    }
    out = it->second.status;
    return kRocProfVisResultSuccess;
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
                rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
                if (scrape.GetStageStatus(stage_index, key, status) !=
                    kRocProfVisResultSuccess)
                {
                    // No rule in that stage produces this key at all, so this
                    // is a bad placeholder rather than missing tool output.
                    error_message = "Placeholder '{stage" + std::to_string(stage_index) + "." +
                                    key + "}' names a value that no stage produces.";
                }
                else
                {
                    error_message = placeholder_error_message(key, status);
                }
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

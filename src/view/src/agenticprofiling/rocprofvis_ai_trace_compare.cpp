// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_trace_compare.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{

// Below this a change prints as 0.0%, rather than as a signed -0.0%.
constexpr double RUN_CHANGE_ROUNDING_PERCENT = 0.05;
constexpr double RUN_CHANGE_PERCENT_SCALE    = 100.0;

std::string
FormatDecimal(double value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    std::string text = out.str();
    while(!text.empty() && text.back() == '0')
    {
        text.pop_back();
    }
    if(!text.empty() && text.back() == '.')
    {
        text.pop_back();
    }
    return text == "-0" ? "0" : text;
}

bool
IsKnown(const RunDiffRow& row)
{
    return row.state != RunDiffState::kHiddenInTarget &&
           row.state != RunDiffState::kHiddenInBaseline;
}

uint64_t
ChangeSize(const RunDiffRow& row)
{
    return row.target_ns > row.baseline_ns ? row.target_ns - row.baseline_ns
                                           : row.baseline_ns - row.target_ns;
}

std::string
SignedChangeText(uint64_t baseline, uint64_t target)
{
    if(target > baseline)
    {
        return "+" + std::to_string(target - baseline);
    }
    if(target < baseline)
    {
        return "-" + std::to_string(baseline - target);
    }
    return "0";
}

std::string
PercentChangeText(uint64_t baseline, uint64_t target)
{
    const double percent = (static_cast<double>(target) - static_cast<double>(baseline)) /
                           static_cast<double>(baseline) * RUN_CHANGE_PERCENT_SCALE;
    if(std::fabs(percent) < RUN_CHANGE_ROUNDING_PERCENT)
    {
        return "0.0%";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%+.1f%%", percent);
    return std::string(buffer);
}

std::string
FormatRunRateChange(double baseline, double target)
{
    const double delta = target - baseline;
    std::string  text  = FormatDecimal(baseline) + " -> " + FormatDecimal(target) + " (";
    if(delta > 0.0)
    {
        text += "+";
    }
    text += FormatDecimal(delta);
    if(baseline > 0.0)
    {
        const double percent = delta / baseline * RUN_CHANGE_PERCENT_SCALE;
        if(std::fabs(percent) < RUN_CHANGE_ROUNDING_PERCENT)
        {
            text += ", 0.0%";
        }
        else
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), ", %+.1f%%", percent);
            text += buffer;
        }
    }
    text += ")";
    return text;
}

}  // namespace

std::vector<RunDiffRow>
DiffRunCategory(const RunCategoryFigures& baseline, const RunCategoryFigures& target)
{
    std::vector<RunDiffRow> rows;
    rows.reserve(baseline.events.size() + target.events.size());
    std::unordered_map<std::string, const RunEventFigures*> target_by_name;
    target_by_name.reserve(target.events.size());
    for(const RunEventFigures& event : target.events)
    {
        target_by_name.emplace(event.name, &event);
    }
    std::unordered_set<std::string> baseline_names;
    baseline_names.reserve(baseline.events.size());
    for(const RunEventFigures& event : baseline.events)
    {
        baseline_names.insert(event.name);
        RunDiffRow row;
        row.name           = event.name;
        row.baseline_ns    = event.total_ns;
        row.baseline_calls = event.calls;
        const std::unordered_map<std::string, const RunEventFigures*>::const_iterator
            match = target_by_name.find(event.name);
        if(match != target_by_name.end())
        {
            row.target_ns    = match->second->total_ns;
            row.target_calls = match->second->calls;
            row.state        = RunDiffState::kBoth;
        }
        else
        {
            // Absent from a list that was cut short is not the same as absent
            // from the run: the name may be part of what "Others" totals.
            row.state =
                target.truncated ? RunDiffState::kHiddenInTarget : RunDiffState::kGone;
        }
        rows.push_back(row);
    }
    for(const RunEventFigures& event : target.events)
    {
        if(baseline_names.count(event.name) > 0)
        {
            continue;
        }
        RunDiffRow row;
        row.name         = event.name;
        row.target_ns    = event.total_ns;
        row.target_calls = event.calls;
        row.state =
            baseline.truncated ? RunDiffState::kHiddenInBaseline : RunDiffState::kNew;
        rows.push_back(row);
    }

    std::stable_sort(rows.begin(), rows.end(),
                     [](const RunDiffRow& a, const RunDiffRow& b) {
                         if(IsKnown(a) != IsKnown(b))
                         {
                             return IsKnown(a);
                         }
                         return ChangeSize(a) > ChangeSize(b);
                     });
    return rows;
}

std::string
FormatRunChange(uint64_t baseline, uint64_t target)
{
    std::string text = std::to_string(baseline) + " -> " + std::to_string(target) + " (" +
                       SignedChangeText(baseline, target);
    if(baseline > 0)
    {
        text += ", " + PercentChangeText(baseline, target);
    }
    text += ")";
    return text;
}

std::string
FormatRunCategory(const char* label, const RunCategoryFigures& baseline,
                  const RunCategoryFigures& target, size_t limit)
{
    std::ostringstream out;
    if(baseline.events.empty() && target.events.empty() && baseline.total_ns == 0 &&
       target.total_ns == 0)
    {
        out << label << ": none in either run\n";
        return out.str();
    }

    out << label << ": total_ns " << FormatRunChange(baseline.total_ns, target.total_ns)
        << "  calls " << baseline.calls << " -> " << target.calls << "\n";

    const std::vector<RunDiffRow> rows  = DiffRunCategory(baseline, target);
    const size_t                  shown = std::min(rows.size(), limit);
    for(size_t i = 0; i < shown; ++i)
    {
        const RunDiffRow& row = rows[i];
        out << "  " << (i + 1) << ". " << row.name << "  ";
        switch(row.state)
        {
            case RunDiffState::kBoth:
            {
                out << "total_ns " << FormatRunChange(row.baseline_ns, row.target_ns)
                    << "  calls " << row.baseline_calls << " -> " << row.target_calls;
                // A total falls when the work is cut or when each call gets
                // cheaper, and only the time per call tells the two apart.
                if(row.baseline_calls != row.target_calls && row.baseline_calls > 0 &&
                   row.target_calls > 0)
                {
                    out << "  per_call_ns "
                        << FormatRunRateChange(
                               static_cast<double>(row.baseline_ns) /
                                   static_cast<double>(row.baseline_calls),
                               static_cast<double>(row.target_ns) /
                                   static_cast<double>(row.target_calls));
                }
                break;
            }
            case RunDiffState::kGone:
            {
                out << "gone from target (baseline total_ns " << row.baseline_ns
                    << ", calls " << row.baseline_calls << ")";
                break;
            }
            case RunDiffState::kNew:
            {
                out << "new in target (total_ns " << row.target_ns << ", calls "
                    << row.target_calls << ")";
                break;
            }
            case RunDiffState::kHiddenInTarget:
            {
                out << "change unknown: baseline total_ns " << row.baseline_ns
                    << ", and the target keeps this name inside Others";
                break;
            }
            case RunDiffState::kHiddenInBaseline:
            {
                out << "change unknown: target total_ns " << row.target_ns
                    << ", and the baseline keeps this name inside Others";
                break;
            }
        }
        out << "\n";
    }
    if(rows.size() > shown)
    {
        out << "  ... " << (rows.size() - shown) << " more names not shown\n";
    }
    return out.str();
}

}  // namespace View
}  // namespace RocProfVis

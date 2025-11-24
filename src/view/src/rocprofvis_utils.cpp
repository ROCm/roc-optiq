// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_utils.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

std::string
RocProfVis::View::nanosecond_to_str(double time_point_ns, bool include_units) {
    std::ostringstream oss;
    oss << static_cast<uint64_t>(time_point_ns);
    if(include_units)
    {
        oss << " ns";
    }
    return oss.str();
}

std::string
RocProfVis::View::nanosecond_to_us_str(double ns, bool include_units)
{
    if(!std::isfinite(ns)) return "NaN";
    uint64_t           ns_uint = static_cast<uint64_t>(ns);
    uint64_t           us_uint = ns_uint / TimeConstants::ns_per_us;
    uint64_t           ns_rem  = ns_uint % TimeConstants::ns_per_us;
    std::ostringstream oss;
    oss << us_uint << '.' << std::setw(3) << std::setfill('0') << ns_rem;
    if(include_units)
    {
        oss << " us";
    }
    return oss.str();
}

std::string
RocProfVis::View::nanosecond_to_ms_str(double ns, bool include_units)
{
    if(!std::isfinite(ns)) return "NaN";
    uint64_t           ns_uint = static_cast<uint64_t>(ns);
    uint64_t           ms_uint = ns_uint / TimeConstants::ns_per_ms;
    uint64_t           ns_rem  = ns_uint % TimeConstants::ns_per_ms;
    std::ostringstream oss;
    oss << ms_uint << '.' << std::setw(6) << std::setfill('0') << ns_rem;
    if(include_units)
    {
        oss << " ms";
    }
    return oss.str();
}

std::string
RocProfVis::View::nanosecond_to_s_str(double ns, bool include_units)
{
    if(!std::isfinite(ns)) return "NaN";
    uint64_t           ns_uint = static_cast<uint64_t>(ns);
    uint64_t           s_uint  = ns_uint / TimeConstants::ns_per_s;
    uint64_t           ns_rem  = ns_uint % TimeConstants::ns_per_s;
    std::ostringstream oss;
    oss << s_uint << '.' << std::setw(9) << std::setfill('0') << ns_rem;
    if(include_units)
    { 
        oss << " s";
    }
    return oss.str();
}

std::string
RocProfVis::View::nanosecond_to_timecode_str(double time_point_ns,
                                             bool   condensed /*= true*/,
                                             bool   round_before_cast /*= false*/)
{
    // Handle non-finite cases first
    if(!std::isfinite(time_point_ns))
    {
        if(std::isnan(time_point_ns)) return "NaN";
        // For Inf, signbit helps distinguish +Inf and -Inf
        if(std::signbit(time_point_ns)) return "-Inf";
        return "+Inf";
    }

    bool is_negative_originally = false;
    if(std::signbit(time_point_ns))
    {
        // Determine if the original negative value is significant enough
        // to remain non-zero after conversion to integer nanoseconds.
        double threshold_for_non_zero_magnitude = round_before_cast ? 0.5 : 1.0;
        if(std::abs(time_point_ns) >= threshold_for_non_zero_magnitude)
        {
            is_negative_originally = true;
        }
        // If std::abs(double_ns_duration) is less than the threshold,
        // its magnitude will become 0 after conversion, so it won't be treated as
        // negative.
    }

    double   abs_val_for_conversion = std::abs(time_point_ns);
    uint64_t ns_duration_magnitude;

    if(round_before_cast)
    {
        ns_duration_magnitude = static_cast<uint64_t>(std::round(abs_val_for_conversion));
    }
    else
    {
        ns_duration_magnitude =
            static_cast<uint64_t>(abs_val_for_conversion);  // Truncates towards zero
    }

    // If the magnitude is zero (after potential rounding/truncation),
    // it's always displayed as positive zero, regardless of original sign.
    if(ns_duration_magnitude == 0)
    {
        if(condensed)
            return "0.000000000";
        else
            return "00:00.000000000";
    }

    std::string sign_prefix = "";
    if(is_negative_originally)
    {
        sign_prefix = "-";
    }

    uint32_t nanoseconds_part =
        static_cast<uint32_t>(ns_duration_magnitude % TimeConstants::ns_per_s);
    uint64_t total_seconds   = ns_duration_magnitude / TimeConstants::ns_per_s;
    uint8_t  display_seconds = static_cast<uint8_t>(total_seconds % TimeConstants::minute_in_s);
    uint64_t total_minutes   = total_seconds / TimeConstants::minute_in_s;
    uint8_t  display_minutes = static_cast<uint8_t>(total_minutes % TimeConstants::minute_in_s);
    uint64_t display_hours   = total_minutes / TimeConstants::minute_in_s;

    std::ostringstream oss;
    oss << sign_prefix;
    // Do not display hours if they are zero
    if(!condensed || display_hours > 0)
    {
        oss << std::setw(2) << std::setfill('0') << display_hours << ":";
    }
    // Display minutes only if hours are displayed or minutes are non-zero
    if(!condensed || display_hours > 0 || display_minutes > 0)
    {
        oss << std::setw(2) << std::setfill('0') << static_cast<int>(display_minutes)
            << ":";
    }
    oss << std::setw(2) << std::setfill('0') << static_cast<int>(display_seconds) << "."
        << std::setw(9) << std::setfill('0') << nanoseconds_part;

    return oss.str();
}

std::string
RocProfVis::View::nanosecond_to_formatted_str(double time_point_ns, TimeFormat format, bool include_units)
{
    switch(format)
    {
        case TimeFormat::kTimecode:
            return RocProfVis::View::nanosecond_to_timecode_str(time_point_ns, false);
        case TimeFormat::kTimecodeCondensed:
            return RocProfVis::View::nanosecond_to_timecode_str(time_point_ns, true);
        case TimeFormat::kSeconds:
            return RocProfVis::View::nanosecond_to_s_str(time_point_ns, include_units);
        case TimeFormat::kMilliseconds:
            return RocProfVis::View::nanosecond_to_ms_str(time_point_ns, include_units);
        case TimeFormat::kMicroseconds:
            return RocProfVis::View::nanosecond_to_us_str(time_point_ns, include_units);
        case TimeFormat::kNanoseconds:
        default: return RocProfVis::View::nanosecond_to_str(time_point_ns, include_units);
    }
}

std::string
RocProfVis::View::nanosecond_str_to_formatted_str(const std::string& time_str, double offset_ns,
                                TimeFormat time_format, bool include_units)
{
    if(time_str.empty())
    {
        return time_str;
    }

    // convert string to double
    double time_ns = 0;
    try
    {
        time_ns = std::stod(time_str);
    } catch(const std::exception&)
    {
        return time_str;
    }

    return nanosecond_to_formatted_str(time_ns - offset_ns, time_format, include_units);
}

double 
RocProfVis::View::calculate_nice_interval(double view_range, int target_divisions) 
{
    if(view_range <= 0.0 || target_divisions <= 0)
    {
        return 1.0;  // Avoid division by zero or log of non-positive
    }

    //Calculate the ideal, but possibly "ugly" interval
    double ideal_interval = view_range / target_divisions;

    //Calculate the order of magnitude, which is a power of 10
    double exponent = std::floor(std::log10(ideal_interval));
    double scale = std::pow(10.0, exponent);

    //Normalize the ideal interval to find a "nice" multiplier (1, 2, 5)
    double normalized_interval = ideal_interval / scale;
    
    double nice_multiplier;
    if(normalized_interval < 1.5)
    {
        nice_multiplier = 1;
    }
    else if(normalized_interval < 3.0)
    {
        nice_multiplier = 2;
    }
    else if(normalized_interval < 4.5)
    {
        nice_multiplier = 4;
    }
    else if(normalized_interval < 7.0)
    {
        nice_multiplier = 5;
    }
    else
    {
        nice_multiplier = 10;
    }

    return nice_multiplier * scale;
}

RocProfVis::View::ViewRangeNS
RocProfVis::View::calculate_adaptive_view_range(double item_start_ns,
                                                double item_duration_ns)
{
    // Compute a smooth, monotonic view span around the item using a
    // linear blend between two padding regimes (short vs long items).
    // span = duration * (1 + 2*pad) with pad blended between pad_short and pad_long.

    // 100 microseconds minimum view span
    const double min_visible_span_ns = 100.0 * TimeConstants::ns_per_us;
    const double T1 = 10.0 * TimeConstants::ns_per_us;  // 10 microseconds (ns)
    const double T2 = 5.0 * TimeConstants::ns_per_ms;   // 5 milliseconds (ns)

    const double pad_short = 9.0;  // generous padding for tiny items
    const double pad_long  = 1.0;  // modest padding for large items

    double d   = std::max(item_duration_ns, 1.0);  // guard against zero
    double pad = pad_short;
    if(d >= T2)
        pad = pad_long;
    else if(d > T1)
        pad = pad_short + (pad_long - pad_short) * ((d - T1) / (T2 - T1));

    double span = d * (1.0 + 2.0 * pad);
    if(span < min_visible_span_ns) span = min_visible_span_ns;

    double center               = item_start_ns + d * 0.5;
    double viewable_range_start = center - span * 0.5;
    double viewable_range_end   = center + span * 0.5;

    return { viewable_range_start, viewable_range_end };
}

std::string
RocProfVis::View::get_application_config_path(bool create_dirs)
{
#ifdef _WIN32
    const char*           appdata = std::getenv("APPDATA");
    std::filesystem::path config_dir =
        appdata ? appdata : std::filesystem::current_path();
    config_dir /= "rocprofvis";
#else
    const char*           xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path config_dir =
        xdg_config ? xdg_config
                   : (std::filesystem::path(std::getenv("HOME")) / ".config");
    config_dir /= "rocprofvis";
#endif
    if(create_dirs)
    {
        std::filesystem::create_directories(config_dir);
    }

    return config_dir.string();
}

std::string
RocProfVis::View::compact_number_format(double number)
{
    if(!std::isfinite(number))
    {
        if(std::isnan(number)) return "NaN";
        return std::signbit(number) ? "-Inf" : "+Inf";
    }

    bool   negative = std::signbit(number);
    double value    = std::fabs(number);

    const char* suffixes[] = { "", "K", "M", "B", "T", "P", "E" };
    uint32_t    magnitude  = 0;
    constexpr size_t max_suffix = std::size(suffixes) - 1;

    while(std::fabs(number) >= 1000.0 && magnitude < std::size(suffixes) - 1)
    {
        number /= 1000.0;
        ++magnitude;
    }

    std::ostringstream output;
    if(magnitude == max_suffix && value >= 1000.0)
    {
        int    exp  = static_cast<int>(std::floor(std::log10(value)));
        double base = value / std::pow(10.0, exp);
        output << (negative ? "-" : "") << std::fixed << std::setprecision(0) << base
               << "e" << (exp >= 0 ? "+" : "") << exp;
        return output.str();
    }

    output << std::fixed << std::setprecision(number >= 100 ? 0 : (number >= 10 ? 1 : 2))
        << number
        << suffixes[magnitude];
    return output.str();
}

bool RocProfVis::View::open_url(const std::string& url)
{
    if (url.empty()) {
        return false;
    }

#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((uintptr_t)result <= 32) {
        return false;
    }
    return true;
#elif defined(__APPLE__)
    std::string command = "open " + url;
    int status = system(command.c_str());
    return status == 0;
#else
    std::string command = "xdg-open " + url;
    int status = system(command.c_str());
    return status == 0;
#endif
}

// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_utils.h"
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

std::string
RocProfVis::View::format_nanosecond_timepoint(double double_ns_duration,
                                              bool   round_before_cast /*= false*/)
{
    // Handle non-finite cases first
    if(!std::isfinite(double_ns_duration))
    {
        if(std::isnan(double_ns_duration)) return "NaN";
        // For Inf, signbit helps distinguish +Inf and -Inf
        if(std::signbit(double_ns_duration)) return "-Inf";
        return "+Inf";
    }

    bool is_negative_originally = false;
    if(std::signbit(double_ns_duration))
    {
        // Determine if the original negative value is significant enough
        // to remain non-zero after conversion to integer nanoseconds.
        double threshold_for_non_zero_magnitude = round_before_cast ? 0.5 : 1.0;
        if(std::abs(double_ns_duration) >= threshold_for_non_zero_magnitude)
        {
            is_negative_originally = true;
        }
        // If std::abs(double_ns_duration) is less than the threshold,
        // its magnitude will become 0 after conversion, so it won't be treated as
        // negative.
    }

    double   abs_val_for_conversion = std::abs(double_ns_duration);
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
        return "00:00.000000000";
    }

    std::string sign_prefix = "";
    if(is_negative_originally)
    {
        sign_prefix = "-";
    }

    uint32_t nanoseconds_part =
        static_cast<uint32_t>(ns_duration_magnitude % 1000000000ULL);
    uint64_t total_seconds   = ns_duration_magnitude / 1000000000ULL;
    uint8_t  display_seconds = static_cast<uint8_t>(total_seconds % 60);
    uint64_t total_minutes   = total_seconds / 60;
    uint8_t  display_minutes = static_cast<uint8_t>(total_minutes % 60);
    uint64_t display_hours   = total_minutes / 60;

    std::ostringstream oss;
    oss << sign_prefix;
    // Do not display hours if they are zero
    if(display_hours > 0)
    {
        oss << std::setw(2) << std::setfill('0') << display_hours << ":";
    }
    // Display minutes only if hours are displayed or minutes are non-zero
    if(display_hours > 0 || display_minutes > 0)
    {
        oss << std::setw(2) << std::setfill('0') << static_cast<int>(display_minutes)
            << ":";
    }
    oss << std::setw(2) << std::setfill('0') << static_cast<int>(display_seconds) << "."
        << std::setw(9) << std::setfill('0') << nanoseconds_part;

    return oss.str();
}

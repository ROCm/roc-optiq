// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <cstdint>
#include <deque>
#include <iostream>
#include <string>

namespace RocProfVis
{
namespace View
{

template <typename T>
class CircularBuffer
{
public:
    CircularBuffer(size_t capacity)
    : m_max_capacity(capacity)
    {}

    void Clear() { m_buffer.clear(); }

    void Push(const T& item)
    {
        if(m_buffer.size() == m_max_capacity)
        {
            m_buffer.pop_front();  // Remove the oldest item
        }
        m_buffer.push_back(item);
    }

    void Pop()
    {
        if(m_buffer.empty())
        {
            // Do nothing if empty
            return;
        }
        m_buffer.pop_front();
    }

    bool IsFull() const { return m_buffer.size() == m_max_capacity; }

    bool IsEmpty() const { return m_buffer.empty(); }

    // Resize the buffer
    void Resize(size_t new_capacity)
    {
        m_max_capacity = new_capacity;

        // If the new capacity is smaller, remove the oldest items
        while(m_buffer.size() > m_max_capacity)
        {
            m_buffer.pop_front();
        }
    }

    const std::deque<T>& GetContainer() { return m_buffer; }

    // Methods to get iterators
    typename std::deque<T>::iterator begin() { return m_buffer.begin(); }

    typename std::deque<T>::iterator end() { return m_buffer.end(); }

    typename std::deque<T>::const_iterator begin() const { return m_buffer.begin(); }

    typename std::deque<T>::const_iterator end() const { return m_buffer.end(); }

private:
    std::deque<T> m_buffer;
    size_t        m_max_capacity;
};

/**
 * @brief Formats a time point, originally from a double representing nanoseconds (can be
 * negative), into a human-readable [+-]HH:MM:SS.nanoseconds string.
 *
 * @param time_point_ns The time point in nanoseconds, stored as a double.
 * @param condensed If true, omits leading hours or minutes if they are zero.
 * @param round_before_cast If true, std::round will be applied to the absolute value
 *                          before casting to integer nanoseconds. Otherwise, it
 * truncates.
 * @return std::string The formatted duration string. Handles positive, negative, and zero
 * values. Also handles NaN and Inf.
 */
std::string
nanosecond_to_timecode_str(double time_point_ns, bool condensed = true,
                           bool round_before_cast = false);

/**
 * @brief Converts a double representing nanoseconds into a string representation in
 * microseconds.
 *
 * @param time_point_ns The duration in nanoseconds as a double.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string in microseconds.
 */
std::string
nanosecond_to_us_str(double time_point_ns, bool include_units = false);

/**
 * @brief Converts a double representing nanoseconds into a string representation in
 * milliseconds.
 *
 * @param time_point_ns The duration in nanoseconds as a double.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string in milliseconds.
 */
std::string
nanosecond_to_ms_str(double time_point_ns, bool include_units = false);

/**
 * @brief Converts a double representing nanoseconds into a string representation in
 * seconds.
 *
 * @param time_point_ns The duration in nanoseconds as a double.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string in seconds.
 */
std::string
nanosecond_to_s_str(double time_point_ns, bool include_units = false);

/**
 * @brief Converts a double representing nanoseconds into a string representation.
 *
 * @param time_point_ns The duration in nanoseconds as a double.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string in nanoseconds.
 */
std::string
nanosecond_to_str(double time_point_ns, bool include_units = false);

enum class TimeFormat
{
    kTimecode,
    kTimecodeCondensed,
    kSeconds,
    kMilliseconds,
    kMicroseconds,
    kNanoseconds
};

/**
 * @brief Converts a double representing nanoseconds into a string representation
 * based on the specified TimeFormat.
 *
 * @param time_point_ns The duration in nanoseconds as a double.
 * @param format The desired TimeFormat for the output string.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string.
 */
std::string
nanosecond_to_formatted_str(double time_point_ns, TimeFormat format, bool include_units = false);

/**
 * @brief Converts a string representing nanoseconds into a formatted string representation
 * based on the specified TimeFormat.
 *
 * @param time_str The duration in nanoseconds as a string.
 * @param offset A double offset in nanoseconds to subtract from the parsed value.
 * @param time_format The desired TimeFormat for the output string.
 * @param include_units If true, appends the appropriate time unit suffix to the string.
 * @return std::string The formatted duration string.
 */
std::string 
nanosecond_str_to_formatted_str(const std::string& time_ns_str, double offset_ns,
                                TimeFormat time_format, bool include_units);


/**
 * @brief Calculates a "nice" grid interval for a timeline.
 *
 * @param viewRange The total duration of the visible timeline range in nanoseconds,
 * specified as a double.
 * @param targetDivisions The desired number of divisions on the screen.
 * @return A "nice" interval in nanoseconds (e.g., 1000, 2000, 5000, 10000...).
 */
double
calculate_nice_interval(double view_range, int target_divisions);

typedef struct ViewRangeNS
{
    double start_ns;
    double end_ns;
} ViewRangeNS;

/**
 * @brief Calculates an adaptive view range based on the item start time and duration.
 *        Centers the item in view with an adaptive padding around it.
 *
 * @param item_start_ns The start time of the item in nanoseconds.
 * @param item_duration_ns The duration of the item in nanoseconds.
 * @return A ViewRangeNS struct representing the calculated view range.
 */
ViewRangeNS
calculate_adaptive_view_range(double item_start_ns, double item_duration_ns);

namespace TimeConstants
{
constexpr uint64_t ns_per_us    = 1000;
constexpr uint64_t ns_per_ms    = 1000 * ns_per_us;
constexpr uint64_t ns_per_s     = 1000 * ns_per_ms;
constexpr uint64_t minute_in_s  = 60;
constexpr uint64_t minute_in_ns = minute_in_s * ns_per_s;
}  // namespace TimeConstants

std::string get_application_config_path(bool create_dirs);

std::string
compact_number_format(float number);

}  // namespace View
}  // namespace RocProfVis

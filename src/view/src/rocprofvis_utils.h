// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <iostream>
#include <deque>
#include <string>  

namespace RocProfVis
{
namespace View
{

template <typename T>
T
clamp(T value, T minVal, T maxVal)
{
    if(value < minVal)
    {
        return minVal;
    }
    else if(value > maxVal)
    {
        return maxVal;
    }
    return value;
}

template <typename T>
class CircularBuffer {
public:
    CircularBuffer(size_t capacity) : m_max_capacity(capacity) {}

    void Clear() {
        m_buffer.clear();
    }
    
    void Push(const T& item) {
        if (m_buffer.size() == m_max_capacity) {
            m_buffer.pop_front(); // Remove the oldest item
        }
        m_buffer.push_back(item);
    }

    void Pop() {
        if (m_buffer.empty()) {
           // Do nothing if empty
           return;
        }
        m_buffer.pop_front();
    }

    bool IsFull() const {
        return m_buffer.size() == m_max_capacity;
    }

    bool IsEmpty() const {
        return m_buffer.empty();
    }

    // Resize the buffer
    void Resize(size_t new_capacity) {
        m_max_capacity = new_capacity;

        // If the new capacity is smaller, remove the oldest items
        while (m_buffer.size() > m_max_capacity) {
            m_buffer.pop_front();
        }
    }    

    const std::deque<T> & GetContainer() {
        return m_buffer;
    }

    // Methods to get iterators
    typename std::deque<T>::iterator begin() {
        return m_buffer.begin();
    }

    typename std::deque<T>::iterator end() {
        return m_buffer.end();
    }

    typename std::deque<T>::const_iterator begin() const {
        return m_buffer.begin();
    }

    typename std::deque<T>::const_iterator end() const {
        return m_buffer.end();
    }

private:
    std::deque<T> m_buffer;
    size_t m_max_capacity;
};


  
/**  
 * @brief Formats a time point, originally from a double representing nanoseconds (can be negative),  
 *        into a human-readable [+-]HH:MM:SS.nanoseconds string.  
 *  
 * @param double_ns_duration The time point in nanoseconds, stored as a double.  
 * @param round_before_cast If true, std::round will be applied to the absolute value  
 *                          before casting to integer nanoseconds. Otherwise, it truncates.  
 * @return std::string The formatted duration string. Handles positive, negative, and zero values.  
 *                     Also handles NaN and Inf.  
 */ 
std::string format_nanosecond_timepoint(double double_ns_duration, bool round_before_cast = false);

} // namespace View
} // namespace RocProfVis

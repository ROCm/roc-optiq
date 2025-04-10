// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <iostream>
#include <deque>

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
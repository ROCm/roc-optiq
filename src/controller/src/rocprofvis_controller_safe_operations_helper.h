// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_cpp_abi_wrapper.h"

#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace RocProfVis
{
namespace Controller
{

template<typename T>
rocprofvis_result_t CheckedAssignUnsigned(uint64_t value, T& output) noexcept
{
    return Abi::CheckedAssignUnsigned(value, &output);
}

template<typename Enum, typename Validator>
rocprofvis_result_t CheckedAssignEnum(uint64_t value, Enum& output,
                                     Validator&& validator) noexcept
{
    return Abi::CheckedAssignEnum(value, &output,
                                  std::forward<Validator>(validator));
}

template<typename Container>
rocprofvis_result_t CheckedResize(Container& container, uint64_t size) noexcept
{
    if(size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
        return kRocProfVisResultOutOfRange;
    }

    const size_t converted_size = static_cast<size_t>(size);
    if(converted_size > container.max_size())
    {
        return kRocProfVisResultOutOfRange;
    }

    try
    {
        container.resize(converted_size);
    }
    catch(std::length_error const&)
    {
        return kRocProfVisResultOutOfRange;
    }
    catch(std::bad_alloc const&)
    {
        return kRocProfVisResultMemoryAllocError;
    }
    catch(...)
    {
        return kRocProfVisResultUnknownError;
    }

    return kRocProfVisResultSuccess;
}

template<typename Container>
rocprofvis_result_t CheckedEnsureIndex(Container& container, uint64_t index) noexcept
{
    if(index > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
        return kRocProfVisResultOutOfRange;
    }

    const size_t converted_index = static_cast<size_t>(index);
    if(converted_index >= container.max_size())
    {
        return kRocProfVisResultOutOfRange;
    }
    if(converted_index < container.size())
    {
        return kRocProfVisResultSuccess;
    }

    return CheckedResize(container, static_cast<uint64_t>(converted_index) + 1);
}

template<typename Function>
rocprofvis_result_t ControllerCall(Function&& function) noexcept
{
    try
    {
        return function();
    }
    catch(std::length_error const&)
    {
        return kRocProfVisResultOutOfRange;
    }
    catch(std::bad_alloc const&)
    {
        return kRocProfVisResultMemoryAllocError;
    }
    catch(...)
    {
        return kRocProfVisResultUnknownError;
    }
}

template<typename Function>
auto ControllerAllocate(Function&& function) noexcept -> decltype(function())
{
    try
    {
        return function();
    }
    catch(...)
    {
        return nullptr;
    }
}

template<typename Function>
void ControllerFree(Function&& function) noexcept
{
    try
    {
        function();
    }
    catch(...)
    {
    }
}

}
}

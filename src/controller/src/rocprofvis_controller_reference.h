// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

template<typename Pointer, typename Type, unsigned Enum>
class Reference
{
public:
    Reference()
    : m_object(nullptr)
    {
    }

    Reference(Pointer* ptr)
    : m_object(nullptr)
    {
        Handle* handle = reinterpret_cast<Handle*>(ptr);
        // Validate through the base before downcasting; Type::GetType may be final.
        m_object = (handle && handle->GetType() == Enum)
                       ? static_cast<Type*>(handle)
                       : nullptr;
    }

    ~Reference()
    {
    }

    bool IsValid(void) const
    {
        Handle* handle = reinterpret_cast<Handle*>(m_object);
        return (handle && handle->GetType() == Enum);
    }

    rocprofvis_handle_t** GetHandleAddress(void)
    {
        ROCPROFVIS_ASSERT(m_object == nullptr);
        return (rocprofvis_handle_t**)&m_object;
    }

    Type* Get(void)
    {
        ROCPROFVIS_ASSERT(IsValid());
        return m_object;
    }

    Type& operator*()
    {
        ROCPROFVIS_ASSERT(m_object);
        return *m_object;
    }

    Type* operator->()
    {
        ROCPROFVIS_ASSERT(m_object);
        return m_object;
    }

private:
    Type* m_object;
};

}
}

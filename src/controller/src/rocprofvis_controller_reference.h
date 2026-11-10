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
        if(HasType(handle))
        {
            m_object = static_cast<Type*>(handle);
        }
    }

    ~Reference()
    {
    }

    bool IsValid(void) const
    {
        return HasType(m_object);
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
    // GetType is final in each Type, so calling it through a Type* is resolved
    // at compile time and always matches Enum. Calling it through the Handle
    // base reads the object's real type.
    static bool HasType(Handle* handle)
    {
        return handle && handle->GetType() == Enum;
    }

    Type* m_object;
};

}
}

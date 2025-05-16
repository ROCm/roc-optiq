// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

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
        Type* t = (Type*)ptr;
        m_object = (t && t->GetType() == Enum) ? t : nullptr; 
    }

    ~Reference()
    {
    }

    bool IsValid(void) const
    {
        return (m_object && m_object->GetType() == Enum);
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

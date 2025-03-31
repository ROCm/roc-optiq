// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <cassert>

namespace RocProfVis
{
namespace Controller
{

template<typename Pointer, typename Type, unsigned Enum>
class Reference
{
public:
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
        return m_object != nullptr;
    }

    Type* Get(void)
    {
        return m_object;
    }

    Type& operator*()
    {
        assert(m_object);
        return *m_object;
    }

    Type* operator->()
    {
        assert(m_object);
        return m_object;
    }

private:
    Type* m_object;
};

}
}

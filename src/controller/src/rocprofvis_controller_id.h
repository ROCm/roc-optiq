// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

namespace RocProfVis
{
namespace Controller
{

template<typename Type>
class IdGenerator
{
public:
    IdGenerator()
    : m_id(0)
    {
    }

    ~IdGenerator()
    {
    }

    uint64_t GetNextId()
    {
        return (++m_id);
    }

private:
    uint64_t m_id;
};

}
}

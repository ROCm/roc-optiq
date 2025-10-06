// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once


#include "rocprofvis_common_types.h"
#include "rocprofvis_dm_base.h"
#include <vector>


namespace RocProfVis
{
namespace DataModel
{

class Trace;  // Forward declaration

class Histogram : public DmBase
{
public:
    Histogram(Trace* ctx, rocprofvis_dm_charptr_t description);

    rocprofvis_dm_size_t    GetNumberOfBins() const;
    rocprofvis_dm_size_t    GetMemoryFootprint() const;
    rocprofvis_dm_charptr_t Description() const;
    void                    SetBins(const std::vector<uint64_t>& bins);

    void                   AddBin(uint64_t value);
    rocprofvis_dm_result_t GetBinAt(rocprofvis_dm_property_index_t index,
                                    uint64_t&                      value) const;

    // Property accessors
    rocprofvis_dm_result_t GetPropertyAsUint64(rocprofvis_dm_property_t       property,
                                               rocprofvis_dm_property_index_t index,
                                               uint64_t* value) override;
    rocprofvis_dm_result_t GetPropertyAsCharPtr(rocprofvis_dm_property_t       property,
                                                rocprofvis_dm_property_index_t index,
                                                char** value) override;
    rocprofvis_dm_result_t GetPropertyAsHandle(rocprofvis_dm_property_t       property,
                                               rocprofvis_dm_property_index_t index,
                                               rocprofvis_dm_handle_t* value) override;

#ifdef TEST
    const char* GetPropertySymbol(rocprofvis_dm_property_t property);
#endif

private:
    Trace*                m_ctx;
    std::string           m_description;
    std::vector<uint64_t> m_bins;
    uint64_t              m_id;
 };

}  // namespace DataModel
}  // namespace RocProfVis

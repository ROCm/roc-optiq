// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface_types.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class PcSampling : public Handle
{
public:
    PcSampling();
    virtual ~PcSampling();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) final;

    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const;

private:

    struct SourceLine
    {
        uint32_t    id             = 0;
        uint32_t    source_file_id = 0;
        uint32_t    line_number    = 0;
        std::string content;
    };
    struct SourceFile
    {
        uint32_t    id        = 0;
        std::string file_path;
        std::string content_checksum;
    };

    struct IsaLine
    {
        uint32_t    id                  = 0;
        uint32_t    code_object_id      = 0;
        uint64_t    code_object_offset  = 0;
        uint32_t    instruction_type_id = 0;
        std::string instruction;
        std::string comment;
    };
    struct CodeObject
    {
        uint32_t    id               = 0;
        std::string uri;
        std::string content_checksum;
    };

    struct IsaToIsaDep
    {
        uint32_t dependent_isa_line_id  = 0;
        uint32_t dependency_isa_line_id = 0;
    };
    struct IsaToSourceDep
    {
        uint32_t isa_line_id    = 0;
        uint32_t source_line_id = 0;
        uint32_t depth          = 0;
    };
    struct StallRecord
    {
        uint32_t id                 = 0;
        uint32_t isa_line_id        = 0;
        uint64_t dispatch_id        = 0;
        float    avg_active_lanes   = 0.0f;
        uint32_t wave_issued_count  = 0;
        uint32_t total_sample_count = 0;
    };
    struct StallReasonCount
    {
        uint32_t record_id   = 0;
        uint32_t type_id     = 0;
        uint32_t count       = 0;
    };

    std::vector<SourceFile>      m_source_files;
    std::vector<SourceLine>      m_source_lines;
    std::vector<CodeObject>      m_code_objects;
    std::vector<IsaLine>         m_isa_lines;
    std::vector<IsaToIsaDep>     m_isa_to_isa_deps;
    std::vector<IsaToSourceDep>  m_isa_to_source_deps;
    std::vector<StallRecord>     m_stall_records;
    std::vector<StallReasonCount> m_stall_reason_counts;

};


}
}

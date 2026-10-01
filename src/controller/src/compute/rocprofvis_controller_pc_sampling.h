// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface_types.h"
#include <mutex>
#include <string>
#include <unordered_map>
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
    friend class ComputeTrace;

    enum class DataLayer
    {
        kIsa,
        kSource,
        kStalls,
    };

    std::recursive_mutex& GetLayerMutex(DataLayer layer);
    std::recursive_mutex& GetPropertyMutex(rocprofvis_property_t property);

    struct SourceLine
    {
        uint64_t    source_line_uuid = 0;
        uint64_t    source_file_uuid = 0;
        uint64_t    line_number      = 0;
        std::string content;
    };
    struct SourceFile
    {
        uint64_t    source_file_uuid = 0;
        uint64_t    workload_id      = 0;
        std::string file_path;
        std::string md5_checksum;
    };
    struct InstructionLine
    {
        uint64_t    instruction_uuid      = 0;
        uint64_t    kernel_symbol_uuid    = 0;
        uint64_t    instruction_type_uuid = 0;
        uint64_t    code_object_offset    = 0;
        std::string instruction;
    };
    struct KernelSymbol
    {
        uint64_t kernel_symbol_uuid = 0;
        uint64_t code_object_uuid   = 0;
        uint64_t kernel_uuid        = 0;
        uint64_t code_object_offset = 0;
    };
    struct CodeObjectStore
    {
        uint64_t code_object_uuid = 0;
        uint64_t workload_id      = 0;
        uint64_t pid              = 0;
        uint64_t code_object_id   = 0;
        uint64_t load_base        = 0;
    };
    struct InstructionSourceLine
    {
        uint64_t instruction_source_line_uuid = 0;
        uint64_t instruction_uuid             = 0;
        uint64_t source_line_uuid             = 0;
        uint64_t source_file_uuid             = 0;
        uint64_t frame_index                  = 0;
    };
    struct PcSampleState
    {
        uint64_t pc_sample_state_uuid    = 0;
        uint64_t instruction_uuid        = 0;
        uint64_t total_count             = 0;
        uint64_t issue_count             = 0;
        uint64_t stall_count             = 0;
        double   active_thread_percent   = 0.0;
        double   wave_occupancy_percent = 0.0;
        uint64_t dispatch_uuid           = 0;
    };
    struct PcSampleStallReason
    {
        uint64_t pc_sample_stall_reason_uuid        = 0;
        uint64_t pc_sample_state_uuid               = 0;
        uint64_t pc_sample_stall_reason_lookup_uuid = 0;
        uint64_t count                              = 0;
    };
    struct PcSampleStallReasonLookup
    {
        uint64_t    pc_sample_stall_reason_lookup_uuid = 0;
        std::string text;
    };
    struct InstructionTypeLookup
    {
        uint64_t    instruction_type_lookup_uuid = 0;
        std::string text;
    };
    struct InstructionSample
    {
        uint64_t instruction_sample_uuid        = 0;
        uint64_t pc_sample_state_uuid           = 0;
        uint64_t instruction_sample_lookup_uuid = 0;
        uint64_t count                          = 0;
    };
    struct InstructionSampleLookup
    {
        uint64_t    instruction_sample_lookup_uuid = 0;
        std::string text;
    };

    std::vector<SourceFile>                m_source_files;
    std::vector<SourceLine>                m_source_lines;
    std::vector<CodeObjectStore>           m_code_object_store;
    std::vector<KernelSymbol>              m_kernel_symbols;
    std::vector<InstructionLine>                   m_instruction_lines;
    std::vector<InstructionSourceLine>     m_instruction_source_lines;
    std::vector<PcSampleState>             m_pc_sample_states;
    std::vector<PcSampleStallReason>       m_pc_sample_stall_reasons;
    std::vector<PcSampleStallReasonLookup> m_pc_sample_stall_reason_lookups;
    std::vector<InstructionTypeLookup>     m_instruction_type_lookups;
    std::vector<InstructionSample>         m_instruction_samples;
    std::vector<InstructionSampleLookup>   m_instruction_sample_lookups;

    std::unordered_map<uint64_t, std::vector<SourceLine>> m_source_line_cache;
    bool                                                  m_source_files_loaded = false;
    bool m_code_object_store_loaded                                             = false;
    bool m_kernel_symbols_loaded                                                = false;
    bool m_instruction_lines_loaded                                             = false;
    bool m_instruction_source_lines_loaded                                      = false;
    bool m_pc_sample_states_loaded                                              = false;
    bool m_stalls_loaded                                                        = false;
    bool m_instruction_samples_loaded                                           = false;

    // Fetches populate independent data sets. Layer-owned locks allow consumers
    // to read a completed layer while unrelated layers are still loading.
    std::recursive_mutex m_isa_data_mutex;
    std::recursive_mutex m_source_data_mutex;
    std::recursive_mutex m_stalls_data_mutex;

};


}
}

// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_profiler_backend.h"
#include <string>
#include <cstdint>
#include <map>

namespace RocProfVis
{
namespace View
{

struct CheckboxEntry
{
    const char* id;
    const char* display_name;
    bool        default_on;
};

extern CheckboxEntry const kRocmDomains[];
extern size_t const        kRocmDomainsCount;

extern CheckboxEntry const kAmdSmiMetrics[];
extern size_t const        kAmdSmiMetricsCount;

extern CheckboxEntry const kPerfettoCategories[];
extern size_t const        kPerfettoCategoriesCount;

struct RocprofPresetEntry
{
    const char* name;
    const char* description;
};

extern RocprofPresetEntry const kRocprofSysPresets[];
extern size_t const             kRocprofSysPresetsCount;

struct RocprofSysSettings
{
    // rocprof-sys built-in preset (e.g. "balanced"), empty = none
    std::string rocprof_preset;

    // General
    double      trace_delay       = 0.0;
    double      trace_duration    = 0.0;
    std::string trace_region;

    // Backends
    bool trace_backend            = true;
    bool profile                  = false;
    bool flat_profile             = false;
    bool use_rocpd                = true;
    bool use_sampling             = false;
    bool use_process_sampling     = true;
    bool use_amd_smi              = true;
    // Sampling
    double      sampling_freq              = 300.0;
    bool        sampling_cputime           = false;
    bool        sampling_realtime          = false;
    bool        sampling_overflow          = false;
    double      sampling_duration          = 0.0;
    int32_t     sampling_allocator_size    = 8;
    std::string sampling_overflow_event    = "perf::PERF_COUNT_HW_CACHE_REFERENCES";
    bool        sampling_keep_internal     = true;
    bool        sampling_include_inlines   = false;

    // ROCm
    std::map<std::string, bool> rocm_domains;
    std::string                 rocm_domains_custom;
    std::string                 rocm_events;
    bool                        rocm_group_by_queue = false;

    // Perfetto
    std::string perfetto_backend         = "inprocess";
    int32_t     perfetto_buffer_size_kb  = 1024000;
    int32_t     perfetto_flush_period_ms = 10000;
    std::string perfetto_fill_policy     = "discard";
    bool        perfetto_annotations     = true;
    bool        perfetto_combine_traces  = false;
    std::map<std::string, bool> enable_categories;
    std::map<std::string, bool> disable_categories;
    std::string perfetto_file            = "perfetto-trace.proto";

    // Process Sampling
    bool                        cpu_freq_enabled = false;
    std::map<std::string, bool> amd_smi_metrics;
    std::string                 amd_smi_metrics_custom;
    std::string                 sampling_cpus    = "none";
    std::string                 sampling_gpus    = "all";
    bool                        use_ainic        = false;

    // Parallelism
    bool use_mpip    = false;
    bool use_ucx     = false;
    bool use_shmem   = false;
    bool use_rcclp   = false;
    bool use_ompt    = false;
    bool use_kokkosp = false;

    // Advanced
    std::string config_file;
    std::string log_level           = "info";
    std::string log_file            = "rocprof-sys-log.txt";
    std::string tmpdir;
    bool        use_pid             = true;
    std::string timemory_components = "wall_clock";

    // Instrument
    std::string instr_include;
    std::string instr_exclude;
    int32_t     min_instructions = 0;

    jt::Json ToJson() const;
    static RocprofSysSettings FromJson(jt::Json const& json);

    static std::map<std::string, bool> BuildDefaultMap(
        CheckboxEntry const* entries, size_t count);
};

class RocprofSysBackend : public IProfilerBackend
{
public:
    RocprofSysBackend();
    ~RocprofSysBackend() override = default;

    const char* Id() const override;
    const char* DisplayName() const override;

    std::vector<ToolOption> GetTools() const override;
    std::string GetDefaultBinary(std::string const& tool_id) const override;
    std::vector<TabDescriptor> GetTabs(std::string const& tool_id) const override;

    std::string Validate(LaunchConfig const& config) const override;

    void FlattenToExecution(
        LaunchConfig const& config,
        std::vector<std::pair<std::string, std::string>>& env_out,
        std::vector<std::string>& argv_out) const override;

    jt::Json SaveSettings() const override;
    void LoadSettings(jt::Json const& payload) override;
    std::string ExportCfg() const override;

    std::vector<WarningMessage> GetWarnings(
        LaunchConfig const& config) const override;

private:
    void RenderGeneralTraceOptions();
    void RenderBackendsTab();
    void RenderSamplingTab();
    void RenderRocmTab();
    void RenderPerfettoTab();
    void RenderProcessSamplingTab();
    void RenderParallelismTab();
    void RenderInstrumentTab();
    void RenderAdvancedTab();

    static std::string JoinEnabledKeys(
        std::map<std::string, bool> const& m,
        std::string const& custom);

    RocprofSysSettings m_settings;
};

} // namespace View
} // namespace RocProfVis

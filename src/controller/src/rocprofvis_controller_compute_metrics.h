// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <array>
#include <string>
#include <vector>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

typedef struct ComputeTableDefinition
{
    rocprofvis_controller_compute_table_types_t m_type;
    std::string m_title;
} ComputeTableDefinition;

typedef struct ComputePlotDataDefinition
{
    rocprofvis_controller_compute_table_types_t m_table_type;
    std::vector<std::string> m_metric_keys;
} ComputePlotDataDefinition;

typedef struct ComputePlotDataSeriesDefinition
{
    std::string m_name;
    std::vector<ComputePlotDataDefinition> m_values;
} ComputePlotDataSourceDefinition;

typedef struct ComputePlotDefinition
{
    rocprofvis_controller_compute_plot_types_t m_type;
    std::string m_title;
    std::string x_axis_label;
    std::string y_axis_label;
} ComputePlotDefinition;

typedef struct ComputeTablePlotDefinition : ComputePlotDefinition
{
    std::vector<ComputePlotDataSeriesDefinition> m_series;
} ComputeTablePlotDefinition;

typedef struct ComputeMetricDefinition
{
    rocprofvis_controller_compute_table_types_t m_table_type;
    std::string m_metric_key;
} ComputeMetricDefinition;

typedef struct ComputeRooflineDefinition
{
    enum MemoryLevel : uint32_t
    {
        MemoryLevelHBM,
        MemoryLevelL2,
        MemoryLevelL1,
        MemoryLevelLDS,
        MemoryLevelCount
    };
    enum Format : uint32_t
    {
        FormatFP64,
        FormatFP32,
        FormatFP16,
        FormatINT8,
        FormatCount
    };
    enum Pipe : uint32_t
    {
        PipeVALU,
        PipeMFMA
    };
    struct ComputeRooflinePlotDefinition : ComputePlotDefinition
    {
        std::vector<Format> m_formats;
    };
    std::unordered_map<MemoryLevel, std::string> m_memory_names;
    std::unordered_map<Format, std::string> m_format_names;
    std::unordered_map<Pipe, std::string> m_pipe_names;
    std::unordered_map<Format, std::unordered_map<Pipe, std::string>> m_format_prefix;
    std::unordered_map<rocprofvis_controller_compute_plot_types_t, ComputeRooflinePlotDefinition> m_plots;
    std::uint32_t m_device_id;
    double x_axis_min;
    double x_axis_max;
} ComputeRooflineDefinition;

const std::unordered_map<std::string, ComputeTableDefinition> COMPUTE_TABLE_DEFINITIONS { 
    {"0.1_Top_Kernels.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeKernelList, "Kernel List"}},
    {"0.2_Dispatch_List.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeDispatchList, "Dispatch List"}},
    {"sysinfo.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSysInfo, "System Info"}},
    {"2.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSpeedOfLight, "System Speed of Light"}},
    {"3.1_Memory_Chart.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeMemoryChart, "Memory Chart"}},
    {"5.1_Command_Processor_Fetcher.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeCPFetcher, "Command Processor Fetcher"}},
    {"5.2_Packet_Processor.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeCPPacketProcessor, "Command Processor Packet Processor"}},
    {"6.1_Workgroup_Manager_Utilizations.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeWorkgroupMngrUtil, "Workgroup Manager Utilizations"}},
    {"6.2_Workgroup_Manager_-_Resource_Allocation.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeWorkgroupMngrRescAlloc, "Workgroup Manager Resource Allocation"}},
    {"7.1_Wavefront_Launch_Stats.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeWavefrontLaunch, "Wavefront Launch Stats"}},
    {"7.2_Wavefront_Runtime_Stats.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeWavefrontRuntime, "Wavefront Runtime Stats"}},
    {"10.1_Overall_Instruction_Mix.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeInstrMix, "Compute Units Overall Instruction Mix"}},
    {"10.2_VALU_Arithmetic_Instr_Mix.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVALUInstrMix, "Compute Units VALU Arithmetic Instruction Mix"}},
    {"10.3_VMEM_Instr_Mix.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVMEMInstrMix, "Compute Units VMEM Instruction Mix"}},
    {"10.4_MFMA_Arithmetic_Instr_Mix.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeMFMAInstrMix, "Compute Units MFMA Arithmetic Instruction Mix"}},
    {"11.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeCUSpeedOfLight, "Compute Units Speed of Light"}},
    {"11.2_Pipeline_Stats.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeCUPipelineStats, "Compute Units Pipeline Stats"}},
    {"11.3_Arithmetic_Operations.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeCUOps, "Compute Units Arithmetic Operations"}},
    {"12.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeLDSSpeedOfLight, "Local Data Share Speed of Light"}},
    {"12.2_LDS_Stats.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeLDSStats, "Local Data Share Stats"}},
    {"13.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeInstrCacheSpeedOfLight, "Instruction Cache Speed of Light"}},
    {"13.2_Instruction_Cache_Accesses.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeInstrCacheAccesses, "Instruction Cache Accesses"}},
    {"13.3_Instruction_Cache_-_L2_Interface.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeInstrCacheL2Interface, "Instruction Cache - L2 Cache Interface"}},
    {"14.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSL1CacheSpeedOfLight, "Scalar L1 Data Cache Speed of Light"}},
    {"14.2_Scalar_L1D_Cache_Accesses.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSL1CacheAccesses, "Scalar L1 Data Cache Accesses"}},
    {"14.3_Scalar_L1D_Cache_-_L2_Interface.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSL1CacheL2Transactions, "Scalar L1 Data Cache - L2 Cache Interface"}},
    {"15.1_Address_Processing_Unit.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeAddrProcessorStats, "Address Processing Unit"}},
    {"15.2_Data-Return_Path.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeAddrProcessorReturnPath, "Data Return Path"}},
    {"16.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVL1CacheSpeedOfLight, "Vector L1 Data Cache Speed of Light"}},
    {"16.2_L1D_Cache_Stalls_(%).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVL1CacheStalls, "Vector L1 Data Cache Stalls"}},
    {"16.3_L1D_Cache_Accesses.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVL1CacheAccesses, "Vector L1 Data Cache Accesses"}},
    {"16.4_L1D_-_L2_Transactions.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVL1CacheL2Transactions, "Vector L1 Data Cache - L2 Cache Transactions"}},
    {"16.5_L1D_Addr_Translation.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeVL1CacheAddrTranslations, "Vector L1 Data Cache Address Translation"}},
    {"17.1_Speed-of-Light.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheSpeedOfLight, "L2 Cache Speed of Light"}},
    {"17.2_L2_-_Fabric_Transactions.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricTransactions, "L2 Cache - Fabric Transactions"}},
    {"17.3_L2_Cache_Accesses.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheAccesses, "L2 Cache Accesses"}},
    {"17.4_L2_-_Fabric_Interface_Stalls.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricStalls, "L2 Cache - Fabric Interface Stalls"}},
    {"17.5_L2_-_Fabric_Detailed_Transaction_Breakdown.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricTransactionsDetailed, "L2 Cache - Fabric Detailed Transaction Breakdown"}},
    {"18.1_Aggregate_Stats_(All_channels).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheStats, "L2 Cache Aggregate Stats"}},
    {"18.2_L2_Cache_Hit_Rate_(pct).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheHitRate, "L2 Cache Hit Rate Per Channel"}},
    {"18.3_L2_Requests_(per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheReqs1, "L2 Cache Requests Per Channel"}},
    {"18.4_L2_Requests_(per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheReqs2, "L2 Cache Requests Per Channel"}},
    {"18.5_L2-Fabric_Requests_(per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricReqs, "L2 Cache - Fabric Requests Per Channel"}},
    {"18.6_L2-Fabric_Read_Latency_(Cycles).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricRdLat, "L2 Cache - Fabric Read Latency Per Channel"}},
    {"18.7_L2-Fabric_Write_and_Atomic_Latency_(Cycles).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricWrAtomLat, "L2 Cache - Fabric Write and Atomic Latency Per Channel"}},
    {"18.8_L2-Fabric_Atomic_Latency_(Cycles).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheFabricAtomLat, "L2 Cache - Fabric Atomic Latency Per Channel"}},
    {"18.9_L2-Fabric_Read_Stall_(Cycles_per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheRdStalls, "L2 Cache - Fabric Read Stall Per Channel"}},
    {"18.10_L2-Fabric_Write_and_Atomic_Stall_(Cycles_per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2CacheWrAtomStalls, "L2 Cache - Fabric Detailed Transaction Breakdown"}},
    {"18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2Cache128Reqs, "L2 Cache - Fabric 128B Read Requests Per Channel"}},
    {"pmc_perf.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeRooflineCounters, "Roofline Counters"}},
    {"roofline.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeRooflineBenchmarks, "Roofline Benchmarks"}}
};

const std::array COMPUTE_PLOT_DEFINITIONS {  
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeKernelDurationPercentage, "Kernel Durations", "Duration (%)", "Kernel", {  
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeKernelList, {
                "{Pct}"
            }}
        }}  
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeKernelDuration, "Kernel Durations", "Mean Duration (ns)", "Kernel", {  
        ComputePlotDataSeriesDefinition{"", { 
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeKernelList, {
                "{Mean(ns)}"
            }}  
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeL2CacheSpeedOfLight, "L2 Cache - Speed of Light", "% of Peak", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeL2CacheSpeedOfLight, {
                "Utilization Avg", 
                "Bandwidth Avg", 
                "Hit Rate Avg"
            }}
        }},
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricSpeedOfLight, "L2 Cache - Speed of Light", "Gb/s", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeL2CacheSpeedOfLight, {
                "L2-Fabric Read BW Avg", 
                "L2-Fabric Write and Atomic BW Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricStallsRead, "L2 Cache - Infinity Fabric Interface Read Stalls", "%", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeL2CacheFabricStalls, {
                "Read - PCIe Stall Avg", 
                "Read - Infinity Fabric Stall Avg", 
                "Read - HBM Stall Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeL2CacheFabricStallsWrite, "L2 Cache - Infinity Fabric Interface Stalls", "%", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeL2CacheFabricStalls, {
                "Write - PCIe Stall Avg", 
                "Write - Infinity Fabric Stall Avg", 
                "Write - HBM Stall Avg", 
                "Write - Credit Starvation Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeInstrMix, "Compute Units - Instruction Mix", "Instructions per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeInstrMix, {
                "VALU Avg", 
                "VMEM Avg", 
                "LDS Avg", 
                "MFMA Avg", 
                "SALU Avg", 
                "SMEM Avg", 
                "Branch Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeCUOps, "Compute Units - Speed of Light", "Instructions per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeCUSpeedOfLight, {
                "VALU FLOPs Pct of Peak", 
                "VALU IOPs Pct of Peak", 
                "MFMA FLOPs (BF16) Pct of Peak", 
                "MFMA FLOPs (F16) Pct of Peak", 
                "MFMA FLOPs (F32) Pct of Peak", 
                "MFMA FLOPs (F64) Pct of Peak", 
                "MFMA IOPs (INT8) Pct of Peak"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeSL1CacheSpeedOfLight, "Scalar L1 Data Cache - Speed of Light", "% of Peak", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeSL1CacheSpeedOfLight, {
                "Bandwidth Avg", 
                "Cache Hit Rate Avg", 
                "sL1D-L2 BW Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeInstrCacheSpeedOfLight, "L1 Instruction Cache - Speed of Light", "% of Peak", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeInstrCacheSpeedOfLight, {
                "Bandwidth Avg", 
                "Cache Hit Rate Avg", 
                "L1I-L2 Bandwidth Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVL1CacheSpeedOfLight, "Vector L1 Data Cache - Speed of Light", "% of Peak", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVL1CacheSpeedOfLight, {
                "Hit rate Avg", 
                "Bandwidth Avg", 
                "Utilization Avg", 
                "Coalescing Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2NCTransactions, "Vector L1 Data Cache - L2 Cache Non-hardware-Coherent Memory (NC) Transactions", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVL1CacheL2Transactions, {
                "NC - Read Avg", 
                "NC - Write Avg", 
                "NC - Atomic Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2UCTransactions, "Vector L1 Data Cache - L2 Cache Uncached Memory (UC) Transactions", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVL1CacheL2Transactions, {
                "UC - Read Avg", 
                "UC - Write Avg", 
                "UC - Atomic Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2RWTransactions, "Vector L1 Data Cache - L2 Read/Write Coherent Memory (RW) Transactions", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVL1CacheL2Transactions, {
                "RW - Read Avg", 
                "RW - Write Avg", 
                "RW - Atomic Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVL1CacheL2CCTransactions, "Vector L1 Data Cache - L2 Coherently Cachable (CC) Transactions", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVL1CacheL2Transactions, {
                "CC - Read Avg", 
                "CC - Write Avg", 
                "CC - Atomic Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeVALUInstrMix, "Vector Arithmetic Logic Unit - Instruction Mix", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeVALUInstrMix, {
                "INT32 Avg", 
                "INT64 Avg", 
                "F16-ADD Avg", 
                "F16-MUL Avg", 
                "F16-FMA Avg", 
                "F16-Trans Avg", 
                "F32-ADD Avg", 
                "F32-MUL Avg", 
                "F32-FMA Avg", 
                "F32-Trans Avg", 
                "F64-ADD Avg", 
                "F64-MUL Avg", 
                "F64-FMA Avg", 
                "F64-Trans Avg", 
                "Conversion Avg"
            }}
        }}
    }},
    ComputeTablePlotDefinition{kRPVControllerComputePlotTypeLDSSpeedOfLight, "Local Data Share - Speed of Light", "Requests per Wave", "", {
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeLDSSpeedOfLight, {
                "Utilization Avg", 
                "Access Rate Avg", 
                "Theoretical Bandwidth (% of Peak) Avg", 
                "Bank Conflict Rate Avg"
            }}
        }}
    }}
};

const std::unordered_map<rocprofvis_controller_compute_metric_types_t, ComputeMetricDefinition> COMPUTE_METRIC_DEFINITIONS {
    {kRPVControllerComputeMetricTypeL2CacheRd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "L2 Rd Value"}},
    {kRPVControllerComputeMetricTypeL2CacheWr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "L2 Wr Value"}},
    {kRPVControllerComputeMetricTypeL2CacheAtomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "L2 Atomic Value"}},
    {kRPVControllerComputeMetricTypeL2CacheHitRate, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "L2 Hit Value"}},
    {kRPVControllerComputeMetricTypeFabricRd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric Rd Lat Value"}},
    {kRPVControllerComputeMetricTypeFabricWr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric Wr Lat Value"}},
    {kRPVControllerComputeMetricTypeFabricAtomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric Atomic Lat Value"}},
    {kRPVControllerComputeMetricTypeSL1CacheHitRate, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1D Hit Value"}},
    {kRPVControllerComputeMetricTypeInstrCacheHitRate, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "IL1 Hit Value"}},
    {kRPVControllerComputeMetricTypeInstrCacheLat, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "IL1 Lat Value"}},
    {kRPVControllerComputeMetricTypeVL1CacheHitRate, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Hit Value"}},
    {kRPVControllerComputeMetricTypeVL1CacheCoales, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Coalesce Value"}},
    {kRPVControllerComputeMetricTypeVL1CacheStall, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Stall Value"}},
    {kRPVControllerComputeMetricTypeLDSUtil, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "LDS Util Value"}},
    {kRPVControllerComputeMetricTypeLDSLat, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "LDS Latency Value"}},
    {kRPVcontrollerComputeMetricTypeLDSAlloc, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "LDS Allocation Value"}},
    {kRPVControllerComputeMetricTypeVGPR, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VGPR Value"}},
    {kRPVControllerComputeMetricTypeSGPR, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "SGPR Value"}},
    {kRPVControllerComputeMetricTypeScratchAlloc, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Scratch Allocation Value"}},
    {kRPVControllerComputeMetricTypeWavefronts, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Wavefronts Value"}},
    {kRPVControllerComputeMetricTypeWorkgroups, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Workgroups Value"}},
    {kRPVControllerComputeMetricTypeFabric_HBMRd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "HBM Rd Value"}},
    {kRPVControllerComputeMetricTypeFabric_HBMWr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "HBM Wr Value"}},
    {kRPVControllerComputeMetricTypeL2_FabricRd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric_L2 Rd Value"}},
    {kRPVControllerComputeMetricTypeL2_FabricWr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric_L2 Wr Value"}},
    {kRPVControllerComputeMetricTypeL2_FabricAtomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "Fabric_L2 Atomic Value"}},
    {kRPVControllerComputeMetricTypeVL1_L2Rd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1_L2 Rd Value"}},
    {kRPVControllerComputeMetricTypeVL1_L2Wr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1_L2 Wr Value"}},
    {kRPVControllerComputeMetricTypeVL1_L2Atomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1_L2 Atomic Value"}},
    {kRPVControllerComputeMetricTypeSL1_L2Rd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1D_L2 Rd Value"}},
    {kRPVControllerComputeMetricTypeSL1_L2Wr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1D_L2 Wr Value"}},
    {kRPVControllerComputeMetricTypeSL1_L2Atomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1D_L2 Atomic Value"}},
    {kRPVControllerComputeMetricTypeInst_L2Req, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "IL1_L2 Rd Value"}},
    {kRPVControllerComputeMetricTypeCU_LDSReq, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "LDS Req Value"}},
    {kRPVControllerComputeMetricTypeCU_VL1Rd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Rd Value"}},
    {kRPVControllerComputeMetricTypeCU_VL1Wr, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Wr Value"}},
    {kRPVControllerComputeMetricTypeCU_VL1Atomic, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1 Atomic Value"}},
    {kRPVControllerComputeMetricTypeCU_SL1Rd, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "VL1D Rd Value"}},
    {kRPVControllerComputeMetricTypeCU_InstrReq, ComputeMetricDefinition{kRPVControllerComputeTableTypeMemoryChart, "IL1 Fetch Value"}}
};

const ComputeRooflineDefinition ROOFLINE_DEFINITION {
    {
        {ComputeRooflineDefinition::MemoryLevelHBM, "HBM"},
        {ComputeRooflineDefinition::MemoryLevelL2, "L2"},
        {ComputeRooflineDefinition::MemoryLevelL1, "L1"},
        {ComputeRooflineDefinition::MemoryLevelLDS, "LDS"}
    },
    {
        {ComputeRooflineDefinition::FormatFP64, "FP64"},
        {ComputeRooflineDefinition::FormatFP32, "FP32"},
        {ComputeRooflineDefinition::FormatFP16, "FP16"},
        {ComputeRooflineDefinition::FormatINT8, "I8"}
    },
    {
        {ComputeRooflineDefinition::PipeVALU, "Peak VALU"},
        {ComputeRooflineDefinition::PipeMFMA, "Peak MFMA"}
    },
    {
        {ComputeRooflineDefinition::FormatFP64, {
            {ComputeRooflineDefinition::PipeVALU, "FP64Flops"},
            {ComputeRooflineDefinition::PipeMFMA, "MFMAF64Flops"}
        }},
        {ComputeRooflineDefinition::FormatFP32, {
            {ComputeRooflineDefinition::PipeVALU, "FP32Flops"},
            {ComputeRooflineDefinition::PipeMFMA, "MFMAF32Flops"}
        }},
        {ComputeRooflineDefinition::FormatFP16, {
            {ComputeRooflineDefinition::PipeMFMA, "MFMAF16Flops"}
        }},
        {ComputeRooflineDefinition::FormatINT8, {
            {ComputeRooflineDefinition::PipeMFMA, "MFMAI8Ops"}
        }},
    },
    {
        {kRPVControllerComputePlotTypeRooflineFP64, ComputeRooflineDefinition::ComputeRooflinePlotDefinition{
            kRPVControllerComputePlotTypeRooflineFP64, "Emperical Roofline Analysis (FP64)", "Arithmetic Intensity (FLOP/Byte)", "Performance (GFLOP/s)", {
                ComputeRooflineDefinition::FormatFP64
            }
        }},
        {kRPVControllerComputePlotTypeRooflineFP32, ComputeRooflineDefinition::ComputeRooflinePlotDefinition{
            kRPVControllerComputePlotTypeRooflineFP32, "Emperical Roofline Analysis (FP32)", "Arithmetic Intensity (FLOP/Byte)", "Performance (GFLOP/s)", {
                ComputeRooflineDefinition::FormatFP32
            }
        }},
        {kRPVControllerComputePlotTypeRooflineFP16, ComputeRooflineDefinition::ComputeRooflinePlotDefinition{
            kRPVControllerComputePlotTypeRooflineFP16, "Emperical Roofline Analysis (FP16)", "Arithmetic Intensity (FLOP/Byte)", "Performance (GFLOP/s)", {
                ComputeRooflineDefinition::FormatFP16
            }
        }},
        {kRPVControllerComputePlotTypeRooflineINT8, ComputeRooflineDefinition::ComputeRooflinePlotDefinition{
            kRPVControllerComputePlotTypeRooflineINT8, "Emperical Roofline Analysis (INT8)", "Arithmetic Intensity (IOP/Byte)", "Performance (GIOP/s)", {
                ComputeRooflineDefinition::FormatINT8
            }
        }}
    },
    0, 0.01, 1000
};

}
}

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
    std::vector<ComputePlotDataSeriesDefinition> m_series;
} ComputePlotDefinition;


const std::unordered_map<std::string, ComputeTableDefinition> COMPUTE_TABLE_DEFINITIONS { 
    {"0.1_Top_Kernels.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeKernelList, "Kernel List"}},
    {"0.2_Dispatch_List.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeDispatchList, "Dispatch List"}},
    {"1.1.csv", ComputeTableDefinition{kRPVControllerComputeTableTypeSysInfo, "System Info"}},
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
    {"18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv", ComputeTableDefinition{kRPVControllerComputeTableTypeL2Cache128Reqs, "L2 Cache - Fabric 128B Read Requests Per Channel"}}
};

const std::array COMPUTE_PLOT_DEFINITIONS {  
    ComputePlotDefinition{kRPVControllerComputePlotTypeKernelDurationPercentage, "Kernel Durations", "Duration (%)", "Kernel", {  
        ComputePlotDataSeriesDefinition{"", {
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeKernelList, {"{Pct}"}}
        }}  
    }},
    ComputePlotDefinition{kRPVControllerComputePlotTypeKernelDuration, "Kernel Durations", "Mean Duration (ns)", "Kernel", {  
        ComputePlotDataSeriesDefinition{"", { 
            ComputePlotDataDefinition{kRPVControllerComputeTableTypeKernelList, {"{Mean(ns)}"}}  
        }}
    }},
}; 

}
}

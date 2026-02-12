// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

typedef enum rocprofvis_controller_node_properties_t : uint32_t
{
    __kRPVControllerNodePropertiesFirst = 0xC0000000,
    kRPVControllerNodeId                = __kRPVControllerNodePropertiesFirst,
    kRPVControllerNodeHostName,
    kRPVControllerNodeDomainName,
    kRPVControllerNodeOSName,
    kRPVControllerNodeOSRelease,
    kRPVControllerNodeOSVersion,
    kRPVControllerNodeHardwareName,
    kRPVControllerNodeMachineId,
    kRPVControllerNodeNumProcessors,
    kRPVControllerNodeProcessorIndexed,
    kRPVControllerNodeNumProcesses,
    kRPVControllerNodeProcessIndexed,
    kRPVControllerNodeMachineGuid,
    kRPVControllerNodeHash,
    __kRPVControllerNodePropertiesLast
} rocprofvis_controller_node_properties_t;

typedef enum rocprofvis_controller_processor_properties_t : uint32_t
{
    __kRPVControllerProcessorPropertiesFirst = 0xD0000000,
    kRPVControllerProcessorId                = __kRPVControllerProcessorPropertiesFirst,
    kRPVControllerProcessorName,
    kRPVControllerProcessorModelName,
    kRPVControllerProcessorUserName,
    kRPVControllerProcessorVendorName,
    kRPVControllerProcessorProductName,
    kRPVControllerProcessorExtData,
    kRPVControllerProcessorUUID,
    kRPVControllerProcessorType,
    kRPVControllerProcessorTypeIndex,
    kRPVControllerProcessorNodeId,
    kRPVControllerProcessorNumQueues,
    kRPVControllerProcessorNumCounters,
    kRPVControllerProcessorQueueIndexed,
    kRPVControllerProcessorCounterIndexed,
    kRPVControllerProcessorIndex,
    kRPVControllerProcessorLogicalIndex,
    __kRPVControllerProcessorPropertiesLast
} rocprofvis_controller_processor_properties_t;


typedef enum rocprofvis_controller_processor_type_t
{
    kRPVControllerProcessorTypeUndefined = 0,
    kRPVControllerProcessorTypeGPU       = 1,
    kRPVControllerProcessorTypeCPU       = 2,
} rocprofvis_controller_processor_type_t;

typedef enum rocprofvis_controller_thread_properties_t : uint32_t
{
    __kRPVControllerThreadPropertiesFirst = 0xF2000000,
    kRPVControllerThreadId                = __kRPVControllerThreadPropertiesFirst,
    kRPVControllerThreadNode,
    kRPVControllerThreadProcess,
    kRPVControllerThreadParentId,
    kRPVControllerThreadTid,
    kRPVControllerThreadName,
    kRPVControllerThreadExtData,
    kRPVControllerThreadStartTime,
    kRPVControllerThreadEndTime,
    kRPVControllerThreadTrack,
    kRPVControllerThreadType,
    __kRPVControllerThreadPropertiesLast
} rocprofvis_controller_thread_properties_t;

typedef enum rocprofvis_controller_thread_type_t
{
    kRPVControllerThreadTypeUndefined    = 0,
    kRPVControllerThreadTypeInstrumented = 1,
    kRPVControllerThreadTypeSampled      = 2,
} rocprofvis_controller_thread_type_t;

typedef enum rocprofvis_controller_queue_properties_t : uint32_t
{
    __kRPVControllerQueuePropertiesFirst = 0xF3000000,
    kRPVControllerQueueId                = __kRPVControllerQueuePropertiesFirst,
    kRPVControllerQueueNode,
    kRPVControllerQueueProcess,
    __kRPVControllerQueueReserved0,
    kRPVControllerQueueName,
    kRPVControllerQueueExtData,
    kRPVControllerQueueProcessor,
    kRPVControllerQueueTrack,
    __kRPVControllerQueuePropertiesLast
} rocprofvis_controller_queue_properties_t;

typedef enum rocprofvis_controller_counter_properties_t : uint32_t
{
    __kRPVControllerCounterPropertiesFirst = 0xF5000000,
    kRPVControllerCounterId                = __kRPVControllerCounterPropertiesFirst,
    kRPVControllerCounterNode,
    kRPVControllerCounterProcess,
    kRPVControllerCounterProcessor,
    kRPVControllerCounterName,
    kRPVControllerCounterSymbol,
    kRPVControllerCounterDescription,
    kRPVControllerCounterExtendedDesc,
    kRPVControllerCounterComponent,
    kRPVControllerCounterUnits,
    kRPVControllerCounterValueType,
    kRPVControllerCounterBlock,
    kRPVControllerCounterExpression,
    kRPVControllerCounterGuid,
    kRPVControllerCounterExtData,
    kRPVControllerCounterTargetArch,
    kRPVControllerCounterEventCode,
    kRPVControllerCounterInstanceId,
    kRPVControllerCounterIsConstant,
    kRPVControllerCounterIsDerived,
    kRPVControllerCounterTrack,
    __kRPVControllerCounterPropertiesLast
} rocprofvis_controller_counter_properties_t;

typedef enum rocprofvis_controller_stream_properties_t : uint32_t
{
    __kRPVControllerStreamPropertiesFirst = 0xF4000000,
    kRPVControllerStreamId      = __kRPVControllerStreamPropertiesFirst,
    kRPVControllerStreamNode,
    kRPVControllerStreamProcess,
    kRPVControllerStreamName,
    kRPVControllerStreamExtData,
    kRPVControllerStreamProcessor,
    kRPVControllerStreamNumProcessors,
    kRPVControllerStreamProcessorIndexed,
    kRPVControllerStreamTrack,
    __kRPVControllerStreamPropertiesLast
} rocprofvis_controller_stream_properties_t;

typedef enum rocprofvis_controller_process_properties_t : uint32_t
{
    __kRPVControllerProcessPropertiesFirst = 0xF1000000,
    kRPVControllerProcessId      = __kRPVControllerProcessPropertiesFirst,
    kRPVControllerProcessNodeId,
    kRPVControllerProcessInitTime,
    kRPVControllerProcessFinishTime,
    kRPVControllerProcessStartTime,
    kRPVControllerProcessEndTime,
    kRPVControllerProcessCommand,
    kRPVControllerProcessEnvironment,
    kRPVControllerProcessExtData,
    kRPVControllerProcessParentId,
    kRPVControllerProcessNumThreads,
    kRPVControllerProcessNumQueues,
    kRPVControllerProcessNumStreams,
    kRPVControllerProcessThreadIndexed,
    kRPVControllerProcessQueueIndexed,
    kRPVControllerProcessStreamIndexed,
    kRPVControllerProcessNumCounters,
    kRPVControllerProcessCounterIndexed,
    __kRPVControllerProcessPropertiesLast,
} rocprofvis_controller_process_properties_t;

typedef enum rocprofvis_controller_topology_node_properties_t : uint32_t
{
    __kRPVControllerTopologyNodePropertiesFirst = 0xF1000000,
    kRPVControllerTopologyNodeTrackId = __kRPVControllerProcessPropertiesFirst,
    kRPVControllerTopologyNodeType,
    kRPVControllerTopologyNodeName,
    kRPVControllerTopologyNodeTrack,
    kRPVControllerTopologyNodeNumChildren,
    kRPVControllerTopologyNodeChildHandleIndexed,
    kRPVControllerTopologyNodeNumProperties,
    kRPVControllerTopologyNodePropertyKeyIndexed,
    kRPVControllerTopologyNodePropertyTypeIndexed,
    kRPVControllerTopologyNodePropertyValueIndexed,
    kRPVControllerTopologyNodePropertyTypeKeyed,
    kRPVControllerTopologyNodePropertyValueKeyed,
    __kRPVControllerTopologyNodePropertiesLast,
} rocprofvis_controller_topology_node_properties_t;

typedef enum rocprofvis_controller_topology_node_type_t : uint32_t
{
    __kRPVControllerTopologyNodeTypeFirst = 0xFF000000,
    kRPVControllerTopologyNodeRoot      = __kRPVControllerProcessPropertiesFirst,
    kRPVControllerTopologyNodeSytemNode,
    kRPVControllerTopologyNodeProcess, 
    kRPVControllerTopologyNodeThread, 
    kRPVControllerTopologyNodeProcessor, 
    kRPVControllerTopologyNodeQueue, 
    kRPVControllerTopologyNodeStream, 
    kRPVControllerTopologyNodeCounter, 
    __kRPVControllerTopologyNodeTypeLast,
} rocprofvis_controller_topology_node_type_t;
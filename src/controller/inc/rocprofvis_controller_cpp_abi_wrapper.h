// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace RocProfVis
{
namespace Controller
{
namespace Abi
{

// Enumeration validators shared by the typed property declarations and access
// functions.
inline bool IsValidTrackType(rocprofvis_controller_track_type_t type) noexcept
{
    return type == kRPVControllerTrackTypeSamples ||
           type == kRPVControllerTrackTypeEvents;
}

inline bool IsValidGraphType(rocprofvis_controller_graph_type_t type) noexcept
{
    return type == kRPVControllerGraphTypeLine ||
           type == kRPVControllerGraphTypeFlame;
}

inline bool IsValidProcessorType(
    rocprofvis_controller_processor_type_t type) noexcept
{
    return type >= kRPVControllerProcessorTypeUndefined &&
           type <= kRPVControllerProcessorTypeNIC;
}

inline bool IsValidSummaryAggregationLevel(
    rocprofvis_controller_summary_aggregation_level_t level) noexcept
{
    return level >= __kRPVControllerSummaryAggregationLevelFirst &&
           level < __kRPVControllerSummaryAggregationLevelLast;
}

inline bool IsValidSortOrder(rocprofvis_controller_sort_order_t order) noexcept
{
    return order == kRPVControllerSortOrderAscending ||
           order == kRPVControllerSortOrderDescending;
}

inline bool IsValidTableType(rocprofvis_controller_table_type_t type) noexcept
{
    return type >= kRPVControllerTableTypeEvents &&
           type <= kRPVControllerTableTypeSampledEvents;
}

inline bool IsValidRooflineComputeType(
    rocprofvis_controller_roofline_ceiling_compute_type_t type) noexcept
{
    return type >= __KRPVControllerRooflineCeilingComputeTypeFirst &&
           type < __KRPVControllerRooflineCeilingComputeTypeLast;
}

inline bool IsValidRooflineBandwidthType(
    rocprofvis_controller_roofline_ceiling_bandwidth_type_t type) noexcept
{
    return type >= __KRPVControllerRooflineCeilingBandwidthTypeFirst &&
           type < __KRPVControllerRooflineCeilingBandwidthTypeLast;
}

inline bool IsValidRooflineIntensityType(
    rocprofvis_controller_roofline_kernel_intensity_type_t type) noexcept
{
    return type >= kRPVControllerRooflineKernelIntensityTypeHBM &&
           type <= kRPVControllerRooflineKernelIntensityTypeLDS;
}

inline bool IsValidMetricSourceType(
    rocprofvis_controller_metric_source_type_t type) noexcept
{
    return type == kRPVControllerMetricSourceTypeWorkload ||
           type == kRPVControllerMetricSourceTypeKernel;
}

inline bool IsValidPrimitiveType(
    rocprofvis_controller_primitive_type_t type) noexcept
{
    return type >= kRPVControllerPrimitiveTypeUInt64 &&
           type <= kRPVControllerPrimitiveTypeObject;
}

inline bool IsValidRemoteStatus(
    rocprofvis_controller_remote_status_t status) noexcept
{
    return status >= kRPVControllerSshIdle &&
           status <= kRPVControllerSshCompleted;
}

inline bool IsValidUserPromptType(
    rocprofvis_controller_user_prompt_type_t type) noexcept
{
    return type == kRPVControllerUserPromptTypeGeneric ||
           type == kRPVControllerUserPromptTypeHostKey;
}

// Property tag infrastructure that binds each C ABI property to its semantic
// C++ value type and, for enumerations, its value validator.
template<typename T>
inline constexpr bool IsSupportedUnsigned =
    std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
    std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>;

template<rocprofvis_property_t Property, typename T>
struct UnsignedPropertyTag
{
    static_assert(IsSupportedUnsigned<T>,
                  "Controller integer properties require an unsigned ABI type");

    using ValueType = T;
    static constexpr rocprofvis_property_t kProperty = Property;
};

template<rocprofvis_property_t Property>
struct BooleanPropertyTag
{
    static constexpr rocprofvis_property_t kProperty = Property;
};

template<rocprofvis_property_t Property, typename Enum, auto Validator>
struct EnumPropertyTag
{
    static_assert(std::is_enum_v<Enum>, "Controller enum properties require an enum type");

    using ValueType = Enum;
    static constexpr rocprofvis_property_t kProperty = Property;

    static bool IsValid(Enum value) noexcept
    {
        return Validator(value);
    }
};

// Typed property catalogue. The aliases are grouped by the controller objects
// and request structures on which the properties are used.
namespace Property
{
// Event properties.
using EventLevel = UnsignedPropertyTag<kRPVControllerEventLevel, uint8_t>;
using EventNumChildren =
    UnsignedPropertyTag<kRPVControllerEventNumChildren, uint32_t>;
using EventArgumentPosition =
    UnsignedPropertyTag<kRPVControllerEventArgumentPosition, uint32_t>;

// Workload properties.
using WorkloadId = UnsignedPropertyTag<kRPVControllerWorkloadId, uint32_t>;
using WorkloadAvailableMetricCategoryId = UnsignedPropertyTag<
    kRPVControllerWorkloadAvailableMetricCategoryIdIndexed, uint32_t>;
using WorkloadAvailableMetricTableId = UnsignedPropertyTag<
    kRPVControllerWorkloadAvailableMetricTableIdIndexed, uint32_t>;
using WorkloadMetricValueNameCategoryId = UnsignedPropertyTag<
    kRPVControllerWorkloadMetricValueNameCategoryIdIndexed, uint32_t>;
using WorkloadMetricValueNameTableId = UnsignedPropertyTag<
    kRPVControllerWorkloadMetricValueNameTableIdIndexed, uint32_t>;

// Kernel properties.
using KernelId = UnsignedPropertyTag<kRPVControllerKernelId, uint32_t>;
using KernelInvocationCount =
    UnsignedPropertyTag<kRPVControllerKernelInvocationCount, uint32_t>;
using KernelDurationMin =
    UnsignedPropertyTag<kRPVControllerKernelDurationMin, uint32_t>;
using KernelDurationMax =
    UnsignedPropertyTag<kRPVControllerKernelDurationMax, uint32_t>;
using KernelDurationMean =
    UnsignedPropertyTag<kRPVControllerKernelDurationMean, uint32_t>;
using KernelDurationMedian =
    UnsignedPropertyTag<kRPVControllerKernelDurationMedian, uint32_t>;

// Compute request argument properties.
using MetricArgsWorkloadId =
    UnsignedPropertyTag<kRPVControllerMetricArgsWorkloadId, uint32_t>;
using MetricArgsKernelId =
    UnsignedPropertyTag<kRPVControllerMetricArgsKernelIdIndexed, uint32_t>;
using MetricArgsCategoryId = UnsignedPropertyTag<
    kRPVControllerMetricArgsMetricCategoryIdIndexed, uint32_t>;
using MetricArgsTableId = UnsignedPropertyTag<
    kRPVControllerMetricArgsMetricTableIdIndexed, uint32_t>;
using MetricArgsEntryId = UnsignedPropertyTag<
    kRPVControllerMetricArgsMetricEntryIdIndexed, uint32_t>;
using PcSamplingArgsWorkloadId = UnsignedPropertyTag<
    kRPVControllerPcSamplingArgsWorkloadId, uint32_t>;
using PcSamplingArgsKernelId =
    UnsignedPropertyTag<kRPVControllerPcSamplingArgsKernelId, uint32_t>;
using PcSamplingArgsSourceFileId = UnsignedPropertyTag<
    kRPVControllerPcSamplingArgsSourceFileId, uint32_t>;
using ComputePivotWorkloadId =
    UnsignedPropertyTag<kRPVControllerCPTArgsWorkloadId, uint32_t>;
using ComputePivotSortOrder = EnumPropertyTag<
    kRPVControllerCPTArgsSortOrder, rocprofvis_controller_sort_order_t,
    IsValidSortOrder>;

// Metric container properties.
using MetricsContainerWorkloadId = UnsignedPropertyTag<
    kRPVControllerMetricsContainerWorkloadIdIndexed, uint32_t>;
using MetricsContainerKernelId = UnsignedPropertyTag<
    kRPVControllerMetricsContainerKernelIdIndexed, uint32_t>;
using MetricSourceType = EnumPropertyTag<
    kRPVControllerMetricsContainerMetricSourceTypeIndexed,
    rocprofvis_controller_metric_source_type_t, IsValidMetricSourceType>;

// PC sampling properties.
using PcSamplingCodeObjectId =
    UnsignedPropertyTag<kRPVControllerPCSamplingCodeObjectId, uint32_t>;
using PcSamplingIsaLineId =
    UnsignedPropertyTag<kRPVControllerPCSamplingIsaLineId, uint32_t>;
using PcSamplingIsaLineCodeObjectId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaLineCodeObjectId, uint32_t>;
using PcSamplingIsaLineInstructionTypeId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaLineInstructionTypeId, uint32_t>;
using PcSamplingIsaToIsaDependentIsaLineId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaToIsaDependentIsaLineId, uint32_t>;
using PcSamplingIsaToIsaDependencyIsaLineId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaToIsaDependencyIsaLineId, uint32_t>;
using PcSamplingIsaToSourceIsaLineId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaToSourceIsaLineId, uint32_t>;
using PcSamplingIsaToSourceSourceLineId = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaToSourceSourceLineId, uint32_t>;
using PcSamplingIsaToSourceDepth = UnsignedPropertyTag<
    kRPVControllerPCSamplingIsaToSourceDepth, uint32_t>;
using PcSamplingStateId =
    UnsignedPropertyTag<kRPVControllerPCSamplingStateId, uint32_t>;
using PcSamplingStateIsaLineId =
    UnsignedPropertyTag<kRPVControllerPCSamplingStateIsaLineId, uint32_t>;
using PcSamplingStateIssuedCount = UnsignedPropertyTag<
    kRPVControllerPCSamplingStateIssuedCount, uint32_t>;
using PcSamplingStateStalledCount = UnsignedPropertyTag<
    kRPVControllerPCSamplingStateStalledCount, uint32_t>;
using PcSamplingStateTotalCount = UnsignedPropertyTag<
    kRPVControllerPCSamplingStateTotalCount, uint32_t>;
using PcSamplingStallReasonSamplingStateId = UnsignedPropertyTag<
    kRPVControllerPCSamplingStallReasonSamplingStateId, uint32_t>;
using PcSamplingStallReasonId = UnsignedPropertyTag<
    kRPVControllerPCSamplingStallReasonId, uint32_t>;
using PcSamplingStallReasonCount = UnsignedPropertyTag<
    kRPVControllerPCSamplingStallReasonCount, uint32_t>;
using PcSamplingSourceFileId = UnsignedPropertyTag<
    kRPVControllerPCSamplingSourceFileId, uint32_t>;
using PcSamplingSourceLineId = UnsignedPropertyTag<
    kRPVControllerPCSamplingSourceLineId, uint32_t>;
using PcSamplingSourceLineSourceFileId = UnsignedPropertyTag<
    kRPVControllerPCSamplingSourceLineSourceFileId, uint32_t>;
using PcSamplingSourceLineNumber = UnsignedPropertyTag<
    kRPVControllerPCSamplingSourceLineNumber, uint32_t>;

// Roofline properties.
using RooflineKernelId =
    UnsignedPropertyTag<kRPVControllerRooflineKernelIdIndexed, uint32_t>;
using RooflineRidgeComputeType = EnumPropertyTag<
    kRPVControllerRooflineCeilingRidgeComputeTypeIndexed,
    rocprofvis_controller_roofline_ceiling_compute_type_t,
    IsValidRooflineComputeType>;
using RooflineRidgeBandwidthType = EnumPropertyTag<
    kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed,
    rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
    IsValidRooflineBandwidthType>;
using RooflineCeilingComputeType = EnumPropertyTag<
    kRPVControllerRooflineCeilingComputeTypeIndexed,
    rocprofvis_controller_roofline_ceiling_compute_type_t,
    IsValidRooflineComputeType>;
using RooflineCeilingBandwidthType = EnumPropertyTag<
    kRPVControllerRooflineCeilingBandwidthTypeIndexed,
    rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
    IsValidRooflineBandwidthType>;
using RooflineKernelIntensityType = EnumPropertyTag<
    kRPVControllerRooflineKernelIntensityTypeIndexed,
    rocprofvis_controller_roofline_kernel_intensity_type_t,
    IsValidRooflineIntensityType>;

// Table properties and table request arguments.
using TableArgsNumTracks =
    UnsignedPropertyTag<kRPVControllerTableArgsNumTracks, uint16_t>;
using TableArgsNumOpTypes =
    UnsignedPropertyTag<kRPVControllerTableArgsNumOpTypes, uint16_t>;
using TableType = EnumPropertyTag<kRPVControllerTableArgsType,
                                 rocprofvis_controller_table_type_t,
                                 IsValidTableType>;
using TableSortOrder = EnumPropertyTag<kRPVControllerTableArgsSortOrder,
                                      rocprofvis_controller_sort_order_t,
                                      IsValidSortOrder>;
using TableColumnType = EnumPropertyTag<kRPVControllerTableColumnTypeIndexed,
                                       rocprofvis_controller_primitive_type_t,
                                       IsValidPrimitiveType>;

// System trace visualization properties.
using SystemNotifySelected =
    BooleanPropertyTag<kRPVControllerSystemNotifySelected>;
using TrackType = EnumPropertyTag<kRPVControllerTrackType,
                                 rocprofvis_controller_track_type_t,
                                 IsValidTrackType>;
using GraphType = EnumPropertyTag<kRPVControllerGraphType,
                                 rocprofvis_controller_graph_type_t,
                                 IsValidGraphType>;
using ProcessorType = EnumPropertyTag<kRPVControllerProcessorType,
                                     rocprofvis_controller_processor_type_t,
                                     IsValidProcessorType>;

// Summary metric properties.
using SummaryAggregationLevel = EnumPropertyTag<
    kRPVControllerSummaryMetricPropertyAggregationLevel,
    rocprofvis_controller_summary_aggregation_level_t,
    IsValidSummaryAggregationLevel>;
using SummaryProcessorType = EnumPropertyTag<
    kRPVControllerSummaryMetricPropertyProcessorType,
    rocprofvis_controller_processor_type_t, IsValidProcessorType>;

// Remote session and user prompt properties.
using RemotePort =
    UnsignedPropertyTag<kRPVControllerRemoteTypePort, uint16_t>;
using RemoteDirection =
    UnsignedPropertyTag<kRPVControllerRemoteTypeDirection, uint8_t>;
using RemoteHostKeyPromptPort = UnsignedPropertyTag<
    kRPVControllerRemoteUserHostKeyPromptPort, uint16_t>;
using RemoteHostKeyPromptState =
    BooleanPropertyTag<kRPVControllerRemoteUserHostKeyPromptState>;
using RemoteGenericPromptEcho = BooleanPropertyTag<
    kRPVControllerRemoteUserGenericPromptEchoIndexed>;
using RemoteStatus = EnumPropertyTag<kRPVControllerRemoteStatus,
                                    rocprofvis_controller_remote_status_t,
                                    IsValidRemoteStatus>;
using RemoteUserPromptType = EnumPropertyTag<
    kRPVControllerRemoteUserPromptType,
    rocprofvis_controller_user_prompt_type_t, IsValidUserPromptType>;

// Extended-data properties with their supported semantic integer types.
using ExtDataUInt8Value =
    UnsignedPropertyTag<kRPVControllerExtDataValue, uint8_t>;
using ExtDataUInt32Value =
    UnsignedPropertyTag<kRPVControllerExtDataValue, uint32_t>;
}

// Checked unsigned-integer conversion and C ABI access.
template<typename T>
rocprofvis_result_t CheckedAssignUnsigned(uint64_t value, T* output) noexcept
{
    static_assert(IsSupportedUnsigned<T>,
                  "Controller integer access supports uint8_t, uint16_t, uint32_t, and uint64_t");

    if(!output)
    {
        return kRocProfVisResultInvalidArgument;
    }
    if(value > static_cast<uint64_t>(std::numeric_limits<T>::max()))
    {
        return kRocProfVisResultOutOfRange;
    }

    *output = static_cast<T>(value);
    return kRocProfVisResultSuccess;
}

template<typename T>
rocprofvis_result_t GetUnsigned(rocprofvis_handle_t* object,
                               rocprofvis_property_t property, uint64_t index,
                               T* value) noexcept
{
    static_assert(IsSupportedUnsigned<T>,
                  "Controller integer access supports uint8_t, uint16_t, uint32_t, and uint64_t");

    if(!value)
    {
        return kRocProfVisResultInvalidArgument;
    }

    uint64_t raw_value = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_uint64(object, property, index, &raw_value);
    if(result == kRocProfVisResultSuccess)
    {
        T converted{};
        result = CheckedAssignUnsigned(raw_value, &converted);
        if(result == kRocProfVisResultSuccess)
        {
            *value = converted;
        }
    }
    return result;
}

template<typename T>
rocprofvis_result_t SetUnsigned(rocprofvis_handle_t* object,
                               rocprofvis_property_t property, uint64_t index,
                               T value) noexcept
{
    static_assert(IsSupportedUnsigned<T>,
                  "Controller integer access supports uint8_t, uint16_t, uint32_t, and uint64_t");

    return rocprofvis_controller_set_uint64(object, property, index,
                                            static_cast<uint64_t>(value));
}

template<typename PropertyTag>
rocprofvis_result_t GetUnsigned(
    rocprofvis_handle_t* object, uint64_t index,
    typename PropertyTag::ValueType* value) noexcept
{
    return GetUnsigned(object, PropertyTag::kProperty, index, value);
}

template<typename PropertyTag>
rocprofvis_result_t SetUnsigned(
    rocprofvis_handle_t* object, uint64_t index,
    typename PropertyTag::ValueType value) noexcept
{
    return SetUnsigned(object, PropertyTag::kProperty, index, value);
}

// Checked Boolean conversion and C ABI access. Only ABI values zero and one
// are accepted as Boolean values.
inline rocprofvis_result_t CheckedAssignBoolean(uint64_t value, bool* output) noexcept
{
    if(!output)
    {
        return kRocProfVisResultInvalidArgument;
    }
    if(value > 1)
    {
        return kRocProfVisResultOutOfRange;
    }

    *output = value != 0;
    return kRocProfVisResultSuccess;
}

inline rocprofvis_result_t GetBoolean(rocprofvis_handle_t* object,
                                     rocprofvis_property_t property, uint64_t index,
                                     bool* value) noexcept
{
    if(!value)
    {
        return kRocProfVisResultInvalidArgument;
    }

    uint64_t raw_value = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_uint64(object, property, index, &raw_value);
    if(result == kRocProfVisResultSuccess)
    {
        bool converted = false;
        result = CheckedAssignBoolean(raw_value, &converted);
        if(result == kRocProfVisResultSuccess)
        {
            *value = converted;
        }
    }
    return result;
}

inline rocprofvis_result_t SetBoolean(rocprofvis_handle_t* object,
                                     rocprofvis_property_t property, uint64_t index,
                                     bool value) noexcept
{
    return rocprofvis_controller_set_uint64(object, property, index, value ? 1 : 0);
}

template<typename PropertyTag>
rocprofvis_result_t GetBoolean(rocprofvis_handle_t* object, uint64_t index,
                              bool* value) noexcept
{
    return GetBoolean(object, PropertyTag::kProperty, index, value);
}

template<typename PropertyTag>
rocprofvis_result_t SetBoolean(rocprofvis_handle_t* object, uint64_t index,
                              bool value) noexcept
{
    return SetBoolean(object, PropertyTag::kProperty, index, value);
}

// Checked enumeration conversion and C ABI access. Each operation applies the
// validator associated with the requested enumeration property.
template<typename Enum, typename Validator>
rocprofvis_result_t CheckedAssignEnum(uint64_t value, Enum* output,
                                     Validator&& validator) noexcept
{
    static_assert(std::is_enum_v<Enum>, "Enum access requires an enum type");

    if(!output)
    {
        return kRocProfVisResultInvalidArgument;
    }

    using Underlying = std::underlying_type_t<Enum>;
    if(value > static_cast<uint64_t>(std::numeric_limits<Underlying>::max()))
    {
        return kRocProfVisResultOutOfRange;
    }

    Enum converted = static_cast<Enum>(static_cast<Underlying>(value));
    if(!validator(converted))
    {
        return kRocProfVisResultInvalidEnum;
    }

    *output = converted;
    return kRocProfVisResultSuccess;
}

template<typename Enum, typename Validator>
rocprofvis_result_t GetEnum(rocprofvis_handle_t* object,
                           rocprofvis_property_t property, uint64_t index,
                           Enum* value, Validator&& validator) noexcept
{
    static_assert(std::is_enum_v<Enum>, "Enum access requires an enum type");

    if(!value)
    {
        return kRocProfVisResultInvalidArgument;
    }

    uint64_t raw_value = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_uint64(object, property, index, &raw_value);
    if(result == kRocProfVisResultSuccess)
    {
        Enum converted{};
        result = CheckedAssignEnum(raw_value, &converted,
                                   std::forward<Validator>(validator));
        if(result == kRocProfVisResultSuccess)
        {
            *value = converted;
        }
    }
    return result;
}

template<typename Enum, typename Validator>
rocprofvis_result_t SetEnum(rocprofvis_handle_t* object,
                           rocprofvis_property_t property, uint64_t index,
                           Enum value, Validator&& validator) noexcept
{
    static_assert(std::is_enum_v<Enum>, "Enum access requires an enum type");

    if(!validator(value))
    {
        return kRocProfVisResultInvalidEnum;
    }

    using Underlying = std::underlying_type_t<Enum>;
    const Underlying raw_value = static_cast<Underlying>(value);
    if constexpr(std::is_signed_v<Underlying>)
    {
        if(raw_value < 0)
        {
            return kRocProfVisResultInvalidEnum;
        }
    }

    return rocprofvis_controller_set_uint64(
        object, property, index, static_cast<uint64_t>(raw_value));
}

template<typename PropertyTag>
rocprofvis_result_t GetEnum(
    rocprofvis_handle_t* object, uint64_t index,
    typename PropertyTag::ValueType* value) noexcept
{
    return GetEnum(object, PropertyTag::kProperty, index, value,
                   PropertyTag::IsValid);
}

template<typename PropertyTag>
rocprofvis_result_t SetEnum(
    rocprofvis_handle_t* object, uint64_t index,
    typename PropertyTag::ValueType value) noexcept
{
    return SetEnum(object, PropertyTag::kProperty, index, value,
                   PropertyTag::IsValid);
}

}
}
}

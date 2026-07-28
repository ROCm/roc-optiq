// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_query_factory.h"
#include "rocprofvis_db_sqlite.h"
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{
QueryFactory::QueryFactory(ProfileDatabase* db)
: m_db(db)
{}

/*************************************************************************************************
 *                               Track build queries for CPU/GPU tracks
 **************************************************************************************************/

std::string
QueryFactory::GetRocprofRegionTrackQuery(bool is_sample_track)
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_region_track_query_format({
            { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
              Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
              Builder::QParam("T.tid", Builder::THREAD_ID_SERVICE_NAME),
              Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
              Builder::QParamCategory(is_sample_track ? kRocProfVisDmRegionSampleTrack
                                                      : kRocProfVisDmRegionMainTrack),
              Builder::QParamOperation(is_sample_track
                                           ? kRocProfVisDmOperationLaunchSample
                                           : kRocProfVisDmOperationLaunch),
              Builder::StoreConfigVersion() },

            { Builder::From("rocpd_region", "R"),
              Builder::InnerJoin("rocpd_track", "T", "T.id = R.track_id"),
              is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                   "SAMPLE.event_id = R.event_id")
                              : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                  "SAMPLE.event_id = R.event_id") },
            { is_sample_track ? Builder::Blank()
                              : Builder::Where("SAMPLE.id", " IS ", "NULL") },
        }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_region_track_query_format({
            { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
              Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
              Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
              Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
              Builder::QParamCategory(is_sample_track ? kRocProfVisDmRegionSampleTrack
                                                      : kRocProfVisDmRegionMainTrack),
              Builder::QParamOperation(is_sample_track
                                           ? kRocProfVisDmOperationLaunchSample
                                           : kRocProfVisDmOperationLaunch),
              Builder::StoreConfigVersion() },
            { Builder::From("rocpd_region", "R"),
              is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                   "SAMPLE.event_id = R.event_id")
                              : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                  "SAMPLE.event_id = R.event_id") },
            { is_sample_track ? Builder::Blank()
                              : Builder::Where("SAMPLE.id", " IS ", "NULL") },
        }));
    }
}

std::string
QueryFactory::GetRocprofKernelDispatchTrackQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack),
                Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack),
                Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_kernel_dispatch") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocTrackQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryAllocationTrack),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::StoreConfigVersion() },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryAllocationTrack),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::StoreConfigVersion() },
              { Builder::From("roc_optiq_memory_allocate") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopyTrackQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryCopyTrack),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryCopyTrack),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_memory_copy") } }));
    }
}

std::string
QueryFactory::GetRocprofPerformanceCountersTrackQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack),
                Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_pmc_event", "PMC_E"),
                Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                   "K.event_id = PMC_E.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("K.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack),
                Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_pmc_event", "PMC_E"),
                Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                   "K.event_id = PMC_E.event_id") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryActivitySubQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_mem_act_subquery_format(
            { { Builder::QParam("T.value", "value") },
              { Builder::From("roc_optiq_memory_allocate", "MA"),
                Builder::InnerJoin("rocpd_timestamp", "T", "MA.start_id == T.id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_mem_act_subquery_format(
            { { Builder::QParam("start", "value") },
              { Builder::From("roc_optiq_memory_allocate") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryActivityTrackQuery()
{
    return Builder::Select(rocprofvis_db_sqlite_track_query_format(
        { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
            Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
            Builder::QParam("pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
            Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
            Builder::QParamCategory(kRocProfVisDmPmcTrack),
            Builder::QParamOperation(kRocProfVisDmOperationNoOp),
            Builder::StoreConfigVersion() },
          { Builder::From("roc_optiq_memory_activity", "A"),
            Builder::InnerJoin(GetRocprofMemoryActivitySubQuery(), "S",
                               "A.start == S.value", MultiNode::No) } }));
}

std::string
QueryFactory::GetRocprofSMIPerformanceCountersTrackQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack),
                Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_pmc_event", "PMC_E"),
                Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = S.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("PMC_I.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack),
                Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::StoreConfigVersion() },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
              } }));
    }
}

/*************************************************************************************************
 *                               Track build queries for stream tracks
 **************************************************************************************************/

std::string
QueryFactory::GetRocprofKernelDispatchTrackQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_kernel_dispatch") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocTrackQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("roc_optiq_memory_allocate") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopyTrackQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_track_query_format(
            { { Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack),
                Builder::QParamOperation(kRocProfVisDmMultipleOperations),
                Builder::StoreConfigVersion() },
              { Builder::From("rocpd_memory_copy") } }));
    }
}

/*************************************************************************************************
 *                               Event level calculation queries
 **************************************************************************************************/

std::string
QueryFactory::GetRocprofRegionLevelQuery(bool is_sample_track)
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("R.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              { Builder::From("rocpd_region", "R"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = R.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = R.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = R.end_id"),
                is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                     "SAMPLE.event_id = R.event_id")
                                : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                    "SAMPLE.event_id = R.event_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME),
                Builder::QParam("R.id"),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              { Builder::From("rocpd_region", "R"),
                is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                     "SAMPLE.event_id = R.event_id")
                                : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                    "SAMPLE.event_id = R.event_id") } }));
    }
}

std::string
QueryFactory::GetRocprofKernelDispatchLevelQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("K.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME), Builder::QParam("id"),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("rocpd_kernel_dispatch") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocLevelQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME), Builder::QParam("id"),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("roc_optiq_memory_allocate") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopyLevelQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("start", Builder::START_SERVICE_NAME),
                Builder::QParam("end", Builder::END_SERVICE_NAME), Builder::QParam("id"),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME) },
              { Builder::From("rocpd_memory_copy") } }));
    }
}

std::string
QueryFactory::GetRocprofPerformanceCountersLevelQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                     "K.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id "),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              { Builder::From("rocpd_pmc_event", "PMC_E"),
                Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                   "K.event_id = PMC_E.event_id") } }));
    }
}

std::string
QueryFactory::GetRocprofSMIPerformanceCountersLevelQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TS.value", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = S.timestamp"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = S.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_level_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("S.timestamp", Builder::START_SERVICE_NAME),
                Builder::QParam("S.timestamp", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0),
                Builder::QParam("PMC_I.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryActivityLevelQuery()
{
    return Builder::Select(rocprofvis_db_sqlite_level_query_format(
        { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
            Builder::QParam("start", Builder::START_SERVICE_NAME),
            Builder::QParam("end", Builder::END_SERVICE_NAME), Builder::QParam("id"),
            Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
            Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
            Builder::QParam("pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
            Builder::SpaceSaver(0), Builder::SpaceSaver(0) },
          { Builder::From("roc_optiq_memory_activity", "A"),
            Builder::InnerJoin(GetRocprofMemoryActivitySubQuery(), "S",
                               "A.start == S.value", MultiNode::No) } }));
}

/*************************************************************************************************
 *                               Time slice queries for CPU/GPU tracks
 **************************************************************************************************/
std::string
QueryFactory::GetRocprofRegionSliceQuery(bool is_sample_track)
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("R.name_id"),
                Builder::QParam("R.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(is_sample_track ? kRocProfVisDmRegionSampleTrack
                                                        : kRocProfVisDmRegionMainTrack) },
              { Builder::From("rocpd_region", "R"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = R.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = R.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = R.end_id"),
                Builder::LeftJoin(
                    Builder::LevelTable(is_sample_track ? "launch_sample" : "launch"),
                    "L", "R.id = L.eid"),
                is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                     "SAMPLE.event_id = R.event_id")
                                : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                    "SAMPLE.event_id = R.event_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("R.start", Builder::START_SERVICE_NAME),
                Builder::QParam("R.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("R.name_id"),
                Builder::QParam("R.id"),
                Builder::QParam("R.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(is_sample_track ? kRocProfVisDmRegionSampleTrack
                                                        : kRocProfVisDmRegionMainTrack) },
              { Builder::From("rocpd_region", "R"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id"),
                is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                     "SAMPLE.event_id = R.event_id")
                                : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                    "SAMPLE.event_id = R.event_id"),
                Builder::LeftJoin(
                    Builder::LevelTable(is_sample_track ? "launch_sample" : "launch"),
                    "L", "R.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofKernelDispatchSliceQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("K.kernel_id"),
                Builder::QParam("K.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                Builder::LeftJoin(Builder::LevelTable("dispatch"), "L",
                                  "K.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("K.kernel_id"),
                Builder::QParam("K.id"),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmKernelDispatchTrack) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                Builder::LeftJoin(Builder::LevelTable("dispatch"), "L",
                                  "K.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocSliceQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("E.category_id"),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryAllocationTrack) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L",
                                  "M.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("E.category_id"),
                Builder::QParam("M.id"),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryAllocationTrack) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L",
                                  "M.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopySliceQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("M.name_id"),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryCopyTrack) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L",
                                  "M.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("M.name_id"),
                Builder::QParam("M.id"),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("L.level", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmMemoryCopyTrack) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L",
                                  "M.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofPerformanceCountersSliceQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                     "K.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                     "K.event_id = PMC_E.event_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofSMIPerformanceCountersSliceQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME),
                Builder::QParam("TS.value", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = S.timestamp_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = S.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("S.timestamp", Builder::START_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME),
                Builder::QParam("S.timestamp", Builder::END_SERVICE_NAME),
                Builder::SpaceSaver(0), Builder::SpaceSaver(0),
                Builder::QParam("PMC_I.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmPmcTrack) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryActivitySliceQuery()
{
    return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
        { { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
            Builder::QParam("start", Builder::START_SERVICE_NAME),
            Builder::QParam("size", Builder::COUNTER_VALUE_SERVICE_NAME),
            Builder::QParam("end", Builder::END_SERVICE_NAME), Builder::SpaceSaver(0),
            Builder::SpaceSaver(0), Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
            Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
            Builder::QParam("pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
            Builder::QParam("size", Builder::EVENT_LEVEL_SERVICE_NAME),
            Builder::QParamCategory(kRocProfVisDmPmcTrack) },
          { Builder::From("roc_optiq_memory_activity", "A"),
            Builder::InnerJoin(GetRocprofMemoryActivitySubQuery(), "S",
                               "A.start == S.value", MultiNode::No) } }));
}

/*************************************************************************************************
 *                               Time slice queries for stream tracks
 **************************************************************************************************/

std::string
QueryFactory::GetRocprofKernelDispatchSliceQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("K.kernel_id"),
                Builder::QParam("K.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                Builder::LeftJoin(Builder::LevelTable("dispatch"), "L",
                                  "K.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("K.kernel_id"),
                Builder::QParam("K.id"),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("K.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                Builder::LeftJoin(Builder::LevelTable("dispatch"), "L",
                                  "K.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocSliceQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("E.category_id"),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id "),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L",
                                  "M.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("E.category_id"),
                Builder::QParam("M.id"),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_alloc"), "L",
                                  "M.id = L.eid") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopySliceQueryForStream()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("M.name_id"),
                Builder::QParam("M.id"),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L",
                                  "M.id = L.eid") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_slice_query_format(
            { { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("E.category_id"), Builder::QParam("M.name_id"),
                Builder::QParam("M.id"),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("L.level_for_stream", Builder::EVENT_LEVEL_SERVICE_NAME),
                Builder::QParamCategory(kRocProfVisDmStreamTrack) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
                Builder::LeftJoin(Builder::LevelTable("mem_copy"), "L",
                                  "M.id = L.eid") } }));
    }
}

/*************************************************************************************************
 *                               Table view queries
 **************************************************************************************************/

std::string
QueryFactory::GetRocprofRegionTableQuery(bool is_sample_track)
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_launch_table_query_format(
            { m_db,
              { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("R.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("R.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("R.name_id", Builder::EVENT_NAME_REFERENCE),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("TH.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("(TE.value-TS.value)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::QParam("-1", Builder::STREAM_ID_SERVICE_NAME) },
              {
                  Builder::From("rocpd_region", "R"),
                  Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = R.track_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = R.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = R.end_id"),
                  Builder::InnerJoin("rocpd_info_thread", "TH", "TH.id = T.tid"),
                  is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                       "SAMPLE.event_id = R.event_id")
                                  : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                      "SAMPLE.event_id = R.event_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_launch_table_query_format(
            { m_db,
              { Builder::QParamOperation(is_sample_track
                                             ? kRocProfVisDmOperationLaunchSample
                                             : kRocProfVisDmOperationLaunch),
                Builder::QParam("R.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("R.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("R.name_id", Builder::EVENT_NAME_REFERENCE),
                Builder::QParam("R.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("P.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("R.start", Builder::START_SERVICE_NAME),
                Builder::QParam("R.end", Builder::END_SERVICE_NAME),
                Builder::QParam("(R.end-R.start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("R.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("R.tid", Builder::THREAD_ID_SERVICE_NAME),
                Builder::QParam("-1", Builder::STREAM_ID_SERVICE_NAME) },
              { Builder::From("rocpd_region", "R"),
                is_sample_track ? Builder::InnerJoin("rocpd_sample", "SAMPLE",
                                                     "SAMPLE.event_id = R.event_id")
                                : Builder::LeftJoin("rocpd_sample", "SAMPLE",
                                                    "SAMPLE.event_id = R.event_id"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = R.event_id"),
                Builder::InnerJoin("rocpd_info_process", "P", "P.id = R.pid"),
                Builder::InnerJoin("rocpd_info_thread", "T", "T.id = R.tid") } }));
    }
}

std::string
QueryFactory::GetRocprofKernelDispatchTableQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_dispatch_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("K.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("K.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("K.kernel_id", Builder::SYMBOL_NAME_REFERENCE),
                Builder::QParam("T.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("T.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("(TE.value-TS.value)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("K.grid_size_x", Builder::GRID_SIZEX_PUBLIC_NAME),
                Builder::QParam("K.grid_size_y", Builder::GRID_SIZEY_PUBLIC_NAME),
                Builder::QParam("K.grid_size_z", Builder::GRID_SIZEZ_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_x",
                                Builder::WORKGROUP_SIZEX_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_y",
                                Builder::WORKGROUP_SIZEY_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_z",
                                Builder::WORKGROUP_SIZEZ_PUBLIC_NAME),
                Builder::QParam("K.group_segment_size", Builder::LDS_SIZE_PUBLIC_NAME),
                Builder::QParam("K.private_segment_size",
                                Builder::SCRATCH_SIZE_PUBLIC_NAME),
                Builder::QParam("S.group_segment_size",
                                Builder::STATIC_LDS_SIZE_PUBLIC_NAME),
                Builder::QParam("S.private_segment_size",
                                Builder::STATIC_SCRATCH_SIZE_PUBLIC_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              {
                  Builder::From("rocpd_kernel_dispatch", "K"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                  Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                  Builder::InnerJoin("rocpd_info_kernel_symbol", "S",
                                     "S.id = K.kernel_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_dispatch_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationDispatch),
                Builder::QParam("K.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("K.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("K.kernel_id", Builder::SYMBOL_NAME_REFERENCE),
                Builder::QParam("K.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("K.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("K.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("K.agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("K.agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("K.agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::QParam("(K.end-K.start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("K.grid_size_x", Builder::GRID_SIZEX_PUBLIC_NAME),
                Builder::QParam("K.grid_size_y", Builder::GRID_SIZEY_PUBLIC_NAME),
                Builder::QParam("K.grid_size_z", Builder::GRID_SIZEZ_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_x",
                                Builder::WORKGROUP_SIZEX_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_y",
                                Builder::WORKGROUP_SIZEY_PUBLIC_NAME),
                Builder::QParam("K.workgroup_size_z",
                                Builder::WORKGROUP_SIZEZ_PUBLIC_NAME),
                Builder::QParam("K.group_segment_size", Builder::LDS_SIZE_PUBLIC_NAME),
                Builder::QParam("K.private_segment_size",
                                Builder::SCRATCH_SIZE_PUBLIC_NAME),
                Builder::QParam("S.group_segment_size",
                                Builder::STATIC_LDS_SIZE_PUBLIC_NAME),
                Builder::QParam("S.private_segment_size",
                                Builder::STATIC_SCRATCH_SIZE_PUBLIC_NAME),
                Builder::QParam("K.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("K.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("K.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              {
                  Builder::From("rocpd_kernel_dispatch", "K"),
                  Builder::InnerJoin("rocpd_event", "E", "E.id = K.event_id"),
                  Builder::InnerJoin("rocpd_info_kernel_symbol", "S",
                                     "S.id = K.kernel_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocTableQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_alloc_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("M.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("M.type", Builder::M_TYPE_REFERENCE),
                Builder::QParam("T.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("T.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("T.agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("(TE.value-TS.value)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                Builder::QParam("M.address", Builder::ADDRESS_PUBLIC_NAME),
                Builder::QParam("M.level", Builder::LEVEL_REFERENCE),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              {
                  Builder::From("roc_optiq_memory_allocate", "M"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                  Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_alloc_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate),
                Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("M.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("M.type", Builder::M_TYPE_REFERENCE),
                Builder::QParam("M.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("M.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("M.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("M.agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("M.agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("M.agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("M.agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("(M.end-M.start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                Builder::QParam("M.address", Builder::ADDRESS_PUBLIC_NAME),
                Builder::QParam("M.level", Builder::LEVEL_REFERENCE),
                Builder::QParam("M.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocActivityQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format(
            { {
                  Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                  Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                  Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                  Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                  Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                  Builder::QParam("M.type", Builder::M_TYPE_REFERENCE),
                  Builder::QParam("M.level", Builder::LEVEL_REFERENCE),
                  Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                  Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                  Builder::QParam("M.address", Builder::ADDRESS_PUBLIC_NAME),
                  Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                  Builder::QParam("T.id", "track_id"),
              },
              {
                  Builder::From("rocpd_memory_allocate", "M"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format(
            { {
                  Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                  Builder::QParam("M.pid", Builder::PROCESS_ID_SERVICE_NAME),
                  Builder::QParam("M.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                  Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                  Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                  Builder::QParam("M.type", Builder::M_TYPE_REFERENCE),
                  Builder::QParam("M.level", Builder::LEVEL_REFERENCE),
                  Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                  Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                  Builder::QParam("M.address", Builder::ADDRESS_PUBLIC_NAME),
                  Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                  Builder::SpaceSaver(0),
              },
              { Builder::From("rocpd_memory_allocate", "M") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocActivityLoadQuery()
{
    return Builder::Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format(
        { { Builder::QParam("id", Builder::ID_PUBLIC_NAME),
            Builder::QParam("pid", Builder::PROCESS_ID_SERVICE_NAME),
            Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
            Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
            Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
            Builder::QParam("type", Builder::M_TYPE_REFERENCE),
            Builder::QParam("level", Builder::LEVEL_REFERENCE),
            Builder::QParam("start", Builder::START_SERVICE_NAME),
            Builder::QParam("end", Builder::END_SERVICE_NAME),
            Builder::QParam("address", Builder::ADDRESS_PUBLIC_NAME),
            Builder::QParam("size", Builder::SIZE_PUBLIC_NAME),
            Builder::QParam("track_id") },
          { Builder::From("roc_optiq_memory_activity") } }));
}

std::string
QueryFactory::GetRocprofMemoryCopyTableQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_copy_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("M.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("M.name_id", Builder::EVENT_NAME_REFERENCE),
                Builder::QParam("T.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("T.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("T.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("(TE.value-TS.value)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                Builder::QParam("M.dst_address", Builder::ADDRESS_PUBLIC_NAME),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_ABS_INDEX_REFERENCE),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_TYPE_REFERENCE),
                Builder::QParam("M.src_agent_id",
                                Builder::AGENT_SRC_TYPE_INDEX_REFERENCE),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_NAME_REFERENCE),
                Builder::QParam("M.src_address", Builder::SRC_ADDRESS_PUBLIC_NAME),
                Builder::QParam("T.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              {
                  Builder::From("rocpd_memory_copy", "M"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = M.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = M.end_id"),
                  Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_memory_copy_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy),
                Builder::QParam("M.id", Builder::ID_PUBLIC_NAME),
                Builder::QParam("M.id", Builder::DB_ID_PUBLIC_NAME),
                Builder::QParam("E.category_id", Builder::CATEGORY_REFERENCE),
                Builder::QParam("M.name_id", Builder::EVENT_NAME_REFERENCE),
                Builder::QParam("M.stream_id", Builder::STREAM_NAME_REFERENCE),
                Builder::QParam("M.queue_id", Builder::QUEUE_NAME_REFERENCE),
                Builder::QParam("M.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.pid", Builder::PROCESS_ID_PUBLIC_NAME),
                Builder::QParam("M.tid", Builder::THREAD_ID_PUBLIC_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ABS_INDEX_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_TYPE_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_TYPE_INDEX_REFERENCE),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_NAME_REFERENCE),
                Builder::QParam("M.start", Builder::START_SERVICE_NAME),
                Builder::QParam("M.end", Builder::END_SERVICE_NAME),
                Builder::QParam("(M.end-M.start)", Builder::DURATION_PUBLIC_NAME),
                Builder::QParam("M.size", Builder::SIZE_PUBLIC_NAME),
                Builder::QParam("M.dst_address", Builder::ADDRESS_PUBLIC_NAME),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_ABS_INDEX_REFERENCE),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_TYPE_REFERENCE),
                Builder::QParam("M.src_agent_id",
                                Builder::AGENT_SRC_TYPE_INDEX_REFERENCE),
                Builder::QParam("M.src_agent_id", Builder::AGENT_SRC_NAME_REFERENCE),
                Builder::QParam("M.src_address", Builder::SRC_ADDRESS_PUBLIC_NAME),
                Builder::QParam("M.pid", Builder::PROCESS_ID_SERVICE_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("M.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParam("M.stream_id", Builder::STREAM_ID_SERVICE_NAME) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_event", "E", "E.id = M.event_id") } }));
    }
}

std::string
QueryFactory::GetRocprofPerformanceCountersTableQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TE.value", Builder::END_SERVICE_NAME),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                     "K.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = K.start_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TE", "TE.id = K.end_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("K.start", Builder::START_SERVICE_NAME),
                Builder::QParam("K.end", Builder::END_SERVICE_NAME),
                Builder::QParam("K.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("K.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_kernel_dispatch", "K",
                                     "K.event_id = PMC_E.event_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryActivityTableQuery()
{
    return Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
        { m_db,
          { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
            Builder::QParam("start", Builder::START_SERVICE_NAME),
            Builder::QParam("end", Builder::END_SERVICE_NAME),
            Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
            Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
            Builder::QParam("pmc_id", Builder::COUNTER_ID_SERVICE_NAME),
            Builder::QParam("size", Builder::COUNTER_VALUE_SERVICE_NAME) },
          { Builder::From("roc_optiq_memory_activity", "A"),
            Builder::InnerJoin(GetRocprofMemoryActivitySubQuery(), "S",
                               "A.start == S.value", MultiNode::No) } }));
}

std::string
QueryFactory::GetRocprofSMIPerformanceCountersTableQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("TS.value", Builder::START_SERVICE_NAME),
                Builder::QParam("TS.value", Builder::END_SERVICE_NAME),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
                  Builder::InnerJoin("rocpd_timestamp", "TS", "TS.id = S.timestamp_id"),
                  Builder::InnerJoin("rocpd_track", "T", "T.id = S.track_id"),
              } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_sample_table_query_format(
            { m_db,
              { Builder::QParamOperation(kRocProfVisDmOperationNoOp),
                Builder::QParam("S.timestamp", Builder::START_SERVICE_NAME),
                Builder::QParam("S.timestamp", Builder::END_SERVICE_NAME),
                Builder::QParam("PMC_I.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("PMC_I.id", Builder::COUNTER_ID_SERVICE_NAME),
                Builder::QParam("PMC_E.value", Builder::COUNTER_VALUE_SERVICE_NAME) },
              {
                  Builder::From("rocpd_pmc_event", "PMC_E"),
                  Builder::InnerJoin("rocpd_info_pmc", "PMC_I",
                                     "PMC_I.id = PMC_E.pmc_id"),
                  Builder::InnerJoin("rocpd_sample", "S", "S.event_id = PMC_E.event_id"),
              } }));
    }
}

std::string
QueryFactory::GetRocprofKernelDispatchStreamFlowQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationDispatch) },
              { Builder::From("rocpd_kernel_dispatch", "K"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = K.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationDispatch) },
              { Builder::From("rocpd_kernel_dispatch") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryAllocStreamFlowQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("T.agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate) },
              { Builder::From("roc_optiq_memory_allocate", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryAllocate) },
              { Builder::From("roc_optiq_memory_allocate") } }));
    }
}

std::string
QueryFactory::GetRocprofMemoryCopyStreamFlowQuery()
{
    if(IsVersionGreaterOrEqual("4"))
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("T.stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("T.nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("M.dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("T.queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy) },
              { Builder::From("rocpd_memory_copy", "M"),
                Builder::InnerJoin("rocpd_track", "T", "T.id = M.track_id") } }));
    }
    else
    {
        return Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format(
            { { Builder::QParam("stream_id", Builder::STREAM_ID_SERVICE_NAME),
                Builder::QParam("nid", Builder::NODE_ID_SERVICE_NAME),
                Builder::QParam("dst_agent_id", Builder::AGENT_ID_SERVICE_NAME),
                Builder::QParam("queue_id", Builder::QUEUE_ID_SERVICE_NAME),
                Builder::QParamOperation(kRocProfVisDmOperationMemoryCopy) },
              { Builder::From("rocpd_memory_copy") } }));
    }
}

}  // namespace DataModel
}  // namespace RocProfVis
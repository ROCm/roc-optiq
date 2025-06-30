// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <queue>

namespace RocProfVis
{
    namespace Controller
    {


        class Event;
        class Sample;
        class SampleLOD;
        class Segment;


        class ComputeTrace;

#define MEM_MGMT_RELEASE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN        1000000
#define MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN          1000

        struct LRUMember
        {
            uint64_t    m_timestamp;
            Handle* m_owner;
            Segment* m_reference;
            void* array_ptr;
            uint64_t    m_memory_usage;
            uint32_t    m_lod;

            bool operator<(const LRUMember& other) const { return m_timestamp > other.m_timestamp; }
        };


        class MemoryManager
        {
        public:
            MemoryManager();
            void Init(size_t num_objects);

            virtual ~MemoryManager();

            std::shared_mutex&  GetMemoryManagerMutex();
            rocprofvis_result_t AddLRUReference(Handle* owner, Segment* reference, uint32_t lod, void* array_ptr);
            rocprofvis_result_t CancelArrayOwnersip(void* array_ptr);
            void                ManageLRU();
            void                ManageEventObjectsPool();
            void                ManageSampleObjectsPool();
            void                ManageSampleLODObjectsPool();
            void                RetainObject(Handle*);
            Event*              NewEvent(uint64_t id, double start_ts, double end_ts);
            Event*              NewEvent(Event* event);
            Sample*             NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp);
            SampleLOD*          NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, std::vector<Sample*>& children);

        private:

            std::set<LRUMember>                                         m_lru_array;
            std::map<void*, std::set<LRUMember>::iterator>              m_lru_inuse_lookup;
            std::unordered_map<Segment*, std::set<LRUMember>::iterator> m_lru_lookup;
            std::thread                                                 m_lru_thread;
            std::thread                                                 m_mem_event_pool_thread;
            std::thread                                                 m_mem_sample_pool_thread;
            std::thread                                                 m_mem_sample_lod_pool_thread;
            std::queue<Event*>                                          m_reusable_event_objects_pool;
            std::queue<Sample*>                                         m_reusable_sample_objects_pool;
            std::queue<SampleLOD*>                                      m_reusable_sample_lod_objects_pool;
            std::mutex                                                  m_reusable_event_objects_mutex;
            std::mutex                                                  m_reusable_sample_objects_mutex;
            std::mutex                                                  m_reusable_sample_lod_objects_mutex;
            std::shared_mutex                                           m_mem_mgmt_mutex;
            std::shared_mutex                                           m_lru_mutex;
            std::shared_mutex                                           m_in_use_mutex;
            std::atomic<bool>                                           m_lru_mgmt_shutdown;
            std::atomic<bool>                                           m_controller_active;
            bool                                                        m_mem_mgmt_initialized;
            size_t                                                      m_lru_storage_limit;
            int64_t                                                     m_pool_storage_limit;
            size_t                                                      m_lru_storage_memory_used;
        };

    }
}

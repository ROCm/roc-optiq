// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <queue>
#include <bitset>

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

        constexpr uint32_t kMemPoolBitSetSize = 4096;


        struct MemoryPool
        {
            void*                           m_base;
            size_t                          m_size;
            std::bitset<kMemPoolBitSetSize> m_bitmask;
            uint32_t                        m_pos;

            MemoryPool(size_t size)
            : m_size(size)
            , m_pos(0)
            {
                m_base = ::operator new(size * kMemPoolBitSetSize);
            }

            ~MemoryPool() {
                ::operator delete(m_base); 
            }
        };

        struct LRUMember
        {
            uint64_t    m_timestamp;
            Handle*     m_owner;
            Segment*    m_reference;
            void*       m_array_ptr;
            uint32_t    m_lod;
        };



        class MemoryManager
        {
        public:
            MemoryManager();
            void Init(size_t num_objects);

            virtual ~MemoryManager();

            std::mutex&         GetMemoryManagerMutex();
            rocprofvis_result_t AddLRUReference(Handle* owner, Segment* reference, uint32_t lod, void* array_ptr);
            rocprofvis_result_t CancelArrayOwnersip(void* array_ptr);
            void                ManageLRU();

            void*               Allocate(size_t size, Handle* owner);
            void                Delete(Handle* handle, Handle* owner);
            void                CleanUp();
            Event*              NewEvent(uint64_t id, double start_ts, double end_ts, Handle* owner);
            Event*              NewEvent(Event* event, Handle* owner);
            Sample*             NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, Handle* owner);
            SampleLOD*          NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, std::vector<Sample*>& children, Handle* owner);

            

        private:

            std::unordered_set<void*>                                   m_lru_inuse_lookup;
            std::unordered_map < Segment*, std::unique_ptr<LRUMember>>  m_lru_array;
            std::condition_variable                                     m_lru_cv;
            std::thread                                                 m_lru_thread;
            std::mutex                                                  m_lru_mutex;
            bool                                                        m_lru_mgmt_shutdown;
            bool                                                        m_array_ownership_changed;
            bool                                                        m_mem_mgmt_initialized;
            size_t                                                      m_lru_storage_limit;
            std::atomic<size_t>                                         m_lru_storage_memory_used;

            std::map < Handle*, std::map<void*, MemoryPool*>>           m_object_pools;
            std::map<Handle*, std::map<void*, MemoryPool*>::iterator>   m_current_pool;
            std::mutex                                                  m_pool_mutex;

        };

    }
}

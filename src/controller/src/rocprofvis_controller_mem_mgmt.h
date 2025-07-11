// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RocProfVis
{
    namespace Controller
    {


        class Event;
        class Sample;
        class SampleLOD;
        class Segment;
        class SegmentTimeline;


        class ComputeTrace;

        constexpr uint32_t kUseVailMemoryPercent = 50;

        typedef enum rocprofvis_object_type_t
        {
            kRocProfVisObjectTypeEvent,
            kRocProfVisObjectTypeSample,
            kRocProfVisObjectTypeSampleLOD,
            kRocProfVisNumberOfObjectTypes
        } rocprofvis_object_type_t;

        class BitSet
        {
        private:
            std::vector<uint64_t>   m_bits;
            static constexpr size_t WORD_SIZE = 64;

        public:

            BitSet(size_t total_bits)
            : m_bits((total_bits + WORD_SIZE - 1) / WORD_SIZE, 0) {}

            size_t Size() const { return m_bits.size() * WORD_SIZE; }
            void Set(size_t pos);
            void Clear(size_t pos);
            bool Test(size_t pos) const;
            uint32_t FindFirstZero() const;
            bool   None() const;
            uint32_t Count() const;
            
        };

        struct MemoryPool
        {
            void*                           m_base;
            size_t                          m_size;
            BitSet                          m_bitmask;
            uint32_t                        m_pos;

            MemoryPool(uint32_t object_size, uint32_t num_objects)
            : m_size(object_size)
            , m_pos(0)
            , m_bitmask(num_objects)
            {
                m_base = ::operator new(object_size * m_bitmask.Size());
            }

            ~MemoryPool() {
                ::operator delete(m_base); 
            }
        };

        struct LRUMember
        {
            uint64_t            m_timestamp;
            SegmentTimeline*    m_owner;
            Segment*            m_reference;
            void*               m_array_ptr;
            uint32_t            m_lod;
        };



        class MemoryManager
        {
        public:
            MemoryManager();
            void Init(size_t num_objects);

            virtual ~MemoryManager();

            std::mutex&         GetMemoryManagerMutex();
            std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
                                GetDefaultLRUIterator();
            std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
                                AddLRUReference(SegmentTimeline* owner,
                                                Segment* reference, uint32_t lod,
                                                void* array_ptr);
            rocprofvis_result_t EnterArrayOwnersip(void* array_ptr);
            rocprofvis_result_t CancelArrayOwnersip(void* array_ptr);
            void                ManageLRU();

            void                Delete(Handle* handle);
            Event*              NewEvent(uint64_t id, double start_ts, double end_ts);
            Event*              NewEvent(Event* event);
            Sample*             NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp);
            SampleLOD*          NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, std::vector<Sample*>& children);
            
            

        private:

            std::unordered_set<void*>                                   m_lru_inuse_lookup;
            std::unordered_map < Segment*, std::unique_ptr<LRUMember>>  m_lru_array;
            std::condition_variable                                     m_lru_cv;
            std::thread                                                 m_lru_thread;
            std::mutex                                                  m_lru_mutex;
            std::mutex                                                  m_lru_inuse_mutex;
            bool                                                        m_lru_mgmt_shutdown;
            bool                                                        m_array_ownership_changed;
            bool                                                        m_mem_mgmt_initialized;
            std::atomic<size_t>                                         m_lru_storage_memory_used;
            size_t                                                      m_trace_size;
            uint32_t                                                    m_mem_block_size;

            std::map<void*, MemoryPool*>                                m_object_pools;
            std::map<void*, MemoryPool*>::iterator                      m_current_pool[kRocProfVisNumberOfObjectTypes];
            std::mutex                                                  m_pool_mutex;

            void*  Allocate(size_t size, rocprofvis_object_type_t type);
            void   CleanUp();
            size_t GetMemoryManagerSizeLimit();

            inline static size_t                                               s_physical_memory_avail=0;
            inline static size_t                                               s_total_loaded_size=0;
            inline static uint32_t                                             s_num_traces=0;

        };

    }
}

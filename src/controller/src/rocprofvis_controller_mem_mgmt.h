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

        constexpr uint32_t kUseVailMemoryPercent = 30;
        constexpr uint64_t kShortTracksMemoryPoolIdentifier = 1;

        typedef enum rocprofvis_object_type_t
        {
            kRocProfVisObjectTypeEvent,
            kRocProfVisObjectTypeSample,
            kRocProfVisObjectTypeSampleLOD,
            kRocProfVisNumberOfObjectTypes
        } rocprofvis_object_type_t;

        typedef enum rocprofvis_owner_type_t
        {
            kRocProfVisOwnerTypeTrack,
            kRocProfVisOwnerTypeGraph,
            kRocProfVisNumberOfOwnerTypes
        } rocprofvis_owner_type_t;

        class BitSet
        {
        private:
            std::vector<uint64_t>   m_bits;
            static constexpr size_t WORD_SIZE = 64;

        public:
            BitSet() = default;
            BitSet(size_t total_bits)
            : m_bits((total_bits + WORD_SIZE - 1) / WORD_SIZE, 0) {}

            void Init(size_t total_bits)
            {
                m_bits.resize((total_bits + WORD_SIZE - 1) / WORD_SIZE);
                std::fill(m_bits.begin(), m_bits.end(), 0);
            }
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
            uint32_t                        m_type;

            MemoryPool(uint32_t object_size, uint32_t object_type, uint32_t num_objects)
            : m_size(object_size)
            , m_type(object_type)
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
            std::set<void*>  m_array_ptr;
        };


        struct LRUOwnerMember
        {
            uint64_t         m_timestamp;
            std::unordered_map<Segment*, std::unique_ptr<LRUMember>> m_lru_segment_array;
        };


        class MemoryManager
        {
            using MemoryPoolMap= std::map<void*, MemoryPool*>;
            using MemPoolsMap  = std::map<uint64_t, MemoryPoolMap>;

        public:
            MemoryManager(uint64_t id);
            void Init(size_t num_objects);
            bool IsShuttingDown();

            virtual ~MemoryManager();

            void AddLRUReference(SegmentTimeline* owner,
                                                    Segment* reference, uint32_t lod,
                                                    void* array_ptr);
            rocprofvis_result_t EnterArrayOwnership(void* array_ptr,
                                                   rocprofvis_owner_type_t type);
            rocprofvis_result_t CancelArrayOwnership(void* array_ptr,
                                                    rocprofvis_owner_type_t type);
            void                Configure(double weight);

            void                Delete(Handle* handle, SegmentTimeline* owner);
            Event*              NewEvent(uint64_t id, double start_ts, double end_ts, SegmentTimeline* owner);
            Sample*             NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, SegmentTimeline* owner);
            SampleLOD*          NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, std::vector<Sample*>& children, SegmentTimeline* owner);
            
            

        private:

            uint64_t                                                    m_id;
            std::unordered_set<void*>                                   m_lru_inuse_lookup[kRocProfVisNumberOfOwnerTypes];
            std::map <SegmentTimeline*, std::unique_ptr<LRUOwnerMember>>  m_lru_array;
            std::condition_variable                                     m_lru_cv;
            std::thread                                                 m_lru_thread;
            std::mutex                                                  m_lru_mutex;
            std::mutex                                                  m_lru_cond_mutex;
            std::mutex                                                  m_lru_inuse_mutex[kRocProfVisNumberOfOwnerTypes];
            std::atomic<bool>                                           m_lru_mgmt_shutdown;
            bool                                                        m_lru_configured;
            bool                                                        m_mem_mgmt_initialized;
            std::atomic<size_t>                                         m_lru_storage_memory_used;
            uint32_t                                                    m_mem_block_size;
            size_t                                                      m_lru_size_limit;
            size_t                                                      m_trace_size;
            double                                                      m_trace_weight;

            MemPoolsMap                                                 m_object_pools;
            std::map<uint64_t, MemoryPool*>                             m_current_pool[kRocProfVisNumberOfObjectTypes];
            std::set<SegmentTimeline*>                                  m_short_tracks;
            std::mutex                                                  m_pool_mutex;

            void            ManageLRU();
            void*           Allocate(size_t size, rocprofvis_object_type_t type, SegmentTimeline *owner);
            void            CleanUp();
            static void     UpdateSizeLimit();
            void            LockTimelines(std::vector<SegmentTimeline*>& locked, std::vector<SegmentTimeline*>& failed_to_lock);
            void            UnlockTimelines(std::vector<SegmentTimeline*>& locked);
            bool            CheckInUse(LRUMember* lru);

            inline static size_t                                        s_physical_memory_avail=0;
            inline static std::vector<MemoryManager*>                   s_memory_manager_instances;
            inline static std::mutex                                    s_lru_config_mutex;

        };

    }
}

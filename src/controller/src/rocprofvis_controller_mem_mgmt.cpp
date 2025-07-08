#include "rocprofvis_controller_mem_mgmt.h"
#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include <chrono>
#include <execution>
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace Controller
{

MemoryManager::MemoryManager()
: m_lru_storage_limit(1000000000)
, m_lru_storage_memory_used(0)
, m_lru_mgmt_shutdown(false)
, m_mem_mgmt_initialized(false)
{ 
}

MemoryManager::~MemoryManager()
{
    if(m_mem_mgmt_initialized)
    {
        {
            std::unique_lock<std::mutex> lock(m_lru_mutex);
            m_lru_mgmt_shutdown = true;
            m_lru_cv.notify_one();
        }
        if(m_lru_thread.joinable())
        {
            m_lru_thread.join();
        }

    }
}

// start memory manager LRU processing when trace size is known
void MemoryManager::Init(size_t trace_size)
{
    m_lru_storage_limit  = trace_size / 5;      // memory limit for data stored in segments   
    m_lru_thread         = std::thread(&MemoryManager::ManageLRU, this);
    m_mem_mgmt_initialized = true;
}


std::mutex& MemoryManager::GetMemoryManagerMutex()
{
    return m_lru_mutex;
}

rocprofvis_result_t
MemoryManager::CancelArrayOwnersip(void* array_ptr)
{

    std::unique_lock lock(m_lru_mutex);
    auto             it = m_lru_inuse_lookup.find(array_ptr);
    if(it != m_lru_inuse_lookup.end())
    {
        m_lru_inuse_lookup.erase(it);
        m_array_ownership_changed = true;
        m_lru_cv.notify_one();
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultNotLoaded;
}

rocprofvis_result_t
MemoryManager::AddLRUReference(Handle* owner, Segment* reference, uint32_t lod, void* array_ptr)
{
    uint64_t         ts  = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now())
                      .time_since_epoch()
                      .count();

    auto     it                   = m_lru_array.find(reference);
    rocprofvis_result_t result               = kRocProfVisResultMemoryAllocError;
    if(it != m_lru_array.end())
    {
        it->second->m_timestamp = ts;
        it->second->m_array_ptr   = array_ptr;
        result                 = kRocProfVisResultSuccess;
    } else
    {
        auto pair = m_lru_array.insert(std::pair<Segment*, std::unique_ptr<LRUMember>>(
            reference,
            std::make_unique<LRUMember>(LRUMember{ ts, owner, reference, array_ptr, lod })));
        if(pair.second)
        {
            result = kRocProfVisResultSuccess;
        }
    }

    if(result == kRocProfVisResultSuccess)
    {
        auto it  = m_lru_inuse_lookup.find(array_ptr);
        if(it == m_lru_inuse_lookup.end())
        {
            m_lru_inuse_lookup.insert(array_ptr);
        }
    }

    return result;
}

void
MemoryManager::ManageLRU()
{
    bool thread_running = true;
    while(thread_running)
    {
        std::unique_lock lock(m_lru_mutex);
        m_lru_cv.wait(lock, [&] {
            return (m_array_ownership_changed && m_lru_storage_memory_used >
                       m_lru_storage_limit) || m_lru_mgmt_shutdown;
        });
        if (m_lru_mgmt_shutdown)
        {
            CleanUp();
            thread_running = false;
        }
        else
        {
            m_array_ownership_changed = false;
            std::vector<LRUMember*> sorted_lru_array;
            sorted_lru_array.reserve(m_lru_array.size());
            for(auto& [segment_ptr, lru_member] : m_lru_array)
            {
                sorted_lru_array.push_back(lru_member.get());
            }

            std::sort(sorted_lru_array.begin(), sorted_lru_array.end(),
                [](const LRUMember* a, const LRUMember* b) {
                    if((a->m_lod == 0) != (b->m_lod == 0))
                        return a->m_lod == 0;  

                    return a->m_timestamp < b->m_timestamp;
                });

            int counter = 0;
            for(auto* lru : sorted_lru_array)
            {
                auto inuse = m_lru_inuse_lookup.find(lru->m_array_ptr);
                if(inuse != m_lru_inuse_lookup.end())
                {
                    continue;
                }

                Segment* reference    = lru->m_reference;
                uint32_t lod          = lru->m_lod;
                SegmentTimeline* owner        = lru->m_owner;

                m_lru_array.erase(reference);

                owner->Remove(reference);

                
                if (m_lru_storage_memory_used <= m_lru_storage_limit)
                {
                    break;
                }
                //if((++counter % 10000) == 0)
                //{
                //    lock.unlock();
                //    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                //    lock.lock();
                //}
            }
        }    
    }
}


void*
MemoryManager::Allocate(size_t size, Handle* owner)
{
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    MemoryPool* current_pool = nullptr;
    auto owner_it = m_current_pool.find(owner);
    if (owner_it != m_current_pool.end())
    {
        auto it = owner_it->second;
        current_pool = it->second;
    }

    if(current_pool == nullptr || current_pool->m_pos == kMemPoolBitSetSize)
    {
        current_pool = new MemoryPool(size);
        auto pair = m_object_pools[owner].insert(std::pair<void*, MemoryPool*>(current_pool->m_base, current_pool));
        if(pair.second)
        {
            m_current_pool[owner] = pair.first;
            m_lru_storage_memory_used += size * kMemPoolBitSetSize;
        }
    }
    char* ptr = (char*)current_pool->m_base + (current_pool->m_pos * size);
    current_pool->m_bitmask.set(current_pool->m_pos++);
    return ptr;
}

void MemoryManager::CleanUp() {

    for(auto pool = m_object_pools.begin(); pool != m_object_pools.end(); ++pool)
    {
        for(auto it = pool->second.begin(); it != pool->second.end(); ++it)
        {
            delete it->second;
        }
    }
}

Event*
MemoryManager::NewEvent(uint64_t id, double start_ts, double end_ts, Handle* owner)
{
    Event* event = nullptr;
    void* ptr = Allocate(sizeof(Event), owner);
    if(ptr)
    {
        event = new(ptr) Event(id, start_ts, end_ts);
    }
    return event;
}

Event*
MemoryManager::NewEvent(Event* other, Handle* owner)
{
    Event* event = nullptr;
    void*  ptr   = Allocate(sizeof(Event), owner);
    if(ptr)
    {
        event = new(ptr) Event(other);
    }
    return event;
}

Sample*
MemoryManager::NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id,
                         double timestamp, Handle* owner)
{
    Sample* sample = nullptr;
    void*  ptr   = Allocate(sizeof(Sample), owner);
    if(ptr)
    {
        sample = new(ptr) Sample(type, id, timestamp);
    }
    return sample;
}

SampleLOD*
MemoryManager::NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id,
                            double timestamp, std::vector<Sample*>& children,
                            Handle* owner)
{
    SampleLOD* sample = nullptr;
    void*   ptr    = Allocate(sizeof(SampleLOD), owner);
    if(ptr)
    {
        sample = new(ptr) SampleLOD(type, id, timestamp, children);
    }
    return sample;

}

void
MemoryManager::Delete(Handle* handle, Handle* owner)
{
    void* ptr = handle;
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    auto  it = m_object_pools[owner].upper_bound(ptr);
    --it;
    MemoryPool* pool = it->second;
    char*       base      = (char*)pool->m_base;
    size_t      size      = pool->m_size * kMemPoolBitSetSize;
    char*       end       = base + size;
    if(ptr >= base && ptr < end) 
    {
        handle->~Handle();
        uint64_t index = ((char*)ptr - base) / pool->m_size;
        pool->m_bitmask.reset(index);
        
        if(pool->m_bitmask.none() && pool->m_pos == kMemPoolBitSetSize)
        {
            m_lru_storage_memory_used -= size;
            m_object_pools[owner].erase(it);
            delete pool;
        }
    } else
    {
        spdlog::debug("Memory manager error : memory address not found!");
    }
    
}



}  // namespace Controller
}  // namespace RocProfVis
#include "rocprofvis_controller_mem_mgmt.h"
#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include <chrono>
#include <execution>

namespace RocProfVis
{
namespace Controller
{

MemoryManager::MemoryManager()
: m_lru_storage_limit(1000000000)
, m_pool_storage_limit(10000000)
, m_lru_storage_memory_used(0)
, m_lru_mgmt_shutdown(false)
, m_mem_mgmt_initialized(false)
{}

MemoryManager::~MemoryManager()
{
    if(m_mem_mgmt_initialized)
    {
        {
            std::unique_lock<std::shared_mutex> lock(m_mem_mgmt_mutex);
            m_lru_mgmt_shutdown = true;
        }
        if(m_lru_thread.joinable())
        {
            m_lru_thread.join();
        }
        {
            std::unique_lock<std::shared_mutex> lock(m_mem_mgmt_mutex);
            m_pool_storage_limit = 0;
        }
        if(m_mem_event_pool_thread.joinable())
        {
            m_mem_event_pool_thread.join();
        }
        if(m_mem_sample_pool_thread.joinable())
        {
            m_mem_sample_pool_thread.join();
        }
        if(m_mem_sample_lod_pool_thread.joinable())
        {
            m_mem_sample_lod_pool_thread.join();
        }
    }
}

// start memory manager LRU processing when trace size is known
void MemoryManager::Init(size_t trace_size)
{
    m_lru_storage_limit  = trace_size / 80;      // memory limit for data stored in segments   
    m_pool_storage_limit   = trace_size / 800;   // memory limit for reused objects
    m_lru_thread         = std::thread(&MemoryManager::ManageLRU, this);
    // processing reused objects by one thread per object type
    m_mem_event_pool_thread    = std::thread(&MemoryManager::ManageEventObjectsPool, this);
    m_mem_sample_pool_thread = std::thread(&MemoryManager::ManageSampleObjectsPool, this);
    m_mem_sample_lod_pool_thread = std::thread(&MemoryManager::ManageSampleLODObjectsPool, this);
    m_mem_mgmt_initialized = true;
    m_controller_active    = true;
}


std::shared_mutex& MemoryManager::GetMemoryManagerMutex()
{
    m_controller_active = true;
    return m_mem_mgmt_mutex;
}

rocprofvis_result_t
MemoryManager::CancelArrayOwnersip(void* array_ptr)
{

    std::unique_lock lock(m_in_use_mutex);
    auto             it = m_lru_inuse_lookup.find(array_ptr);
    if(it != m_lru_inuse_lookup.end())
    {
        m_lru_inuse_lookup.erase(it);
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultNotLoaded;
}

rocprofvis_result_t
MemoryManager::AddLRUReference(Handle* owner, Segment* reference, uint32_t lod, void* array_ptr)
{
    m_controller_active = true;
    std::unique_lock lock(m_lru_mutex);
    uint64_t         ts  = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now())
                      .time_since_epoch()
                      .count();
    uint64_t segment_memory_usage = 0;
    auto     it                   = m_lru_lookup.find(reference);
    if(it != m_lru_lookup.end())
    {
        owner                = it->second->m_owner;
        lod                  = it->second->m_lod;
        array_ptr            = it->second->array_ptr;
        segment_memory_usage = it->second->m_memory_usage;
        m_lru_array.erase(it->second);
    }
    else
    {
        reference->GetMemoryUsage(&segment_memory_usage,
                                  kRPVControllerCommonMemoryUsageInclusive);
        m_lru_storage_memory_used += segment_memory_usage;
    }

    auto pair = m_lru_array.insert(
        { ts, owner, reference, array_ptr, segment_memory_usage, lod });
    if(pair.second)
    {
        m_lru_lookup[reference]       = pair.first;
        {
            std::unique_lock<std::shared_mutex> internal_lock(m_in_use_mutex);
            m_lru_inuse_lookup[array_ptr] = pair.first;
        }
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultMemoryAllocError;
}

void
MemoryManager::ManageLRU()
{
    bool thread_running = true;
    while(thread_running)
    {

        if(m_mem_mgmt_mutex.try_lock())
        {     
            if(m_lru_storage_memory_used > m_lru_storage_limit || m_lru_mgmt_shutdown)
            {
                {
                    std::unique_lock<std::shared_mutex> internal_lock(
                        m_lru_mutex);
                    size_t   num_references = m_lru_array.size();
                    uint64_t memory_used    = m_lru_storage_memory_used;

                    auto it = m_lru_array.rbegin();
                    while((m_lru_storage_memory_used > m_lru_storage_limit ||
                           m_lru_mgmt_shutdown) &&
                          it != m_lru_array.rend())
                    {
                        bool not_inuse = false;
                        {
                            std::unique_lock<std::shared_mutex> internal_lock(
                                m_in_use_mutex);
                            auto it_inuse = m_lru_inuse_lookup.find(it->array_ptr);
                            not_inuse     = it_inuse == m_lru_inuse_lookup.end();
                        }
                        if(not_inuse || m_lru_mgmt_shutdown)
                        {
                            Segment* reference    = it->m_reference;
                            uint32_t lod          = it->m_lod;
                            Handle*  owner        = it->m_owner;
                            uint64_t memory_usage = it->m_memory_usage;

                            it = std::set<LRUMember>::reverse_iterator(
                                m_lru_array.erase(std::next(it).base()));
                            m_lru_lookup.erase(reference);
                            m_lru_storage_memory_used -= memory_usage;
                            owner->DeleteSegment(reference, lod);
                        }
                         else
                        {
                            ++it;
                        }
                    }
                    if(m_lru_mgmt_shutdown)
                    {
                        thread_running = false;
                    }
                }
            }
            m_controller_active = false;
            m_mem_mgmt_mutex.unlock();
        }
        
    }
}


void
MemoryManager::ManageEventObjectsPool()
{
    while(true)
    {
        // if not shutdown and controller is active
        // make it low priority thread
        if((m_pool_storage_limit > 0 && m_controller_active) || m_reusable_event_objects_pool.size() < m_pool_storage_limit)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        // if LRU manager is not holding lock
        if(m_mem_mgmt_mutex.try_lock_shared())
        {
            std::vector<Event*> v;
            {
                std::unique_lock<std::mutex> lock(m_reusable_event_objects_mutex);
                int target = m_reusable_event_objects_pool.size() - m_pool_storage_limit;
                if(target > 0)
                {
                    if(target > MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN &&
                       m_pool_storage_limit > 0)
                    {
                        target = MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN;
                    }                    
                    
                    v.reserve(target);
                    for(int i = 0; i < target; i++)
                    {
                        v.push_back(m_reusable_event_objects_pool.front());
                        m_reusable_event_objects_pool.pop();
                    }
                }
            }
            m_mem_mgmt_mutex.unlock_shared();
            std::for_each(std::execution::par, v.begin(), v.end(), [](Event* obj) { delete obj; });
            if(m_pool_storage_limit == 0 && m_reusable_event_objects_pool.size() == 0)
            {
                break;
            }
        }
    }
}

void
MemoryManager::ManageSampleObjectsPool()
{
    while(true)
    {
        if(m_pool_storage_limit > 0 && m_controller_active)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if(m_mem_mgmt_mutex.try_lock_shared())
        {
            std::vector<Sample*> v;
            {
                std::unique_lock<std::mutex> lock(m_reusable_sample_objects_mutex);
                int target = m_reusable_sample_objects_pool.size() - m_pool_storage_limit;
                if(target > 0)
                {
                    if(target > MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN &&
                       m_pool_storage_limit > 0)
                    {
                        target = MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN;
                    }
                    
                    v.reserve(target);
                    for(int i = 0; i < target; i++)
                    {
                        v.push_back(m_reusable_sample_objects_pool.front());
                        m_reusable_sample_objects_pool.pop();
                    }
                }
            }
            m_mem_mgmt_mutex.unlock_shared();
            std::for_each(std::execution::par, v.begin(), v.end(),
                          [](Sample* obj) { delete obj; });
            if(m_pool_storage_limit == 0 && m_reusable_event_objects_pool.size() == 0)
            {
                break;
            }
        }

    }
}

void
MemoryManager::ManageSampleLODObjectsPool()
{
    while(true)
    {
        if(m_pool_storage_limit > 0 && m_controller_active)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if(m_mem_mgmt_mutex.try_lock_shared())
        {
            std::vector<SampleLOD*> v;
            {
                std::unique_lock<std::mutex> lock(m_reusable_sample_lod_objects_mutex);
                int                          target =
                    m_reusable_sample_lod_objects_pool.size() - m_pool_storage_limit;
                if(target > 0)
                {
                    if(target > MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN &&
                       m_pool_storage_limit > 0)
                    {
                        target = MEM_MGMT_DELETE_NUMBER_OF_OBJECTS_IN_SINGLE_RUN;
                    }
                    v.reserve(target);
                    for(int i = 0; i < target; i++)
                    {
                        v.push_back(m_reusable_sample_lod_objects_pool.front());
                        m_reusable_sample_lod_objects_pool.pop();
                    }
                }
            }
            m_mem_mgmt_mutex.unlock_shared();
            std::for_each(std::execution::par, v.begin(), v.end(),
                            [](SampleLOD* obj) { delete obj; });

            if(m_pool_storage_limit == 0 && m_reusable_event_objects_pool.size() == 0)
            {
                break;
            }
        }
       
    }
}

void
MemoryManager::RetainObject(Handle* ptr)
{
    if(Event* event = dynamic_cast<Event*>(ptr))
    {
            std::lock_guard<std::mutex> lock(m_reusable_event_objects_mutex);
            m_reusable_event_objects_pool.push(event);
    }
    else if(Sample* sample = dynamic_cast<Sample*>(ptr))
    {
            std::lock_guard<std::mutex> lock(m_reusable_sample_objects_mutex);
            m_reusable_sample_objects_pool.push(sample);
    }
    else if(SampleLOD* sample_lod = dynamic_cast<SampleLOD*>(ptr))
    {
            std::lock_guard<std::mutex> lock(m_reusable_sample_lod_objects_mutex);
            m_reusable_sample_lod_objects_pool.push(sample_lod);
    }
}

Event*
MemoryManager::NewEvent(Event* other)
{
    Event* event = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_reusable_event_objects_mutex);
        if(!m_reusable_event_objects_pool.empty())
        {
            event  = m_reusable_event_objects_pool.front();
            *event = *other;
            m_reusable_event_objects_pool.pop();
        }
    }
    if(event == nullptr)
    {
        event = new Event(other);
    }
    return event;
}

Event*
MemoryManager::NewEvent(uint64_t id, double start_ts, double end_ts)
{
    Event* event = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_reusable_event_objects_mutex);
        if(!m_reusable_event_objects_pool.empty())
        {
            event  = m_reusable_event_objects_pool.front();
            *event = Event(id, start_ts, end_ts);
            m_reusable_event_objects_pool.pop();
        }
    }
    if(event == nullptr)
    {
        event = new Event(id, start_ts, end_ts);
    }
    return event;
}

Sample*
MemoryManager::NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id,
                 double timestamp)
{
    Sample* sample = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_reusable_sample_objects_mutex);
        if(!m_reusable_sample_objects_pool.empty())
        {
            sample  = m_reusable_sample_objects_pool.front();
            *sample = Sample(type, id, timestamp);
            m_reusable_event_objects_pool.pop();
        }
    }
    if(sample == nullptr)
    {
        sample = new Sample(type, id, timestamp);
    }
    return sample;
}

SampleLOD*
MemoryManager::NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id,
                    double timestamp, std::vector<Sample*>& children)
{
    SampleLOD* sample = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_reusable_sample_lod_objects_mutex);
        if(!m_reusable_sample_lod_objects_pool.empty())
        {
            sample  = m_reusable_sample_lod_objects_pool.front();
            *sample = SampleLOD(type, id, timestamp, children);
            m_reusable_event_objects_pool.pop();
        }
    }
    if(sample == nullptr)
    {
        sample = new SampleLOD(type, id, timestamp, children);
    }
    return sample;
}



}  // namespace Controller
}  // namespace RocProfVis
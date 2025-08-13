#include "rocprofvis_controller_mem_mgmt.h"
#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include <chrono>
#include <execution>
#include <cmath>
#include "spdlog/spdlog.h"
#if defined(_MSC_VER)
#include <windows.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <sys/sysinfo.h>
#endif
#undef min
#undef max
#include <algorithm>


namespace RocProfVis
{
namespace Controller
{

MemoryManager::MemoryManager(uint64_t id)
: m_lru_storage_memory_used(0)
, m_lru_mgmt_shutdown(false)
, m_mem_mgmt_initialized(false)
, m_mem_block_size(1024)
, m_id(id)
, m_trace_weight(1.0)
{ 
    //for(int type = 0; type < kRocProfVisNumberOfObjectTypes; type++)
    //{
    //    m_current_pool[type] = m_object_pools.end();
    //}
}

MemoryManager::~MemoryManager()
{
    if(m_mem_mgmt_initialized)
    {
        {
            std::unique_lock<std::mutex> lock(m_lru_cond_mutex);
            m_lru_mgmt_shutdown = true;
            m_lru_cv.notify_one();
        }
        if(m_lru_thread.joinable())
        {
            m_lru_thread.join();
        }

    }
    for (auto it = s_memory_manager_instances.begin();
        it != s_memory_manager_instances.end(); ++it)
    {
        if (*it == this)
        {
            std::unique_lock lock(s_lru_config_mutex);
            s_memory_manager_instances.erase(it);
            UpdateSizeLimit();
            break;
        }
    }
}

// start memory manager LRU processing when trace size is known
void MemoryManager::Init(size_t trace_size)
{
    if(s_physical_memory_avail == 0)
    {
#if defined(_MSC_VER)
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&mem_info);

        s_physical_memory_avail = (mem_info.ullAvailPhys / 100) * kUseVailMemoryPercent;

#elif defined(__GNUC__) || defined(__clang__)
        struct sysinfo mem_info;
        sysinfo(&mem_info);

        uint64_t avail_phys_mem = mem_info.freeram;
        avail_phys_mem *= mem_info.mem_unit;

        s_physical_memory_avail = (avail_phys_mem / 100) * kUseVailMemoryPercent;
#endif
    }

    m_trace_size =  trace_size;

    size_t num_gigabytes = (s_physical_memory_avail >> 29);
    size_t exponent      = 1;
    while(num_gigabytes > 1)
    {
        num_gigabytes >>= 1;
        exponent <<= 1;
    }
    m_mem_block_size = exponent << 11;

    spdlog::debug("Physical memory = {}!", s_physical_memory_avail);
    spdlog::debug("Memory manager memory allocation block  size = {}!", m_mem_block_size);
    
    m_lru_thread         = std::thread(&MemoryManager::ManageLRU, this);
    m_mem_mgmt_initialized = true;

    {
        std::unique_lock lock(s_lru_config_mutex);
        s_memory_manager_instances.push_back(this);
        UpdateSizeLimit();
    }
 
}


void
MemoryManager::UpdateSizeLimit()
{
    size_t total_weighted_size = 0;

    for(auto& instance : s_memory_manager_instances)
    {
        total_weighted_size += instance->m_trace_weight * instance->m_trace_size;
    }

    for(auto& instance : s_memory_manager_instances)
    {
        size_t size_limit =
            ((instance->m_trace_weight * instance->m_trace_size) / total_weighted_size) *
                            s_physical_memory_avail;
        {
            std::unique_lock<std::mutex> lock(instance->m_lru_cond_mutex);
            instance->m_lru_size_limit = size_limit;
            instance->m_lru_configured = true;
            instance->m_lru_cv.notify_one();
        }

        spdlog::debug("Trace[{}] size = {}!", instance->m_id,
                      instance->m_trace_size);
        spdlog::debug("Trace[{}] memory limit = {}!", instance->m_id, size_limit);
    }
}

void
MemoryManager::Configure(double weight)
{
    std::unique_lock lock(s_lru_config_mutex);

    for(auto& instance : s_memory_manager_instances)
    {
        instance->m_trace_weight = 1.0;
    }
    m_trace_weight = weight;
    UpdateSizeLimit();
}


//std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
//MemoryManager::GetDefaultLRUIterator()
//{
//    return m_lru_array.end();
//}

rocprofvis_result_t
MemoryManager::CancelArrayOwnersip(void* array_ptr, rocprofvis_owner_type_t type)
{

    std::unique_lock lock(m_lru_inuse_mutex[type]);
    auto             it = m_lru_inuse_lookup[type].find(array_ptr);
    if(it != m_lru_inuse_lookup[type].end())
    {
        m_lru_inuse_lookup[type].erase(it);
        {
            std::unique_lock lock(m_lru_cond_mutex);
            m_lru_configured = true;
        }
        m_lru_cv.notify_one();
        return kRocProfVisResultSuccess;
    }

    return kRocProfVisResultNotLoaded;
}

rocprofvis_result_t
MemoryManager::EnterArrayOwnersip(void* array_ptr, rocprofvis_owner_type_t type)
{
    std::unique_lock lock(m_lru_inuse_mutex[type]);
    auto             it = m_lru_inuse_lookup[type].find(array_ptr);
    if(it == m_lru_inuse_lookup[type].end())
    {
        m_lru_inuse_lookup[type].insert(array_ptr);
    }

    return kRocProfVisResultSuccess;
}

bool
MemoryManager::CheckInUse(LRUMember* lru)
{
    bool is_in_use = false;
    {
        std::unique_lock lock(m_lru_inuse_mutex[kRocProfVisOwnerTypeTrack]);
        for(auto ptr : lru->m_array_ptr)
        {
            auto inuse = m_lru_inuse_lookup[kRocProfVisOwnerTypeTrack].find(ptr);
            if(inuse != m_lru_inuse_lookup[kRocProfVisOwnerTypeTrack].end())
            {
                is_in_use = true;
                break;
            }
        }
    }
    if (!is_in_use)
    {
        std::unique_lock lock(m_lru_inuse_mutex[kRocProfVisOwnerTypeGraph]);
        for(auto ptr : lru->m_array_ptr)
        {
            auto inuse = m_lru_inuse_lookup[kRocProfVisOwnerTypeGraph].find(ptr);
            if(inuse != m_lru_inuse_lookup[kRocProfVisOwnerTypeGraph].end())
            {
                is_in_use = true;
                break;
            }
        }
    }
    return is_in_use;
}

//std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
//MemoryManager::AddLRUReference(SegmentTimeline* owner, Segment* reference, uint32_t lod, void* array_ptr)
//{
//    uint64_t         ts  = std::chrono::time_point_cast<std::chrono::nanoseconds>(
//                      std::chrono::system_clock::now())
//                      .time_since_epoch()
//                      .count();
//
//    auto it = reference->GetLRUIterator();
//    if(it != m_lru_array.end())
//    {
//        it->second->m_timestamp = ts;
//        it->second->m_array_ptr.insert(array_ptr);
//    } else
//    {
//        {
//            std::unique_lock lock(m_lru_mutex);
//            auto             pair =
//                m_lru_array.insert(std::pair<Segment*, std::unique_ptr<LRUMember>>(
//                    reference, std::make_unique<LRUMember>(LRUMember{
//                                   ts, owner, reference, { array_ptr }, lod })));
//            if(pair.second)
//            {
//                it = pair.first;
//            }
//        }
//    }
//
//    return it;
//}

void
MemoryManager::AddLRUReference(SegmentTimeline* owner, Segment* reference, uint32_t lod,
                               void* array_ptr)
{
    std::unique_lock lock(m_lru_mutex);
    auto& member_ptr = m_lru_array[owner];
    if(!member_ptr)
    {
        member_ptr = std::make_unique<LRUOwnerMember>();
    }

    member_ptr->m_timestamp = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                                  std::chrono::system_clock::now())
                                  .time_since_epoch()
                                  .count();
    auto& lru = member_ptr->m_lru_segment_array[reference];
    if(!lru)
    {
        lru = std::make_unique<LRUMember>();
    }

    lru->m_array_ptr.insert(array_ptr);

}

void
MemoryManager::LockTimilines(std::vector<SegmentTimeline*>& locked, std::vector<SegmentTimeline*>& failed_to_lock)
{
    std::set<SegmentTimeline*> timelines;
    for(auto& pair : m_lru_array)
    {
        timelines.insert(pair.first);
    }

    for(auto* timeline : timelines)
    {
        if(timeline->GetMutex()->try_lock())
        {
            locked.push_back(timeline);
        } else
        {
            failed_to_lock.push_back(timeline);
        }
    }
 
}

void
MemoryManager::UnlockTimilines(std::vector<SegmentTimeline*>& locked)
{
    for (auto timeline : locked)
    {
        timeline->GetMutex()->unlock();
    }
}

void
MemoryManager::ManageLRU()
{
    bool thread_running = true;
    while(thread_running)
    {
        {
            std::unique_lock lock(m_lru_cond_mutex);
            m_lru_cv.wait(lock, [&] {
                return (m_lru_configured &&
                        m_lru_storage_memory_used > m_lru_size_limit) ||
                       m_lru_mgmt_shutdown;
            });
        }
        {
            std::vector<SegmentTimeline*> locked;
            std::vector<SegmentTimeline*> couldnt_take_lock;
            LockTimilines(locked, couldnt_take_lock);

            if(m_lru_mgmt_shutdown)
            {
                CleanUp();
                thread_running = false;
            }
            else
            {                
                m_lru_configured = false;
                std::vector<std::pair<SegmentTimeline*, LRUOwnerMember*>> sorted_entries;

                {
                    std::unique_lock lock(m_lru_mutex);
                    for(auto& [handle, member_ptr] : m_lru_array)
                        sorted_entries.emplace_back(handle, member_ptr.get());
                }

                std::sort(sorted_entries.begin(), sorted_entries.end(),
                          [](const auto& a, const auto& b) {
                              return a.second->m_timestamp < b.second->m_timestamp;
                          });

                uint64_t ts = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
                for(auto& [owner, member] : sorted_entries)
                {
                    if(couldnt_take_lock.size() > 0 &&
                       std::find(couldnt_take_lock.begin(), couldnt_take_lock.end(),
                                 owner) != couldnt_take_lock.end())
                    {
                        continue;
                    }
                    for(auto it = member->m_lru_segment_array.begin(); it != member->m_lru_segment_array.end();)
                    {
                        if(CheckInUse(it->second.get()))
                        {
                            ++it;
                        }
                        else
                        {
                            Segment* segment = it->first;
                            {
                                std::unique_lock lock(m_lru_mutex);
                                it = member->m_lru_segment_array.erase(it);
                            }
                            owner->Remove(segment);
                        }
                    }

                    if(m_lru_storage_memory_used <= m_lru_size_limit)
                    {
                        break;
                    }
                    uint64_t ts1 = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                                      std::chrono::system_clock::now())
                                      .time_since_epoch()
                                      .count();
                    if (ts1 - ts > 100000000)
                    {
                        break;
                    }
                }

            }
            UnlockTimilines(locked);
        }
    }
}


void*
MemoryManager::Allocate(size_t size, rocprofvis_object_type_t type, SegmentTimeline* owner)
{
    MemoryPool* current_pool = nullptr;
    uint32_t first_zero_bit = 0;

    uint64_t pool_idetifier = uint64_t(owner);

    if (m_short_tracks.find(owner) != m_short_tracks.end())
    {
        pool_idetifier = kShortTracksMemoryPoolIdentifier;
    }
    else
    {
        if (owner->GetMaxNumItems() < m_mem_block_size / 32)
        {
            m_short_tracks.insert(owner);
            pool_idetifier = kShortTracksMemoryPoolIdentifier;
        }
    }

    auto it = m_current_pool[type].find(pool_idetifier); 
    if(it != m_current_pool[type].end())
    {
        current_pool = it->second;
        first_zero_bit = current_pool->m_pos++;
        if(first_zero_bit >= current_pool->m_bitmask.Size())
        {
            current_pool = nullptr;
        }
    }

    if(current_pool == nullptr)
    {
        current_pool = new MemoryPool(size, type, m_mem_block_size);
        auto& inner_map = m_object_pools[pool_idetifier];
        auto [inner_it, inserted] = inner_map.emplace(current_pool->m_base, current_pool);
        if(inserted)
        {
            m_current_pool[type][pool_idetifier] = current_pool;
            m_lru_storage_memory_used += size * current_pool->m_bitmask.Size();
            first_zero_bit = current_pool->m_pos++;
        }
        else
        {
            current_pool = nullptr;
        }
    }
    if(current_pool != nullptr)
    {
        char* ptr = (char*) current_pool->m_base + (first_zero_bit * size);
        current_pool->m_bitmask.Set(first_zero_bit);
        return ptr;
    }
    throw std::runtime_error("Allocation problem!");
    return nullptr;
}

void MemoryManager::CleanUp() {
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    for(auto it = m_object_pools.begin(); it != m_object_pools.end(); ++it)
    {
        for(auto it1 : it->second)
        {
            delete it1.second;
        }
    }
}

Event*
MemoryManager::NewEvent(uint64_t id, double start_ts, double end_ts, SegmentTimeline* owner)
{
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    Event* event = nullptr;
    void*  ptr   = Allocate(sizeof(Event), kRocProfVisObjectTypeEvent, owner);
    if(ptr)
    {
        event = new(ptr) Event(id, start_ts, end_ts);
    }
    return event;
}

Sample*
MemoryManager::NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id,
                         double timestamp, SegmentTimeline* owner)
{
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    Sample* sample = nullptr;
    void*   ptr    = Allocate(sizeof(Sample), kRocProfVisObjectTypeSample, owner);
    if(ptr)
    {
        sample = new(ptr) Sample(type, id, timestamp);
    }
    return sample;
}

SampleLOD*
MemoryManager::NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id,
                            double timestamp, std::vector<Sample*>& children,
                            SegmentTimeline* owner)
{
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    SampleLOD* sample = nullptr;
    void*      ptr    = Allocate(sizeof(SampleLOD), kRocProfVisObjectTypeSampleLOD, owner);
    if(ptr)
    {
        sample = new(ptr) SampleLOD(type, id, timestamp, children);
    }
    return sample;

}

void
MemoryManager::Delete(Handle* handle, SegmentTimeline* owner)
{
    if(!handle->IsDeletable()) return;

    uint64_t pool_idetifier = uint64_t(owner);

    if(m_short_tracks.find(owner) != m_short_tracks.end())
    {
        pool_idetifier = kShortTracksMemoryPoolIdentifier;
    }
    else
    {
        if(owner->GetMaxNumItems() < m_mem_block_size / 32)
        {
            m_short_tracks.insert(owner);
            pool_idetifier = kShortTracksMemoryPoolIdentifier;
        }
    }

    void* ptr = handle;

    auto owner_it = m_object_pools.find(pool_idetifier);
    if(owner_it == m_object_pools.end())
    {
        spdlog::debug("Memory manager error: no pools for owner {}!", (void*) owner);
        return;
    }

    auto& pool_map = owner_it->second;

    auto it = pool_map.lower_bound(ptr);
    if(it == pool_map.end() || it->first > ptr)
    {
        if(it == pool_map.begin())
        {
            spdlog::debug(
                "Memory manager error: ptr {} is before first pool for owner {}!", ptr,
                (void*) owner);
            return;
        }
        --it;
    }

    MemoryPool* pool       = it->second;
    char*       base       = static_cast<char*>(pool->m_base);
    size_t      total_size = pool->m_size * pool->m_bitmask.Size();
    char*       end        = base + total_size;

    if(ptr < base || ptr >= end)
    {
        spdlog::debug(
            "Memory manager error: pool found but ptr {} not inside base {} - end {}",
            ptr, (void*) base, (void*) end);
        return;
    }

    handle->~Handle();
    uint64_t index = (static_cast<char*>(ptr) - base) / pool->m_size;
    pool->m_bitmask.Clear(index);

    if(pool->m_bitmask.None())
    {

        m_lru_storage_memory_used -= total_size;

        pool_map.erase(it);                         
        m_current_pool[pool->m_type].erase(pool_idetifier);  

        delete pool;
    }
}


void
BitSet::Set(size_t pos)
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    if(word < m_bits.size())
    {
        m_bits[word] |= (1ULL << bit);
    }
}

void
BitSet::Clear(size_t pos)
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    if(word < m_bits.size())
    {
        m_bits[word] &= ~(1ULL << bit);
    }
}

bool
BitSet::Test(size_t pos) const
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    if(word < m_bits.size())
    {
        return m_bits[word] & (1ULL << bit);
    }
    else
    {
        return false;
    }
}

bool
BitSet::None() const
{
    for(const auto& word : m_bits)
    {
        if(word != 0) return false;
    }
    return true;
}

uint32_t
BitSet::FindFirstZero() const
{
    for(size_t i = 0; i < m_bits.size(); ++i)
    {
        uint64_t w = m_bits[i];
        if(~w != 0)
        {
            uint64_t free_bits = ~w;
#if defined(_MSC_VER)
            unsigned long bit_index;
            _BitScanForward64(&bit_index, free_bits);
            size_t bit_pos = i * WORD_SIZE + bit_index;
#else
            size_t bit_index = __builtin_ctzll(free_bits);
            size_t bit_pos   = i * WORD_SIZE + bit_index;
#endif
            if(bit_pos < Size()) return bit_pos;
        }
    }
    return Size();
}

uint32_t
BitSet::Count() const {
    size_t total = 0;
    for(const auto& word : m_bits)
    {
#if defined(_MSC_VER)
        total += __popcnt64(word);
#else
        total += __builtin_popcountll(word);
#endif
    }
    return total;
}


}  // namespace Controller
}  // namespace RocProfVis
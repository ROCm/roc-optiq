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

size_t g_physical_memory_avail = 0;
size_t g_total_loaded_size = 0;
uint32_t g_num_traces = 0;

MemoryManager::MemoryManager()
: m_lru_storage_memory_used(0)
, m_lru_mgmt_shutdown(false)
, m_mem_mgmt_initialized(false)
, m_mem_block_size(1024)
{ 
    g_num_traces++;
    for(int type = 0; type < kRocProfVisNumberOfObjectTypes; type++)
    {
        m_current_pool[type] = m_object_pools.end();
    }
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
    g_num_traces--;
}

// start memory manager LRU processing when trace size is known
void MemoryManager::Init(size_t trace_size)
{
    if(g_physical_memory_avail == 0)
    {
#if defined(_MSC_VER)
        MEMORYSTATUSEX mem_info;
        mem_info.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&mem_info);

        g_physical_memory_avail = (mem_info.ullAvailPhys / 100) * kUseVailMemoryPercent;

#elif defined(__GNUC__) || defined(__clang__)
        struct sysinfo mem_info;
        sysinfo(&mem_info);

        uint64_t avail_phys_mem = mem_info.freeram;
        avail_phys_mem *= mem_info.mem_unit;

        g_physical_memory_avail = (avail_phys_mem / 100) * kUseVailMemoryPercent;
#endif
    }

    m_trace_size = trace_size;
    g_total_loaded_size += trace_size;

    size_t num_gigabytes = (g_physical_memory_avail >> 29);
    size_t exponent      = 1;
    while(num_gigabytes > 1)
    {
        num_gigabytes >>= 1;
        exponent <<= 1;
    }
    m_mem_block_size = exponent << 11;
    spdlog::debug("Physical memory = {}!", g_physical_memory_avail);
    spdlog::debug("Trace size = {}!", m_trace_size);
    spdlog::debug("All traces size = {}!", g_total_loaded_size);
    spdlog::debug("Memory manager memory limit = {}!", GetMemoryManagerSizeLimit());
    spdlog::debug("Memory manager memory allocation block  size = {}!", m_mem_block_size);
    
    m_lru_thread         = std::thread(&MemoryManager::ManageLRU, this);
    m_mem_mgmt_initialized = true;
}

size_t
MemoryManager::GetMemoryManagerSizeLimit()
{ 
    return std::max( 
                    (int64_t)m_trace_size + (((int64_t) g_physical_memory_avail - (int64_t) g_total_loaded_size) / g_num_traces),
                    (int64_t)100000000
    );
}


std::mutex& MemoryManager::GetMemoryManagerMutex()
{
    return m_lru_mutex;
}

std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
MemoryManager::GetDefaultLRUIterator()
{
    return m_lru_array.end();
}

rocprofvis_result_t
MemoryManager::CancelArrayOwnersip(void* array_ptr)
{

    std::unique_lock lock(m_lru_inuse_mutex);
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
MemoryManager::EnterArrayOwnersip(void* array_ptr)
{
    std::unique_lock lock(m_lru_inuse_mutex);
    auto             it = m_lru_inuse_lookup.find(array_ptr);
    if(it == m_lru_inuse_lookup.end())
    {
        m_lru_inuse_lookup.insert(array_ptr);
    }

    return kRocProfVisResultSuccess;
}


std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator
MemoryManager::AddLRUReference(SegmentTimeline* owner, Segment* reference, uint32_t lod, void* array_ptr)
{
    uint64_t         ts  = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now())
                      .time_since_epoch()
                      .count();

    auto it = reference->GetLRUIterator();
    if(it != m_lru_array.end())
    {
        it->second->m_timestamp = ts;
        it->second->m_array_ptr   = array_ptr;
    } else
    {
        auto pair = m_lru_array.insert(std::pair<Segment*, std::unique_ptr<LRUMember>>(
            reference,
            std::make_unique<LRUMember>(LRUMember{ ts, owner, reference, array_ptr, lod })));
        if(pair.second)
        {
            it = pair.first;
        }
    }

    return it;
}


void
MemoryManager::ManageLRU()
{
    bool thread_running = true;
    while(thread_running)
    {
        std::unique_lock lock(m_lru_mutex);
        m_lru_cv.wait(lock, [&] {
            return (m_array_ownership_changed &&
                    m_lru_storage_memory_used > GetMemoryManagerSizeLimit()) ||
                   m_lru_mgmt_shutdown;
        });
        if (m_lru_mgmt_shutdown)
        {
            CleanUp();
            thread_running = false;
        }
        else
        {
            size_t mem_size_limit = GetMemoryManagerSizeLimit();
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
                {
                    std::unique_lock lock(m_lru_inuse_mutex);
                    auto inuse = m_lru_inuse_lookup.find(lru->m_array_ptr);
                    if(inuse != m_lru_inuse_lookup.end())
                    {
                        continue;
                    }
                }

                Segment* reference    = lru->m_reference;
                uint32_t lod          = lru->m_lod;
                SegmentTimeline* owner        = lru->m_owner;

                m_lru_array.erase(reference);

                owner->Remove(reference);

                
                if(m_lru_storage_memory_used <= mem_size_limit)
                {
                    break;
                }
            }
        }    
    }
}


void*
MemoryManager::Allocate(size_t size, rocprofvis_object_type_t type)
{
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    MemoryPool* current_pool = nullptr;
    uint32_t first_zero_bit = 0;
    if(m_current_pool[type] != m_object_pools.end())
    {
        current_pool = m_current_pool[type]->second;
        first_zero_bit =
            current_pool->m_pos >= current_pool->m_bitmask.Size() || 
                                   current_pool->m_bitmask.Test(current_pool->m_pos)
                             ? current_pool->m_bitmask.FindFirstZero()
                             : current_pool->m_pos++;
        if (first_zero_bit == current_pool->m_bitmask.Size())
        {
            current_pool = nullptr;
            first_zero_bit = 0;
        }
    }

    if(current_pool == nullptr)
    {
        current_pool = new MemoryPool(size, m_mem_block_size);
        auto pair = m_object_pools.insert(std::pair<void*, MemoryPool*>(current_pool->m_base, current_pool));
        if(pair.second)
        {
            m_current_pool[type] = pair.first;
            m_lru_storage_memory_used += size * current_pool->m_bitmask.Size();
        }
    }
    char* ptr = (char*) current_pool->m_base + (first_zero_bit * size);
    current_pool->m_bitmask.Set(first_zero_bit);
    return ptr;
}

void MemoryManager::CleanUp() {

    for(auto it = m_object_pools.begin(); it != m_object_pools.end(); ++it)
    {
            delete it->second;
    }
}

Event*
MemoryManager::NewEvent(uint64_t id, double start_ts, double end_ts)
{
    Event* event = nullptr;
    void*  ptr   = Allocate(sizeof(Event), kRocProfVisObjectTypeEvent);
    if(ptr)
    {
        event = new(ptr) Event(id, start_ts, end_ts);
    }
    return event;
}

Event*
MemoryManager::NewEvent(Event* other)
{
    Event* event = nullptr;
    void*  ptr   = Allocate(sizeof(Event), kRocProfVisObjectTypeEvent);
    if(ptr)
    {
        event = new(ptr) Event(other);
    }
    return event;
}

Sample*
MemoryManager::NewSample(rocprofvis_controller_primitive_type_t type, uint64_t id,
                         double timestamp)
{
    Sample* sample = nullptr;
    void*   ptr    = Allocate(sizeof(Sample), kRocProfVisObjectTypeSample);
    if(ptr)
    {
        sample = new(ptr) Sample(type, id, timestamp);
    }
    return sample;
}

SampleLOD*
MemoryManager::NewSampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id,
                            double timestamp, std::vector<Sample*>& children)
{
    SampleLOD* sample = nullptr;
    void*      ptr    = Allocate(sizeof(SampleLOD), kRocProfVisObjectTypeSampleLOD);
    if(ptr)
    {
        sample = new(ptr) SampleLOD(type, id, timestamp, children);
    }
    return sample;

}

void
MemoryManager::Delete(Handle* handle)
{
    void* ptr = handle;
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    auto  it = m_object_pools.upper_bound(ptr);
    --it;
    MemoryPool* pool = it->second;
    char*       base      = (char*)pool->m_base;
    size_t      size      = pool->m_size * pool->m_bitmask.Size();
    char*       end       = base + size;
    if(ptr >= base && ptr < end) 
    {
        handle->~Handle();
        uint64_t index = ((char*)ptr - base) / pool->m_size;
        pool->m_bitmask.Clear(index);
        pool->m_pos = index;
        
        if(pool->m_bitmask.None())
        {
            m_lru_storage_memory_used -= size;
            m_object_pools.erase(it);
            delete pool;
        }
    } else
    {
        spdlog::debug("Memory manager error : memory address not found!");
    }
    
}


void
BitSet::Set(size_t pos)
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    m_bits[word] |= (1ULL << bit);
}

void
BitSet::Clear(size_t pos)
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    m_bits[word] &= ~(1ULL << bit);
}

bool
BitSet::Test(size_t pos) const
{
    size_t word = pos / WORD_SIZE;
    size_t bit  = pos % WORD_SIZE;
    return m_bits[word] & (1ULL << bit);
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
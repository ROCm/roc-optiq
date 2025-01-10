#ifndef EVENT_RECORD__H
#define EVENT_RECORD__H

#include <cstdint>

class Trace;

class EventRecord 
{
    public:
        EventRecord(const uint64_t id, const uint64_t timestamp, const int64_t duration, const uint32_t typeId, const uint32_t descriptionId):
            m_id(id), m_timestamp(timestamp), m_duration(duration), m_typeId(typeId), m_descriptionId(descriptionId) {};

        const uint64_t          getId() {return m_id;};
        const uint64_t          getTimestamp() {return m_timestamp;};
        const int64_t           getDuration() {return m_duration;};
        const char*             getTypeName(Trace* ctx);
        const char*             getDescription(Trace* ctx);

    private:
        uint64_t                m_id;
        uint64_t                m_timestamp;
        int64_t                 m_duration;
        uint32_t                m_typeId; 
        uint32_t                m_descriptionId;
};



#endif //EVENT_RECORD__H
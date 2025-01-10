#ifndef METRIC_RECORD__H
#define METRIC_RECORD__H

#include <cstdint>


class MetricRecord 
{
    public:
        MetricRecord(const uint64_t timestamp, const double value):
            m_timestamp(timestamp), m_value(value) {};
        const uint64_t        getTimestamp() {return m_timestamp;};
        const double          getValue() {return m_value;};
    private:
        uint64_t        m_timestamp;
        double          m_value;
};



#endif //METRIC_RECORD__H
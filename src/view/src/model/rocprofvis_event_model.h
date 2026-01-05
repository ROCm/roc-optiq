// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"

#include <unordered_map>

namespace RocProfVis
{
namespace View
{

/**
 * @brief Manages event data.
 *
 * This model holds event information that can be searched, filtered, and displayed.
 */
class EventModel
{
public:
    EventModel()  = default;
    ~EventModel() = default;

    // Event access
    const EventInfo* GetEvent(uint64_t event_id) const;
    EventInfo*       GetEvent(uint64_t event_id);
    bool             AddEvent(uint64_t event_id, EventInfo&& event);
    bool             RemoveEvent(uint64_t event_id);
    void             ClearEvents();
    size_t           EventCount() const;

private:
    std::unordered_map<uint64_t, EventInfo> m_event_data;
};

}  // namespace View
}  // namespace RocProfVis

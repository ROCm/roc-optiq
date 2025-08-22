#include "rocprofvis_annotations.h"

namespace RocProfVis
{
namespace View
{

Annotations::Annotations()
{
    // Constructor implementation
}

Annotations::~Annotations()
{
    // Destructor implementation
}

void
Annotations::AddAnnotation(uint64_t track_id, const std::string& text, double timestamp)
{
    m_annotations.push_back({ track_id, text, timestamp });
}

std::vector<std::string>
Annotations::GetAnnotations(uint64_t track_id) const
{
    std::vector<std::string> result;
    for(const auto& annotation : m_annotations)
    {
        if(annotation.track_id == track_id)
        {
            result.push_back(annotation.text);
        }
    }
    return result;
}

bool
Annotations::RemoveAnnotation(uint64_t track_id, double timestamp)
{
    for(auto it = m_annotations.begin(); it != m_annotations.end(); ++it)
    {
        if(it->track_id == track_id && it->timestamp == timestamp)
        {
            m_annotations.erase(it);
            return true;
        }
    }
    return false;
}

}  // namespace View
}  // namespace RocProfVis
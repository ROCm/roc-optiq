#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class Annotations
{
public:
    Annotations();
    ~Annotations();

    // Example: Add an annotation
    void AddAnnotation(uint64_t track_id, const std::string& text, double timestamp);

    // Example: Get all annotations for a track
    std::vector<std::string> GetAnnotations(uint64_t track_id) const;

    // Example: Remove an annotation
    bool RemoveAnnotation(uint64_t track_id, double timestamp);

private:
    // Internal storage for annotations (replace with your actual data structure)
    struct Annotation
    {
        uint64_t    track_id;
        std::string text;
        double      timestamp;
    };

    std::vector<Annotation> m_annotations;
};

}  // namespace View
}  // namespace RocProfVis
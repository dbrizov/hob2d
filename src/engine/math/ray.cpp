#include "ray.h"

#include <format>

namespace hob {
    std::string Ray::to_string() const {
        return std::format("Ray(origin={}, direction={})", origin.to_string(), direction.to_string());
    }
} // namespace hob

#include "vector3.h"

#include <format>

namespace hob {
    std::string Vector3::to_string() const {
        return std::format("({:.2f}, {:.2f}, {:.2f})", x, y, z);
    }
} // namespace hob

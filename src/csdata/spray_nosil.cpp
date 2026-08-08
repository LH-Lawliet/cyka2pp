#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

constexpr SprayPoint k_nosil_0[] = {
    {0.0, -0.125},
    {0.0, -0.09375},
    {0.0, -0.5625},
    {-0.0625, -1.15625},
    {0.03125, -1.6875},
    {-0.1875, -2.46875},
    {-0.28125, -2.9375},
    {0.21875, -3.5},
    {0.4375, -3.75},
    {1.09375, -3.9375},
    {0.84375, -4.1875},
    {0.25, -4.34375},
    {-0.625, -4.21875},
    {-1.25, -4.0},
    {-1.361227, -3.907737},
    {-1.439331, -3.907737},
    {-1.562064, -3.898394},
    {-1.71875, -3.84375},
    {-1.875, -3.65625},
    {-1.6875, -3.875},
};
} // namespace

SpraySpan find_nosil(std::string_view w) {
    if (w == "M4A1") {
        return {k_nosil_0, 20};
    }
    return {};
}

} // namespace cyka::csdata::detail

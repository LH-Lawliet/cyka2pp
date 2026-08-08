#pragma once

#include "cyka/options.hpp"

#include <span>
#include <string>
#include <string_view>

namespace cyka::cli {

struct Args {
    std::string demo;
    Options options;
    bool help{false};
    bool ok{true};
    std::string error;
};

[[nodiscard]] Args parse_args(std::span<char*> argv);
void print_help(std::string_view prog);

} // namespace cyka::cli

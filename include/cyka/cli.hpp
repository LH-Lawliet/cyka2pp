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

[[nodiscard]] Args parseArgs(std::span<char*> argv);
void printHelp(std::string_view prog);

} // namespace cyka::cli

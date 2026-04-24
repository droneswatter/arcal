#pragma once

#include <cstdint>
#include <string>

namespace arcal::lacal {

struct Options {
    std::string host{"127.0.0.1"};
    uint16_t port = 8766;
    int domain = 0;
};

Options parseArgs(int argc, char** argv);
int runServer(const Options& opts);

} // namespace arcal::lacal

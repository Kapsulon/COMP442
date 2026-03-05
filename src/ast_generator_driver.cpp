#include <fstream>
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    spdlog::set_pattern("[%^%-7l%$] %v");

    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    lang::SyntacticAnalyzer syntacticAnalyzer(false);

    for (std::uint64_t idx = 1; idx != argc; idx++) {
        syntacticAnalyzer.openFile(argv[idx]);
        syntacticAnalyzer.parse();
    }

    return 0;
}

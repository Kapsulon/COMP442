#include <fstream>
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

#define OUTPUT_SETS 0

int main(int argc, char **argv)
{
    spdlog::set_pattern("[%^%l%$] %v");

    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }


    lang::SyntacticAnalyzer syntacticAnalyzer;

    for (std::uint64_t idx = 1; idx != argc; idx++) {
        syntacticAnalyzer.openFile(argv[idx]);
#if OUTPUT_SETS
        std::ofstream first(std::string(argv[idx]) + ".out.first");
        std::ofstream follow(std::string(argv[idx]) + ".out.follow");
        first << syntacticAnalyzer.getFirstSet();
        follow << syntacticAnalyzer.getFollowSet();
        first.close();
        follow.close();
#endif
        syntacticAnalyzer.parse();
    }

    return 0;
}

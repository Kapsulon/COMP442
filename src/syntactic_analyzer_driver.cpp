#include <fstream>
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    lang::SyntacticAnalyzer syntacticAnalyzer;

    for (std::uint64_t idx = 1; idx != argc; idx++) {
        syntacticAnalyzer.openFile(argv[idx]);
        std::ofstream first(std::string(argv[idx]) + ".out.first");
        std::ofstream follow(std::string(argv[idx]) + ".out.follow");
        first << syntacticAnalyzer.getFirstSet();
        follow << syntacticAnalyzer.getFollowSet();
        first.close();
        follow.close();
        syntacticAnalyzer.parse();
    }

    return 0;
}

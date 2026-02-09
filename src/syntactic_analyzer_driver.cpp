#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    return 0;
}

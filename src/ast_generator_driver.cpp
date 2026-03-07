#include <fstream>
#include <functional>
#include <string>

#include "AST/ASTNode.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    spdlog::set_pattern("[%^%-7l%$] %v");

    if (argc < 2) {
        spdlog::error("No input file specified.");
        return 1;
    }

    lang::SyntacticAnalyzer syntacticAnalyzer;

    for (std::uint64_t idx = 1; idx != static_cast<std::uint64_t>(argc); idx++) {
        syntacticAnalyzer.openFile(argv[idx]);
        syntacticAnalyzer.parse();

        auto ast = syntacticAnalyzer.getAST();
        if (!ast) {
            spdlog::warn("{}: No AST produced (parse errors?)", argv[idx]);
        } else {
            syntacticAnalyzer.outputDotAST();
        }

        syntacticAnalyzer.outputDerivationSteps();
        syntacticAnalyzer.outputSyntaxErrors();
    }

    return 0;
}

#include <fstream>
#include <functional>
#include <string>

#include "AST/ASTNode.hpp"
#include "SemanticAnalyzer/SemanticAnalyzer.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"
#include "spdlog/spdlog.h"

int main(int argc, char **argv)
{
    spdlog::set_pattern("[%^%-7l%$] %v");

    if (argc < 2) {
        spdlog::error("No input file(s) specified.");
        return 1;
    }

    lang::SemanticAnalyzer semanticAnalyzer;

    for (int idx = 1; idx != argc; idx++) {
        semanticAnalyzer.openFile(argv[idx]);
        semanticAnalyzer.parse();
        semanticAnalyzer.outputSymbolTable();
    }

    return 0;
}

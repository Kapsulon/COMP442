#pragma once

#include <string_view>
#include "Problems/Problems.hpp"
#include "SyntacticAnalyzer/SyntacticAnalyzer.hpp"

namespace lang
{
    class SemanticAnalyzer
    {
    public:
        void openFile(std::string_view path);

        void parse();

        void outputSymbolTables() const;

    private:
        Problems m_problems;
        SyntacticAnalyzer m_syntacticAnalyzer;
    };
} // namespace lang

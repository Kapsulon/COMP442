#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"

namespace lang
{
    class SyntacticAnalyzer
    {
    public:
        SyntacticAnalyzer() = default;
        ~SyntacticAnalyzer();

        void openFile(std::string_view path);
        void closeFile();

        void parse();

    private:
        LexicalAnalyzer m_lexicalAnalyzer;

        std::vector<Token> m_tokens;
    };
} // namespace lang

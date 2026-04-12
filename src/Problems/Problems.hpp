#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "LexicalAnalyzer/LexicalAnalyzer.hpp"

namespace lang
{
    class Problems
    {
    public:
        void info(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens);
        void warn(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens);
        void error(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens);

        std::uint64_t getProblemCount() const;
        std::uint64_t getInfoCount() const;
        std::uint64_t getWarningCount() const;
        std::uint64_t getErrorCount() const;

        std::string getProblems(const LexicalAnalyzer &lexer) const;
        void displayProblems(const LexicalAnalyzer &lexer) const;
        void outputProblems(const LexicalAnalyzer &lexer, std::string_view path) const;

        void merge(const Problems &other);
        void clear();

    private:
        struct Problem {
            enum class Level {
                INFO,
                WARNING,
                ERROR
            };

            std::vector<Token> tokens;
            std::string kind;
            std::string message;
            Level level;
        };

        std::string getProblemString(const LexicalAnalyzer &lexer, const Problem &problem) const;

        std::vector<Problem> m_problems;
    };
} // namespace lang

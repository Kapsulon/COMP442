#include "Problems.hpp"
#include <algorithm>
#include <format>
#include <utility>
#include "LexicalAnalyzer/LexicalAnalyzer.hpp"
#include "spdlog/spdlog.h"

static std::string underlineProblematicTokens(const std::vector<lang::Token> &tokens)
{
    std::string res;
    for (const auto &token : tokens) {
        if (token.pos > 1)
            res.append(static_cast<size_t>(token.pos - 1), ' ');
        res.push_back('^');
        if (token.lexeme.size() > 1)
            res.append(token.lexeme.size() - 1, '~');
    }
    return res;
}

static std::string expandTabs(std::string_view line)
{
    std::string out;
    out.reserve(line.size());

    std::uint32_t col = 1;
    for (char ch : line) {
        if (ch == '\r')
            continue;
        if (ch == '\t') {
            auto spaces = static_cast<size_t>(NEXT_TAB_POS(col));
            out.append(spaces, ' ');
            col += static_cast<std::uint32_t>(spaces);
        } else {
            out.push_back(ch);
            col += 1;
        }
    }

    return out;
}

namespace lang
{
    void Problems::info(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens)
    {
        m_problems.emplace_back(Problem{ .tokens = std::move(tokens), .kind = kind, .message = message, .level = Problem::Level::INFO });
    }

    void Problems::warn(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens)
    {
        m_problems.emplace_back(Problem{ .tokens = std::move(tokens), .kind = kind, .message = message, .level = Problem::Level::WARNING });
    }

    void Problems::error(const std::string &kind, const std::string &message, std::initializer_list<Token> tokens)
    {
        m_problems.emplace_back(Problem{ .tokens = std::move(tokens), .kind = kind, .message = message, .level = Problem::Level::ERROR });
    }

    std::uint64_t Problems::getProblemCount() const
    {
        return m_problems.size();
    }

    std::uint64_t Problems::getInfoCount() const
    {
        return std::count_if(m_problems.begin(), m_problems.end(), [&](const Problem &problem) { return problem.level == Problem::Level::INFO; });
    }

    std::uint64_t Problems::getWarningCount() const
    {
        return std::count_if(m_problems.begin(), m_problems.end(), [&](const Problem &problem) { return problem.level == Problem::Level::WARNING; });
    }

    std::uint64_t Problems::getErrorCount() const
    {
        return std::count_if(m_problems.begin(), m_problems.end(), [&](const Problem &problem) { return problem.level == Problem::Level::ERROR; });
    }

    std::string Problems::getProblemString(const LexicalAnalyzer &lexer, const Problem &problem) const
    {
        std::string line = expandTabs(lexer.getLine(problem.tokens[0].line));
        std::string underline = underlineProblematicTokens(problem.tokens);

        std::string problemString = std::format(
            "{}:{}:{}: {}: {}\n"
            "  {}\t|\t{}\n"
            "  \t|\t{}\n",
            problem.tokens[0].file_path,
            problem.tokens[0].line,
            problem.tokens[0].pos,
            problem.kind,
            problem.message,
            problem.tokens[0].line,
            line,
            underline);

        return problemString;
    }

    std::string Problems::getProblems(const LexicalAnalyzer &lexer) const
    {
        std::string res;

        for (auto &problem : m_problems) res.append(getProblemString(lexer, problem));

        return res;
    }

    void Problems::displayProblems(const LexicalAnalyzer &lexer) const
    {
        for (auto &problem : m_problems) {
            switch (problem.level) {
                case Problem::Level::INFO:
                    spdlog::info(getProblemString(lexer, problem));
                    break;
                case Problem::Level::WARNING:
                    spdlog::warn(getProblemString(lexer, problem));
                    break;
                case Problem::Level::ERROR:
                    spdlog::error(getProblemString(lexer, problem));
                    break;
            }
        }
    }

    void Problems::clear()
    {
        m_problems.clear();
    }
} // namespace lang

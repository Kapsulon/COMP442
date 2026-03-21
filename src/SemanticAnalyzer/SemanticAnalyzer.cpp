#include "SemanticAnalyzer.hpp"

#include "spdlog/spdlog.h"
#include "utils/colors.hpp"

namespace lang
{
    void SemanticAnalyzer::openFile(std::string_view path)
    {
        m_syntacticAnalyzer.openFile(path);
    }

    void SemanticAnalyzer::parse()
    {
        m_syntacticAnalyzer.parse();
        auto ast = m_syntacticAnalyzer.getAST();

        if (!ast) {
            spdlog::warn("{}: No AST produced (parse errors?)", m_syntacticAnalyzer.getCurrentFilePath());
            return;
        }

        auto start = std::chrono::high_resolution_clock::now();

        // TODO: semantic analysis here

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        m_problems.displayProblems(m_syntacticAnalyzer.getLexer());

        spdlog::info(
            "{}: Analyzed in \t\t {:.2f}ms \t [" CYAN "{} info(s)" RESET ", " YELLOW "{} warning(s)" RESET ", " RED "{} error(s)" RESET "]",
            m_syntacticAnalyzer.getCurrentFilePath(),
            elapsed.count(),
            m_problems.getInfoCount(),
            m_problems.getWarningCount(),
            m_problems.getErrorCount());
    }

    void SemanticAnalyzer::outputSymbolTables() const {}

} // namespace lang

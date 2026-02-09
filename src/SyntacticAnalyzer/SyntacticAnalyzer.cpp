#include "SyntacticAnalyzer.hpp"

namespace lang
{
    SyntacticAnalyzer::~SyntacticAnalyzer()
    {
        closeFile();
    }

    void SyntacticAnalyzer::openFile(std::string_view path)
    {
        m_lexicalAnalyzer.closeFile();

        m_tokens.resize(0);
        std::uint64_t read_bytes = m_lexicalAnalyzer.readFile(path);
        m_tokens.reserve(read_bytes / 10);
    }

    void SyntacticAnalyzer::closeFile()
    {
        m_lexicalAnalyzer.closeFile();
    }

    void SyntacticAnalyzer::parse()
    {
        while (true) {
            Token token = m_lexicalAnalyzer.next();
            m_tokens.push_back(token);

            if (token.type == TokenType::END_OF_FILE)
                break;
        }
    }
} // namespace lang

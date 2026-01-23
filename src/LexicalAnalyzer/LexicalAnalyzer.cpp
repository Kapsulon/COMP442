#include "LexicalAnalyzer.hpp"
#include <iterator>
#include <regex>
#include "spdlog/spdlog.h"

std::string lang::tokenTypeToString(TokenType type)
{
    switch (type) {
        case lang::TokenType::BLOCK_COMMENT:
            return "BLOCK_COMMENT";
        case lang::TokenType::INLINE_COMMENT:
            return "INLINE_COMMENT";
        case lang::TokenType::INT_NUM:
            return "INT_NUM";
        case lang::TokenType::FLOAT_NUM:
            return "FLOAT_NUM";
        case lang::TokenType::ID:
            return "ID";
        case lang::TokenType::EQUAL:
            return "EQUAL";
        case lang::TokenType::NOT_EQUAL:
            return "NOT_EQUAL";
        case lang::TokenType::LESS_THAN:
            return "LESS_THAN";
        case lang::TokenType::GREATER_THAN:
            return "GREATER_THAN";
        case lang::TokenType::LESS_EQUAL:
            return "LESS_EQUAL";
        case lang::TokenType::GREATER_EQUAL:
            return "GREATER_EQUAL";
        case lang::TokenType::PLUS:
            return "PLUS";
        case lang::TokenType::MINUS:
            return "MINUS";
        case lang::TokenType::MULTIPLY:
            return "MULTIPLY";
        case lang::TokenType::DIVIDE:
            return "DIVIDE";
        case lang::TokenType::ASSIGN:
            return "ASSIGN";
        case lang::TokenType::OPEN_PARENTHESIS:
            return "OPEN_PARENTHESIS";
        case lang::TokenType::CLOSE_PARENTHESIS:
            return "CLOSE_PARENTHESIS";
        case lang::TokenType::OPEN_BRACE:
            return "OPEN_BRACE";
        case lang::TokenType::CLOSE_BRACE:
            return "CLOSE_BRACE";
        case lang::TokenType::OPEN_BRACKET:
            return "OPEN_BRACKET";
        case lang::TokenType::CLOSE_BRACKET:
            return "CLOSE_BRACKET";
        case lang::TokenType::SEMICOLON:
            return "SEMICOLON";
        case lang::TokenType::COMMA:
            return "COMMA";
        case lang::TokenType::DOT:
            return "DOT";
        case lang::TokenType::COLON:
            return "COLON";
        case lang::TokenType::DOUBLE_COLON:
            return "DOUBLE_COLON";
        case lang::TokenType::IF:
            return "IF";
        case lang::TokenType::THEN:
            return "THEN";
        case lang::TokenType::ELSE:
            return "ELSE";
        case lang::TokenType::WHILE:
            return "WHILE";
        case lang::TokenType::CLASS:
            return "CLASS";
        case lang::TokenType::INTEGER:
            return "INTEGER";
        case lang::TokenType::FLOAT:
            return "FLOAT";
        case lang::TokenType::DO:
            return "DO";
        case lang::TokenType::END:
            return "END";
        case lang::TokenType::PUBLIC:
            return "PUBLIC";
        case lang::TokenType::PRIVATE:
            return "PRIVATE";
        case lang::TokenType::OR:
            return "OR";
        case lang::TokenType::AND:
            return "AND";
        case lang::TokenType::NOT:
            return "NOT";
        case lang::TokenType::READ:
            return "READ";
        case lang::TokenType::WRITE:
            return "WRITE";
        case lang::TokenType::RETURN:
            return "RETURN";
        case lang::TokenType::INHERITS:
            return "INHERITS";
        case lang::TokenType::LOCAL:
            return "LOCAL";
        case lang::TokenType::VOID:
            return "VOID";
        case lang::TokenType::MAIN:
            return "MAIN";
        default:
            return "UNKNOWN";
    }
}

const std::array<std::pair<lang::TokenType, std::regex>, 5> lang::LexicalAnalyzer::m_RegExPatterns{
    std::make_pair(lang::TokenType::BLOCK_COMMENT, MAKE_REGEX(R"(^\/\*[\s\S]*?\*\/)")),
    std::make_pair(lang::TokenType::INLINE_COMMENT, MAKE_REGEX(R"(^\/\/.*$)")),
    std::make_pair(lang::TokenType::FLOAT_NUM, MAKE_REGEX(R"(^([1-9][0-9]*|0)\.([0-9]*[1-9]|0)(e(\+|\-)?([1-9][0-9]*|0))?)")),
    std::make_pair(lang::TokenType::INT_NUM, MAKE_REGEX(R"(^([1-9][0-9]*)|0)")),
    std::make_pair(lang::TokenType::ID, MAKE_REGEX(R"(^[a-zA-Z]([a-zA-Z]|[0-9]|_)*)"))
};

lang::Token lang::LexicalAnalyzer::makeToken(TokenType type, std::string lexeme)
{
    return { .type = type, .lexeme = lexeme, .line = m_lineNumber, .pos = m_position };
}

void lang::LexicalAnalyzer::readFile(std::string_view path)
{
    std::ifstream file(path.data());
    m_lineNumber = 1;
    m_position = 1;
    m_iter = 0;

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + std::string(path));
    }

    m_file_contents = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

lang::Token lang::LexicalAnalyzer::next()
{
    if (m_iter >= m_file_contents.size())
        return makeToken(TokenType::END_OF_FILE, "");

    while (m_file_contents[m_iter] == ' ' || m_file_contents[m_iter] == '\n' || m_file_contents[m_iter] == '\t' || m_file_contents[m_iter] == '\r') {
        if (m_file_contents[m_iter] == '\n') {
            m_lineNumber++;
            m_position = 1;
        } else if (m_file_contents[m_iter] == '\t') {
            m_position += NEXT_TAB_POS(m_position);
        } else if (m_file_contents[m_iter] != '\r') {
            m_position++;
        }
        m_iter++;
    }

    if (m_iter >= m_file_contents.size())
        return makeToken(TokenType::END_OF_FILE, "");

    std::string sub = m_file_contents.substr(m_iter, m_file_contents.size() - m_iter);

    lang::Token token = runRegEx(sub);
    if (token.type != lang::TokenType::UNKNOWN) {
        if (token.type == lang::TokenType::ID) {
            lang::Token keywordToken = checkKeywords(token);
            if (keywordToken.type != lang::TokenType::UNKNOWN) {
                return keywordToken;
            }
        }
        return token;
    } else {
        lang::Token operatorToken = checkOperators(sub);
        if (operatorToken.type != lang::TokenType::UNKNOWN) {
            return operatorToken;
        }
    }

    lang::Token unknownToken = makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
    m_iter++;
    m_position++;
    return unknownToken;
}

lang::Token lang::LexicalAnalyzer::runRegEx(const std::string &sub)
{
    std::regex test = MAKE_REGEX(R"(^\/\*[\s\S]*?\*\/)");
    for (auto pattern = m_RegExPatterns.begin(); pattern != m_RegExPatterns.end(); pattern++) {
        std::smatch match;

        if (std::regex_search(sub, match, pattern->second) && match.position() == 0) {
            lang::Token token = makeToken(pattern->first, match.str(0));

            for (char c : match.str(0)) {
                if (c == '\n') {
                    m_lineNumber++;
                    m_position = 1;
                } else if (c == '\t') {
                    m_position += NEXT_TAB_POS(m_position);
                } else if (c != '\r') {
                    m_position++;
                }
                m_iter++;
            }

            return token;
        }
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}

lang::Token lang::LexicalAnalyzer::checkKeywords(lang::Token &token)
{
    for (const auto &keyword : m_Keywords) {
        if (token.lexeme.substr(0, keyword.first.size()) == keyword.first) {
            token.type = keyword.second;
            return token;
        }
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}

lang::Token lang::LexicalAnalyzer::checkOperators(const std::string &sub)
{
    for (const auto &op : m_Operators) {
        if (sub.substr(0, op.first.size()) == op.first) {
            lang::Token token = makeToken(op.second, std::string(op.first));
            m_iter += op.first.size();
            m_position += op.first.size();
            return token;
        }
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}

#include "LexicalAnalyzer.hpp"
#include <fcntl.h>
#include <iterator>
#include <regex>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include "ctre.hpp"
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

lang::Token lang::LexicalAnalyzer::makeToken(TokenType type, std::string lexeme)
{
    return { .type = type, .lexeme = lexeme, .line = m_lineNumber, .pos = m_position };
}

std::uint64_t lang::LexicalAnalyzer::readFile(std::string_view path)
{
    m_fd = open(path.data(), O_RDONLY);

    if (!m_fd) {
        throw std::runtime_error("Failed to open file: " + std::string(path));
    }

    size_t size = lseek(m_fd, 0, SEEK_END);

    m_lineNumber = 1;
    m_position = 1;
    m_iter = 0;

    if (m_data) {
        munmap(m_data, m_file_contents.size());
        m_data = nullptr;
    }

    m_data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, m_fd, 0);

    m_file_contents = std::string_view(static_cast<const char *>(m_data), size);
    m_slice = std::string_view(m_file_contents);

    close(m_fd);
    m_fd = -1;

    return m_file_contents.size();
}

void lang::LexicalAnalyzer::closeFile()
{
    if (m_data) {
        munmap(m_data, m_file_contents.size());
        m_data = nullptr;
    }
    if (m_fd != -1) {
        close(m_fd);
        m_fd = -1;
    }
}

lang::Token lang::LexicalAnalyzer::next()
{
    if (m_iter >= m_file_contents.size())
        return makeToken(TokenType::END_OF_FILE, "");

    std::uint64_t start_iter = m_iter;

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

    m_slice = std::string_view(m_file_contents).substr(m_iter);

    if (m_iter >= m_file_contents.size())
        return makeToken(TokenType::END_OF_FILE, "");

    lang::Token token = runRegEx();
    if (token.type != lang::TokenType::UNKNOWN) [[likely]] {
        if (token.type == lang::TokenType::ID) {
            lang::Token keywordToken = checkKeywords(token);
            if (keywordToken.type != lang::TokenType::UNKNOWN) {
                return keywordToken;
            }
        }
        return token;
    } else {
        lang::Token operatorToken = checkOperators();
        if (operatorToken.type != lang::TokenType::UNKNOWN) {
            return operatorToken;
        }
    }

    lang::Token unknownToken = makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
    m_iter++;
    m_position++;
    return unknownToken;
}

float lang::LexicalAnalyzer::getProgress() const
{
    return static_cast<float>(m_iter) / static_cast<float>(m_file_contents.size());
}

std::uint32_t lang::LexicalAnalyzer::MatchBlockComment(std::string_view s)
{
    if (auto m = REGEX_BLOCK_COMMENT(s))
        return m.size();
    return 0;
}

std::uint32_t lang::LexicalAnalyzer::MatchInlineComment(std::string_view s)
{
    if (auto m = REGEX_INLINE_COMMENT(s))
        return m.size();
    return 0;
}

std::uint32_t lang::LexicalAnalyzer::MatchFloatNum(std::string_view s)
{
    if (auto m = REGEX_FLOAT_NUM(s))
        return m.size();
    return 0;
}

std::uint32_t lang::LexicalAnalyzer::MatchIntNum(std::string_view s)
{
    if (auto m = REGEX_INT_NUM(s))
        return m.size();
    return 0;
}

std::uint32_t lang::LexicalAnalyzer::MatchID(std::string_view s)
{
    if (auto m = REGEX_ID(s))
        return m.size();
    return 0;
}

lang::Token lang::LexicalAnalyzer::runRegEx()
{
    std::uint32_t best = 0;
    lang::TokenType best_kind{};

    for (auto &r : rules) {
        std::uint32_t n = r.match(m_slice);
        if (n > best) {
            best = n;
            best_kind = r.type;
        }
    }

    if (best > 0) {
        for (std::uint32_t i = 0; i < best; i++) {
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
        lang::Token token = makeToken(best_kind, std::string(m_slice.substr(0, best)));
        m_slice.remove_prefix(best);
        return token;
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}

const std::unordered_map<std::string, lang::TokenType> lang::LexicalAnalyzer::m_Keywords{
    { "if", TokenType::IF },         { "then", TokenType::THEN },         { "else", TokenType::ELSE },       { "while", TokenType::WHILE },
    { "class", TokenType::CLASS },   { "integer", TokenType::INTEGER },   { "float", TokenType::FLOAT },     { "do", TokenType::DO },
    { "end", TokenType::END },       { "public", TokenType::PUBLIC },     { "private", TokenType::PRIVATE }, { "or", TokenType::OR },
    { "and", TokenType::AND },       { "not", TokenType::NOT },           { "read", TokenType::READ },       { "write", TokenType::WRITE },
    { "return", TokenType::RETURN }, { "inherits", TokenType::INHERITS }, { "local", TokenType::LOCAL },     { "void", TokenType::VOID },
    { "main", TokenType::MAIN }
};

lang::Token lang::LexicalAnalyzer::checkKeywords(lang::Token &token)
{

    auto find = m_Keywords.find(token.lexeme);
    if (find != m_Keywords.end()) {
        token.type = find->second;
        return token;
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}

lang::Token lang::LexicalAnalyzer::checkOperators()
{
    for (const auto &op : m_Operators) {
        if (m_slice.starts_with(op.first)) {
            lang::Token token = makeToken(op.second, std::string(op.first));
            m_iter += op.first.size();
            m_slice.remove_prefix(op.first.size());
            m_position += op.first.size();
            return token;
        }
    }

    return makeToken(lang::TokenType::UNKNOWN, std::string(1, m_file_contents[m_iter]));
}
